使用Go从头开始构建BitTorrent客户端
=========================================

这篇文章来自 这里_。

.. _这里: https://blog.jse.li/posts/torrent

摘要：从访问海盗湾，到无中生有转换成 mp3 文件，这两者之间的完整路径是什么？\
在这篇文章中，我们将实现足够的 BitTorrent 协议来下载 debian。查看 源代码_ 或\
跳至 最后_。

.. _源代码: https://github.com/veggiedefender/torrent-client
.. _最后: #放在一起

这篇文章有 俄语翻译_。

.. _俄语翻译: https://4gophers.ru/articles/bittorrent/

BitTorrent 是一个用于通过 Internet 下载和分发文件的协议。与传统的客户\
端/服务器关系不同，在传统的客户端/服务器关系中，下载器需要连接到中央服务器\
（例如：在Netflix上观看电影或加载您正在阅读的网页），而在BitTorrent网络中\
的参与者，称为peers，从相互之间下载文件碎片-这就是使它成为对等协议的原因。\
我们将研究其工作原理，并建立我们自己的客户端，使该客户端可以找到对等端并在\
它们之间交换数据。

.. image:: img/client-server-p2p.png

在过去的20年中，该协议进行了有机发展，各种人和组织为诸如加密，私人种子和\
寻找 peers 的新方法等功能添加了扩展。 我们将从2001年的 原始规范_ 开始实\
现，以保持这个周末级别的项目。

.. _原始规范: https://www.bittorrent.org/beps/bep_0003.html

我将会使用一个 `Debian ISO`_ 文件作为我的实验文件，因为它的大小有350MB，\
不是特别大。作为一个流行的Linux发行版，将有许多快速且合作的 peers 可供我\
们连接。而且，我们将避免与下载盗版内容相关的法律和道德问题。

.. _`Debian ISO`: https://cdimage.debian.org/debian-cd/current/amd64/bt-cd/#indexlist


寻找 Peers
---------------

这是一个问题：我们想使用 BitTorrent 下载文件，但这是点对点协议，我们不知\
道在哪里可以找到要下载文件的点。这就像搬到新城市并尝试结交朋友一样，也许我\
们会去当地的酒吧或聚会小组！ 像这样的集中位置是 Tracker 背后的重要思想，\
Tracker 是将 peer 介绍给彼此的中央服务器。它们只是运行在HTTP*上的网络服\
务器，您可以在 http://bttracker.debian.org:6969/ 上找到Debian。

.. image:: img/trackers.png

当然，如果这些中央服务器便于在 Peers 之间交换非法内容，则很容易遭到联邦政府\
的突袭。您可能还记得阅读过有关TorrentSpy，Popcorn Time和KickassTorrents\
等 Tracker 的信息，这些 Trackers 被抓住并关闭了。新方法通过使 Peers 发现成\
为分布式过程来消除中间人。我们不会实现它们，但是如果您有兴趣，可以研究的一些\
术语是DHT，PEX和magnet links。

解析.torrent文件
---------------------

一个 .torrent 文件描述了种子文件的内容以及用于连接到 Tracker 的信息。这是我们启\
动种子下载过程所需要的。 Debian 的 .torrent 文件如下所示
::

    d8:announce41:http://bttracker.debian.org:6969/announce7:comment35:"Debian CD from cdimage.debian.org"13:creation datei1573903810e9:httpseedsl145:https://cdimage.debian.org/cdimage/release/10.2.0//srv/cdbuilder.debian.org/dst/deb-cd/weekly-builds/amd64/iso-cd/debian-10.2.0-amd64-netinst.iso145:https://cdimage.debian.org/cdimage/archive/10.2.0//srv/cdbuilder.debian.org/dst/deb-cd/weekly-builds/amd64/iso-cd/debian-10.2.0-amd64-netinst.isoe4:infod6:lengthi351272960e4:name31:debian-10.2.0-amd64-netinst.iso12:piece lengthi262144e6:pieces26800:�����PS�^�� (binary blob of the hashes of each piece)ee

