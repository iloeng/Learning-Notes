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

https://blog.jse.li/posts/torrent/#retrieving-peers-from-the-tracker

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

放在一起
--------------------------------
