Pieces 源码阅读系列 3 
---------------------------------

Torrent 类分析
===================

将编码器和解码器分析完毕后，来总体看一下 ``Torrent`` 类的初始化，其代码如下：

.. code-block:: python

    class Torrent:
        """
        Represent the torrent meta-data that is kept within a .torrent file. It is
        basically just a wrapper around the bencoded data with utility functions.

        This class does not contain any session state as part of the download.
        """
        def __init__(self, filename):
            self.filename = filename
            self.files = []

            with open(self.filename, 'rb') as f:
                meta_info = f.read()
                self.meta_info = bencoding.Decoder(meta_info).decode()
                info = bencoding.Encoder(self.meta_info[b'info']).encode()
                self.info_hash = sha1(info).digest()
                self._identify_files()

初始化的时候，会读取传入的种子文件名， ``meta_info`` 即为种子文件内容， 然后对其进行解码\
得出未编码前的文本，我们能从里面得到有用的信息。以代码给的种子 \
ubuntu-19.04-desktop-amd64.iso.torrent 为例，解码后的结果类似于如下文本，有大部分二进制\
数据被我省略了：

::

    OrderedDict(
        [
            (b'announce', b'http://torrent.ubuntu.com:6969/announce'),
            (b'announce-list',
                [   
                    [b'http://torrent.ubuntu.com:6969/announce'],
                    [b'http://ipv6.torrent.ubuntu.com:6969/announce']
                ]
            ),
            (b'comment', b'Ubuntu CD releases.ubuntu.com'),
            (b'creation date', 1555564125),
            (b'info',
                OrderedDict(
                    [
                        (b'length', 2097152000),
                        (b'name', b'ubuntu-19.04-desktop-amd64.iso'),
                        (b'piece length', 524288),
                        (b'pieces',
                                b'+\xb5!\xe5\xfbO\xe13s\xd7=\xd4\xd5\xeb\xf1\x04'
                                b'\x11\xbc4[\x82\xc90q\n\xad\xc8\xe1'
                                ...
                        )
                    ]
                )
            )
        ]
    )

上述的数据即为 ``self.meta_info`` 的值。然后对整个字典中的 ``info`` 字段求 Hash 值，也就是\
获取信息字典的 SHA1 Hash 值。需要注意的是，并不是对解码后的 info 字典求 Hash 值，而是对它代\
表的二进制数据进行 Hash 运算。所以会在获取到字典后，先进行一步编码操作，然后在对编码后的数据\
获取 SHA1 值。

然后来分析 ``_identify_files`` 函数。其代码如下：

.. code-block:: python

    def _identify_files(self):
        """
        Identifies the files included in this torrent
        """
        if self.multi_file:
            # TODO Add support for multi-file torrents
            raise RuntimeError('Multi-file torrents is not supported!')
        self.files.append(
            TorrentFile(
                self.meta_info[b'info'][b'name'].decode('utf-8'),
                self.meta_info[b'info'][b'length']))

首先判断 ``self.multi_file`` 是否为真，其代码如下：

.. code-block:: python

    @property
    def multi_file(self) -> bool:
        """
        Does this torrent contain multiple files?
        """
        # If the info dict contains a files element then it is a multi-file
        return b'files' in self.meta_info[b'info']

它的功能很简单，就是用来判断这个种子是不是包含了多个文件，判断 ``files`` 字段是不是在信息\
字典中。为真就是多文件的种子，否则就是单文件种子。而我们选择的 Ubuntu 系统种子显然就是一个\
单文件种子，就一个 iso 系统映像文件。

然后从 info 字典中拿出来文件的 ``name`` 和 ``length`` (单位:字节)，获取完成后，传到 \
``TorrentFile`` 中，它是 ``namedtuple`` 具名元组，一个具名元组，需要两个参数，一个是\
类名，另一个是类的各个字段名，在代码中，类名就是 ``TorrentFile`` ， 字段名就是 \
``name`` 和 ``length``。

.. code-block:: python

    TorrentFile = namedtuple('TorrentFile', ['name', 'length'])

最后将构造的 ``TorrentFile`` 具名元组添加到 ``self.files`` list 中。

到这里，把 ``Torrent`` 类也分析完毕，接着返回上一层：

``client = TorrentClient(Torrent(args.torrent))`` 接下来需要进入 ``TorrentClient`` \
类进行分析了。

TorrentClient
====================

首先看类的初始化过程，其代码如下：

.. code-block:: python

    class TorrentClient:

        def __init__(self, torrent):
            self.tracker = Tracker(torrent)
            # The list of potential peers is the work queue, consumed by the
            # PeerConnections
            self.available_peers = Queue()
            # The list of peers is the list of workers that *might* be connected
            # to a peer. Else they are waiting to consume new remote peers from
            # the `available_peers` queue. These are our workers!
            self.peers = []
            # The piece manager implements the strategy on which pieces to
            # request, as well as the logic to persist received pieces to disk.
            self.piece_manager = PieceManager(torrent)
            self.abort = False

初始化传入的参数是 Torrent 类处理的结果，然后用 ``self.tracker`` 保存 Tracker 类处理\
的结果； ``self.available_peers`` 创建了一个队列； ``self.peers`` 创建一个空的列表； \
``self.piece_manager`` 保存了 ``PieceManager`` 处理的结果； ``self.abort`` 初始设置\
为 ``False``。

Tracker
===========================

按照这个过程，先进入 ``Tracker`` 类进行分析了。其初始化代码如下：

.. code-block:: python 

    class Tracker:
        """
        Represents the connection to a tracker for a given Torrent that is either
        under download or seeding state.
        """

        def __init__(self, torrent):
            self.torrent = torrent
            self.peer_id = _calculate_peer_id()
            self.http_client = aiohttp.ClientSession()

这里的初始化参数 ``torrent`` ，仍然是 ``Torrent`` 类处理种子文件的结果，然后用 \
``self.torrent`` 保存它， 使用 ``self.peer_id`` 保存 ``_calculate_peer_id`` \
函数结果， 使用 ``self.http_client`` 保存 ``aiohttp.ClientSession()`` ，我对\
这个模块还不太熟悉，后面再找时间学习一下。

接着来看 ``_calculate_peer_id`` 函数，其代码如下：

.. code-block:: python

    def _calculate_peer_id():
        """
        Calculate and return a unique Peer ID.

        The `peer id` is a 20 byte long identifier. This implementation use the
        Azureus style `-PC1000-<random-characters>`.

        Read more:
            https://wiki.theory.org/BitTorrentSpecification#peer_id
        """
        return '-PC0001-' + ''.join(
            [str(random.randint(0, 9)) for _ in range(12)])

用来生成一个随机的 peer_id。