乱码部分是以 **Bencode** （发音为 *bee-encode*）的格式进行编码的，我们需要对其进行解码。

Bencode 可以编码与 JSON 大致相同的结构类型-字符串，整数，列表和字典。\
Bencoded 数据不像 JSON 那样易于人读/写，但是它可以有效地处理二进制数据，\
并且很容易从流中进行解析。字符串带有长度前缀，看起来像 ``4：spam``。整数位于\
*start* 和 *end* 标记之间，因此 ``7`` 将编码为 ``i7e``。列表和词典的工作方式类似：\
``l4：spami7ee`` 表示 ``['spam'，7]``，而 ``d4：spami7ee`` 表示 ``{spam：7}``。

以更漂亮的格式，我们的.torrent文件如下所示：
::

    d
        8:announce
          41\:http\://bttracker.debian.org:6969/announce
        7:comment
          35:"Debian CD from cdimage.debian.org"
        13:creation date
          i1573903810e
        4:info
          d
            6:length
              i351272960e
            4:name
              31:debian-10.2.0-amd64-netinst.iso
            12:piece length
              i262144e
            6:pieces
              26800:�����PS�^�� (binary blob of the hashes of each piece)
          e
    e

在此文件中，我们可以发现 Tracker 的URL，创建日期（以Unix时间戳），文\
件的名称和大小以及包含每个片段SHA-1哈希值的大的二进制blob，这些均等于\
我们要下载的文件的大小部分。种子的确切大小因种子而异，但它们通常在256\
KB至1MB之间。 这意味着一个大文件可能由数千个文件组成。 我们将从同伴\
那里下载这些片段，将它们与种子文件中的哈希值进行对照，将它们组装在一\
起，我们就获得了一个文件！           

.. image:: img/pieces.png

这种机制使我们能够在进行过程中验证每个零件的完整性。它使 BitTorrent \
能够抵抗意外损坏或故意 **torrent poisoning**。 除非攻击者能够通过预\
映像攻击破坏SHA-1，否则我们将完全获得我们所要求的内容。

编写 Bencode 解析器确实很有趣，但是解析器并不是我们今天关注的重点。\
但是我发现 Fredrik Lundh 的 `50行解析器`_ 特别具有启发性。对于这个项目，\
我使用了 https://github.com/jackpal/bencode-go
::

    import (
        "github.com/jackpal/bencode-go"
        "io"
    )

    type bencodeInfo struct {
        Pieces      string `bencode:"pieces"`
        PieceLength int    `bencode:"piece length"`
        Length      int    `bencode:"length"`
        Name        string `bencode:"name"`
    }

    type bencodeTorrent struct {
        Announce string      `bencode:"announce"`
        Info     bencodeInfo `bencode:"info"`
    }

    // Open parses a torrent file
    func Open(r io.Reader) (*bencodeTorrent, error) {
        bto := bencodeTorrent{}
        err := bencode.Unmarshal(r, &bto)
        if err != nil {
            return nil, err
        }
        return &bto, nil
    }

.. _`50行解析器`: https://effbot.org/zone/bencode.htm

因为我喜欢保持结构相对平坦，并且我希望将应用程序结构与序列化结构分开，\
所以我导出了另一个更平坦的结构 ``TorrentFile`` ，并编写了一些辅助函\
数以在两者之间进行转换。

值得注意的是，我将 ``片段`` （以前是字符串）分割为一片哈希（每个 \
``[20] byte`` ），以便以后可以轻松访问各个哈希。我还计算了整个 bencoded \
信息字典的 SHA-1 哈希（包含名称，大小和片段哈希的那一部分）。我们将其称\
为 **infohash** ，当我们与跟踪者和同伴交谈时，它是唯一地标识文件。 稍后\
再详细介绍。

.. image:: img/info-hash.png

::

    type TorrentFile struct {
        Announce    string
        InfoHash    [20]byte
        PieceHashes [][20]byte
        PieceLength int
        Length      int
        Name        string
    }
    func (bto bencodeTorrent) toTorrentFile() (TorrentFile, error) {
        // …
    }

从 Tracker 中检索 Peers
-------------------------

现在我们有了关于文件及其 Tracker 的信息，让我们与 Tracker 对话，\
宣布我们作为对等方(Peer)的存在，并检索其他对等方(Peers)的列表。我\
们只需要使用几个查询参数对 .torrent 文件中提供的 announce URL 发\
出GET请求：
::

    func (t *TorrentFile) buildTrackerURL(peerID [20]byte, port uint16) (string, error) {
        base, err := url.Parse(t.Announce)
        if err != nil {
            return "", err
        }
        params := url.Values{
            "info_hash":  []string{string(t.InfoHash[:])},
            "peer_id":    []string{string(peerID[:])},
            "port":       []string{strconv.Itoa(int(Port))},
            "uploaded":   []string{"0"},
            "downloaded": []string{"0"},
            "compact":    []string{"1"},
            "left":       []string{strconv.Itoa(t.Length)},
        }
        base.RawQuery = params.Encode()
        return base.String(), nil
    }

重要的是：

**info_hash** ：标识要下载的文件。这是我们之前根据 bencoded ``info`` \
dict 计算出的 infohash。Tracker 将使用它来确定向我们显示哪些 Peers。

**peer_id** : 一个20字节的名称，用于向 Tracker 和对等者 (peers) 标识自\
己。我们将为此生成 20 个随机字节。真实的 BitTorrent 客户端的ID类似\
于 ``-TR2940-k8hj0wgej6ch`` ， 它标识客户端软件和版本， 在本例中，\
TR2940 代表传输客户端 2.94。

.. image:: img/info-hash-peer-id.png

分析 Tracker 响应
--------------------------------

我们得到了一个编码后的响应：
::

    d
      8:interval
        i900e
      5:peers
        252:(another long binary blob)
    e

``Interval`` 告诉我们应该多久重新连接一次 Tracker 以刷新我们的对等\
列表。值是 900 意味着我们应该每 15 分钟（900秒）重新连接一次。

``Peers`` 是另一个包含每个 peer 的 IP 地址的长二进制 blob。它是由\
6个字节组组成的。每组中的前四个字节代表对等方的 IP 地址，每个字节代\
表 IP 中的一个数字。最后两个字节表示端口，表示为大端 ``uint16``。\
**Big-endian** 或 **network order** 意味着我们可以将一组字节从左\
到右压缩成整数。例如，字节 ``0x1A`` 、 ``0xE1`` 变成 ``0x1AE1`` \
或以十进制表示为 6881。

.. image:: img/address.png

::

    // Peer encodes connection information for a peer
    type Peer struct {
        IP   net.IP
        Port uint16
    }

    // Unmarshal parses peer IP addresses and ports from a buffer
    func Unmarshal(peersBin []byte) ([]Peer, error) {
        const peerSize = 6 // 4 for IP, 2 for port
        numPeers := len(peersBin) / peerSize
        if len(peersBin)%peerSize != 0 {
            err := fmt.Errorf("Received malformed peers")
            return nil, err
        }
        peers := make([]Peer, numPeers)
        for i := 0; i < numPeers; i++ {
            offset := i * peerSize
            peers[i].IP = net.IP(peersBin[offset : offset+4])
            peers[i].Port = binary.BigEndian.Uint16(peersBin[offset+4 : offset+6])
        }
        return peers, nil
    }

从 Peers 下载
----------------------------------------

现在我们有了一个 Peers 列表，是时候与他们连接并开始下载片段了！我们可以将过程\
分为几个步骤。 对于每个 Peer，我们希望：

1. 与 Peer 启动一个 TCP 连接。就像打个电话一样。
2. 完成双向 BitTorrent **握手** 。 “你好？” “你好。”
3. 交换消息以下载片段。 “请给我 ＃231 片段。”

启动一个 TCP 连接
******************************************

::

    conn, err := net.DialTimeout("tcp", peer.String(), 3*time.Second)
    if err != nil {
        return nil, err
    }

我设置了超时时间，这样我就不会在不让我建立联系的 Peers 身上浪费太多时\
间。 在大多数情况下，这是一个非常标准的TCP连接。

完成握手
******************************************

我们刚刚建立了与对等方 (Peers) 的连接，但是我们想握手以验证我们对等方的假设

* 可以使用 BitTorrent 协议进行通讯
* 能够理解并回复我们的信息
* 拥有我们想要的文件，或者至少知道我们在说什么

.. image:: img/handshake.png

我的父亲告诉我，良好的握手秘诀是牢固握力和目光接触。而良好的 BitTorrent 握\
手秘诀在于它由五个部分组成：

1. 协议标识符的长度，始终为19（十六进制为 0x13 ）
2. 协议标识符，称为 **pstr** ，始终为 ``BitTorrent Protocol``
3. 八个 ``保留字节`` ，都设置为0。我们会将其中一些翻转为1，以表示我们支持某\
   些 `extensions`_。 但是我们没有，所以我们将它们保持为0。
4. 我们之前计算出的信息哈希，用于标识我们想要的文件
5. **Peer ID** 我们用来识别自己

.. _`extensions`: http://www.bittorrent.org/beps/bep_0010.html

放在一起，握手字符串可能如下所示：
::

    \x13BitTorrent protocol\x00\x00\x00\x00\x00\x00\x00\x00\x86\xd4\xc8\x00\x24\xa4\x69\xbe\x4c\x50\xbc\x5a\x10\x2c\xf7\x17\x80\x31\x00\x74-TR2940-k8hj0wgej6ch

向我们的 Peer 发送一次握手后，我们应该以相同的格式收到一次握手。返回的信息哈希\
应该与我们发送的信息哈希匹配，以便我们知道我们在谈论同一文件。 如果一切都按计划\
进行，那么就很好了。如果没有，我们可以切断连接，因为出了点问题。“Hello?” “这是\
谁？ 你想要什么？” “Okay, wow, wrong number."

在我们的代码中，让我们构造一个表示握手的结构，并编写一些用于序列化和读取它们的方法：
::

    // A Handshake is a special message that a peer uses to identify itself
    type Handshake struct {
        Pstr     string
        InfoHash [20]byte
        PeerID   [20]byte
    }

    // Serialize serializes the handshake to a buffer
    func (h *Handshake) Serialize() []byte {
        buf := make([]byte, len(h.Pstr)+49)
        buf[0] = byte(len(h.Pstr))
        curr := 1
        curr += copy(buf[curr:], h.Pstr)
        curr += copy(buf[curr:], make([]byte, 8)) // 8 reserved bytes
        curr += copy(buf[curr:], h.InfoHash[:])
        curr += copy(buf[curr:], h.PeerID[:])
        return buf
    }

    // Read parses a handshake from a stream
    func Read(r io.Reader) (*Handshake, error) {
        // Do Serialize(), but backwards
        // ...
    }

发送和接受消息
******************************************

完成初始握手后，我们就可以发送和接收消息。 好吧，还不完全，如果对方没有准备好\
接受消息，我们将无法发送任何消息，除非对方告诉我们他们已经准备好了。 在这种状\
态下，我们被其他 Peer 阻塞住了。 他们会向我们发送一条取消锁定的消息，来告知我\
们我们可以开始向他们询问数据。默认情况下，我们假设我们一直处于阻塞状态，除非\
另行证明。

一旦我们变成非阻塞状态，我们就可以开始发送碎片请求，他们可以向我们发送包含碎片\
的消息。

.. image:: img/choke.png

解释信息
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

一条信息具有长度，**ID** 和 **Payload** 。 在电线上，它看起来像：

.. image:: img/message.png

一条消息以长度指示符开头，该指示符告诉我们该消息将有多少字节的长度。这是一个32位\
整数，表示它是由四个按大端字节序排列的字节组成。下一个字节，即 **ID** ，告诉我\
们正在接收的消息类型，例如 ``2`` 字节表示 “interested”。最后，可选的 **Payload** \
将填充消息的剩余长度。
::

    type messageID uint8

    const (
        MsgChoke         messageID = 0
        MsgUnchoke       messageID = 1
        MsgInterested    messageID = 2
        MsgNotInterested messageID = 3
        MsgHave          messageID = 4
        MsgBitfield      messageID = 5
        MsgRequest       messageID = 6
        MsgPiece         messageID = 7
        MsgCancel        messageID = 8
    )

    // Message stores ID and payload of a message
    type Message struct {
        ID      messageID
        Payload []byte
    }

    // Serialize serializes a message into a buffer of the form
    // <length prefix><message ID><payload>
    // Interprets `nil` as a keep-alive message
    func (m *Message) Serialize() []byte {
        if m == nil {
            return make([]byte, 4)
        }
        length := uint32(len(m.Payload) + 1) // +1 for id
        buf := make([]byte, 4+length)
        binary.BigEndian.PutUint32(buf[0:4], length)
        buf[4] = byte(m.ID)
        copy(buf[5:], m.Payload)
        return buf
    }

要从数据流中读取消息，我们只需遵循消息的格式。我们读取四个字节并将其解释为 ``uint32`` \
，以获取消息的长度。然后，我们读取该字节数以获得 **ID** （第一个字节）和 **Payload** \
（其余字节）。
::

    // Read parses a message from a stream. Returns `nil` on keep-alive message
    func Read(r io.Reader) (*Message, error) {
        lengthBuf := make([]byte, 4)
        _, err := io.ReadFull(r, lengthBuf)
        if err != nil {
            return nil, err
        }
        length := binary.BigEndian.Uint32(lengthBuf)

        // keep-alive message
        if length == 0 {
            return nil, nil
        }

        messageBuf := make([]byte, length)
        _, err = io.ReadFull(r, messageBuf)
        if err != nil {
            return nil, err
        }

        m := Message{
            ID:      messageID(messageBuf[0]),
            Payload: messageBuf[1:],
        }

        return &m, nil
    }

Bitfields
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

消息中最有趣的一种类型是位域( **Bitfield** )，位域是 Peers 用来有效编码他们能够发\
送给我们哪些数据的数据结构。位域看起来像一个字节数组，要检查它们具有哪些文件片段，我\
们只需要查看设置为 1 的位的位置即可。您可以将其视为咖啡店会员卡的数字等效物。我们从\
全为 ``0`` 的空白卡开始，然后将位翻转为 ``1`` 以将其位置标记为“盖章”。

.. image:: img/bitfield.png

通过使用 *bit* 而不是 *Byte* 工作，是因为此数据结构非常紧凑。我们可以在一个字节的\
空间（ ``bool`` 的大小）中填充有关八段的信息。难点是访问值变得有些棘手。计算机可以\
寻址的最小内存单位是字节，因此要获取位，我们必须进行一些按位操作：

::

    // A Bitfield represents the pieces that a peer has
    type Bitfield []byte

    // HasPiece tells if a bitfield has a particular index set
    func (bf Bitfield) HasPiece(index int) bool {
        byteIndex := index / 8
        offset := index % 8
        return bf[byteIndex]>>(7-offset)&1 != 0
    }

    // SetPiece sets a bit in the bitfield
    func (bf Bitfield) SetPiece(index int) {
        byteIndex := index / 8
        offset := index % 8
        bf[byteIndex] |= 1 << (7 - offset)
    }

放在一起
--------------------------------

现在，我们拥有下载 torrent 所需的所有工具：我们有从跟踪器获得的对等方的列表，\
并且我们可以通过建立 TCP 连接，发起握手以及发送和接收消息来与它们进行通信。我\
们的最后一个大问题是处理与多个对等方交谈所涉及的并发性，以及在与对等方交互时管\
理对等方的状态。这些都是经典的难题。

并发管理：将通道作为队列
******************************************

在 Go 中，我们通过 `通信共享内存`_ ，并且可以将 Go 通道视为廉价的线程安全队列。

.. _`通信共享内存`: https://blog.golang.org/share-memory-by-communicating

我们将设置两个 channel 来同步我们的并发工作：一个用于在同伴之间分发工作（下载\
的作品），另一个用于收集下载的作品。当下载的片段通过结果 channel 进入时，我们\
可以将它们复制到缓冲区中以开始组装完整的文件。
::

    // Init queues for workers to retrieve work and send results
    workQueue := make(chan *pieceWork, len(t.PieceHashes))
    results := make(chan *pieceResult)
    for index, hash := range t.PieceHashes {
        length := t.calculatePieceSize(index)
        workQueue <- &pieceWork{index, hash, length}
    }

    // Start workers
    for _, peer := range t.Peers {
        go t.startDownloadWorker(peer, workQueue, results)
    }

    // Collect results into a buffer until full
    buf := make([]byte, t.Length)
    donePieces := 0
    for donePieces < len(t.PieceHashes) {
        res := <-results
        begin, end := t.calculateBoundsForPiece(res.index)
        copy(buf[begin:end], res.buf)
        donePieces++
    }
    close(workQueue)

我们将为从 Tracker 收到的每个同伴产生一个 worker goroutine。 它将与对等方连\
接并握手，然后开始从 ``workQueue`` 检索工作，并尝试下载它，然后通过结果 Channel 将\
下载的片段发送回去。

.. image:: img/download.png

::

    func (t *Torrent) startDownloadWorker(peer peers.Peer, workQueue chan *pieceWork, results chan *pieceResult) {
        c, err := client.New(peer, t.PeerID, t.InfoHash)
        if err != nil {
            log.Printf("Could not handshake with %s. Disconnecting\n", peer.IP)
            return
        }
        defer c.Conn.Close()
        log.Printf("Completed handshake with %s\n", peer.IP)

        c.SendUnchoke()
        c.SendInterested()

        for pw := range workQueue {
            if !c.Bitfield.HasPiece(pw.index) {
                workQueue <- pw // Put piece back on the queue
                continue
            }

            // Download the piece
            buf, err := attemptDownloadPiece(c, pw)
            if err != nil {
                log.Println("Exiting", err)
                workQueue <- pw // Put piece back on the queue
                return
            }

            err = checkIntegrity(pw, buf)
            if err != nil {
                log.Printf("Piece #%d failed integrity check\n", pw.index)
                workQueue <- pw // Put piece back on the queue
                continue
            }

            c.SendHave(pw.index)
            results <- &pieceResult{pw.index, buf}
        }
    }

状态管理
******************************************

我们将跟踪结构中的每个对等体，并在阅读消息时对其进行修改。其中将包含诸如从同伴那里\
下载了多少，从同伴那里请求了多少以及是否阻塞了数据。如果要进一步扩展，可以将其形式\
化为有限状态机。但是到目前为止，一个结构和一个开关已经足够了。
::

    type pieceProgress struct {
        index      int
        client     *client.Client
        buf        []byte
        downloaded int
        requested  int
        backlog    int
    }

    func (state *pieceProgress) readMessage() error {
        msg, err := state.client.Read() // this call blocks
        switch msg.ID {
        case message.MsgUnchoke:
            state.client.Choked = false
        case message.MsgChoke:
            state.client.Choked = true
        case message.MsgHave:
            index, err := message.ParseHave(msg)
            state.client.Bitfield.SetPiece(index)
        case message.MsgPiece:
            n, err := message.ParsePiece(state.index, state.buf, msg)
            state.downloaded += n
            state.backlog--
        }
        return nil
    }

是时候开始请求了！
******************************************

文件，碎片和碎片哈希不是完整的故事，我们可以通过将碎片分解成块来进一步发展。\
块是碎片的一部分，我们可以通过碎片的索引，碎片中的字节偏移量和长度来完全定义\
块。当我们从对等体请求数据时，实际上是在请求数据块。一个块通常为16KB，这意味\
着一个256KB的块实际上可能需要16个请求。

如果对等方收到大于16KB的块的请求，则应该切断该连接。但是，根据我的经验，他们\
通常非常乐意满足最大128KB的请求。在更大的块尺寸下，我的整体速度只有中等程度\
的提高，因此最好遵循规范。

流水线
******************************************

网络往返很昂贵，一个一个地请求每个块绝对会降低我们的下载性能。因此，以流水线\
方式管理我们的请求是很重要的，以便我们对一些未完成的请求保持恒定的压力。这可\
以将我们的连接吞吐量提高一个数量级。

.. image:: img/pipelining.png

传统上，BitTorrent 客户端保持五个流水线请求排队，这就是我要使用的值。我发现增\
加它可以使下载速度提高一倍。较新的客户端使用自适应队列大小来更好地适应现代网络\
的速度和条件。这绝对是一个值得调整的参数，对于将来的性能优化而言，这是一个很低\
的目标。
::

    // MaxBlockSize is the largest number of bytes a request can ask for
    const MaxBlockSize = 16384

    // MaxBacklog is the number of unfulfilled requests a client can have in its pipeline
    const MaxBacklog = 5

    func attemptDownloadPiece(c *client.Client, pw *pieceWork) ([]byte, error) {
        state := pieceProgress{
            index:  pw.index,
            client: c,
            buf:    make([]byte, pw.length),
        }

        // Setting a deadline helps get unresponsive peers unstuck.
        // 30 seconds is more than enough time to download a 262 KB piece
        c.Conn.SetDeadline(time.Now().Add(30 * time.Second))
        defer c.Conn.SetDeadline(time.Time{}) // Disable the deadline

        for state.downloaded < pw.length {
            // If unchoked, send requests until we have enough unfulfilled requests
            if !state.client.Choked {
                for state.backlog < MaxBacklog && state.requested < pw.length {
                    blockSize := MaxBlockSize
                    // Last block might be shorter than the typical block
                    if pw.length-state.requested < blockSize {
                        blockSize = pw.length - state.requested
                    }

                    err := c.SendRequest(pw.index, state.requested, blockSize)
                    if err != nil {
                        return nil, err
                    }
                    state.backlog++
                    state.requested += blockSize
                }
            }

            err := state.readMessage()
            if err != nil {
                return nil, err
            }
        }

        return state.buf, nil
    }

main.go
******************************************

这是一个简短的。 我们就到这了。

::

    package main

    import (
        "log"
        "os"

        "github.com/veggiedefender/torrent-client/torrentfile"
    )

    func main() {
        inPath := os.Args[1]
        outPath := os.Args[2]

        tf, err := torrentfile.Open(inPath)
        if err != nil {
            log.Fatal(err)
        }

        err = tf.DownloadToFile(outPath)
        if err != nil {
            log.Fatal(err)
        }
    }

这并不是全部
=========================

为简洁起见，我仅包含了一些重要的代码片段。值得注意的是，我忽略了所有粘合代码，\
解析，单元测试以及构建字符的无聊部分。如果您有兴趣，请查看我的 完整实施_ 。

.. _完整实施: https://github.com/veggiedefender/torrent-client