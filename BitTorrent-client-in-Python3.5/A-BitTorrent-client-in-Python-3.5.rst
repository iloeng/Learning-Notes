基于 Python 3.5 的 BitTorrent 客户端
-------------------------------------

- 原文标题: A BitTorrent client in Python 3.5
- 原文发布于: 周三, 2016-08-24
- 原文来自于 https://markuseliasson.se/article/bittorrent-in-python/
- 想法来自于 build-your-own-x_

.. _build-your-own-x: https://github.com/danistefanovic/build-your-own-x

Python 3.5 开始支持异步 IO, 这似乎是实现 BitTorrent 客户端的完美\
选择。本文将指导您了解 BitTorrent 协议的细节，同时展示如何使用它实\
现一个小的客户端。

当 Python 3.5 与新的模块 ``asyncio`` 一起发布时，我很好奇地尝试了\
一下。最近，我决定使用 ``asyncio`` 实现一个简单的 BitTorrent 客户\
端--我一直对点对点(P2P, peer-to-peer)协议感兴趣，它似乎是一个完美\
的选择。

这个项目名为 Pieces, 所有的源代码都可以在 GitHub_ 上获得，并在 Apache 2 \
许可下发布。你可以随意地从中学习、窃取、改进、嘲笑或忽略它。

我之前发布了一个 `关于Python的异步模块的简短介绍`_ 。如果这是您第一\
次查看 ``asyncio`` ，那么首先通读一下它可能是一个好主意。

.. _GitHub: https://github.com/eliasson/pieces
.. _`关于Python的异步模块的简短介绍`: https://markuseliasson.se/article/introduction-to-asyncio

BitTorrent 介绍
==================================

BitTorrent 从 2001 年就开始存在了，当时 `Bram Cohen`_ 编写了该协议\
的第一个版本。最大的突破是像海盗湾这样的网站让下载盗版内容使 BitTorrent \
变得流行起来。流媒体网站，如 Netflix，可能会导致使用 BitTorrent 下载电\
影的人数减少。但是 BitTorrent 仍然在一些不同的、合法的解决方案中使用，\
在这些方案中，大文件的分发是很重要的。

- Facebook_ 利用它在其庞大的数据中心内发布更新
- `Amazon S3`_ 实施它用于下载静态文件
- 传统的下载仍然用于更大的文件，比如 `Linux发行版`_

.. _`Bram Cohen`: https://en.wikipedia.org/wiki/Bram_Cohen
.. _Facebook: https://torrentfreak.com/facebook-uses-bittorrent-and-they-love-it-100625/
.. _`Amazon S3`: http://docs.aws.amazon.com/AmazonS3/latest/dev/S3Torrent.html
.. _`Linux发行版`: http://www.ubuntu.com/download/alternative-downloads

BitTorrent 是一种点对点协议，在该协议中，对等点加入到一群其他对等点之\
间交换数据。每个对等点同时连接多个对等点，从而同时向多个对等点下载或上\
传。与从中央服务器下载文件相比，对于带宽的限制而言，这非常好。它还可以\
很好地保持文件的可用性，因为它不依赖于单个文件源的在线。

.torrent 文件规定了一个给定文件有多少块，如何在对等点之间交换，以及如\
何这些块的数据完整性可以被客户确认。

在实现它的过程中，最好阅读一下，或者打开另一个 `非正式的 BitTorrent 规范`_ 的\
窗口。毫无疑问，这是关于 BitTorrent 协议的最好的信息来源。官方的规范是模糊的，\
缺乏某些细节，所以非官方的是你想要研究的。

.. _`非正式的 BitTorrent 规范`: https://wiki.theory.org/BitTorrentSpecification

解析 .torrent 文件
========================

客户端需要做的第一件事是找出它应该从哪里下载什么文件。这些信息被存储在 ``.torrent`` \
文件中，也就是元信息( ``meta-info`` )。在元信息中存储了许多属性，我们需要这些\
属性来成功地实现客户端。

例如：
    - 下载的文件的名称
    - 下载文件的大小
    - 要连接到 Tracker 的 URL

所有的这些属性都以二进制格式存储，称为 *Bencoding* 。

Bencoding 支持四种不同的数据类型， *字典* ， *列表* ，*整数* 和 *字符串* -- \
它很容易转换成 Python 的 *object literals* 或 *JSON* 。

以下是 `Haskell Library`_ 提供的 `Augmented Backus-Naur Form`_ 中描述的 bencoding。

.. _`Haskell Library`: https://hackage.haskell.org/package/bencoding-0.4.3.0/docs/Data-BEncode.html
.. _`Augmented Backus-Naur Form`: https://en.wikipedia.org/wiki/Augmented_Backus%E2%80%93Naur_Form

.. code-block:: haskell

    <BE>    ::= <DICT> | <LIST> | <INT> | <STR>

    <DICT>  ::= "d" 1 * (<STR> <BE>) "e"
    <LIST>  ::= "l" 1 * <BE>         "e"
    <INT>   ::= "i"     <SNUM>       "e"
    <STR>   ::= <NUM> ":" n * <CHAR>; where n equals the <NUM>

    <SNUM>  ::= "-" <NUM> / <NUM>
    <NUM>   ::= 1 * <DIGIT>
    <CHAR>  ::= %
    <DIGIT> ::= "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"

在 ``pieces`` 项目中 Bencoding 编码和编码数据的解码都是在 ``pieces.bencoding`` \
模块中实现的( 源代码_ )。

.. _源代码: https://github.com/eliasson/pieces/blob/master/pieces/bencoding.py

下面是一些使用该模块将 Bencoded 编码数据解码为 Python 表示的示例。

.. code-block:: python

    >>> from pieces.bencoding import Decoder

    # An integer value starts with an 'i' followed by a series of
    # digits until terminated with a 'e'.
    >>> Decoder(b'i123e').decode()
    123

    # A string value, starts by defining the number of characters
    # contained in the string, followed by the actual string.
    # Notice that the string returned is a binary string, not unicode.
    >>> Decoder(b'12:Middle Earth').decode()
    b'Middle Earth'

    # A list starts with a 'l' followed by any number of objects, until
    # terminated with an 'e'.
    # As in Python, a list may contain any type of object.
    >>> Decoder(b'l4:spam4:eggsi123ee').decode()
    [b'spam', b'eggs', 123]

    # A dict starts with a 'd' and is terminated with a 'e'. objects
    # in between those characters must be pairs of string + object.
    # The order is significant in a dict, thus OrderedDict (from
    # Python 3.1) is used.
    >>> Decoder(b'd3:cow3:moo4:spam4:eggse').decode()
    OrderedDict([(b'cow', b'moo'), (b'spam', b'eggs')])

同样，Python 对象结构也可以使用相同的模块编码为 Bencoded 编码的字节字符串。

.. code-block:: python

    >>> from collections import OrderedDict
    >>> from pieces.bencoding import Encoder

    >>> Encoder(123).encode()
    b'i123e'

    >>> Encoder('Middle Earth').encode()
    b'12:Middle Earth'

    >>> Encoder(['spam', 'eggs', 123]).encode()
    bytearray(b'l4:spam4:eggsi123ee')

    >>> d = OrderedDict()
    >>> d['cow'] = 'moo'
    >>> d['spam'] = 'eggs'
    >>> Encoder(d).encode()
    bytearray(b'd3:cow3:moo4:spam4:eggse')

这些示例也可以在 单元测试_ 中找到。

.. _单元测试: https://github.com/eliasson/pieces/blob/master/tests/test_bendoding.py

解析器的实现非常简单，这里没有使用异步，甚至没有从磁盘读取 ``.torrent`` 文件。

使用这个来自于 ``pieces.bencoding`` 解析器， 让我们打开流行的 Linux 发行版 Ubuntu \
的 ``.torrent`` 文件:

.. code-block:: python

    >>> with open('tests/data/ubuntu-16.04-desktop-amd64.iso.torrent', 'rb') as f:
    ...     meta_info = f.read()
    ...     torrent = Decoder(meta_info).decode()
    ...
    >>> torrent
    OrderedDict([(b'announce', b'http://torrent.ubuntu.com:6969/announce'), (b'announce-list', [[b'http://torrent.ubuntu.com:6969/announce'], [b'http://ipv6.torrent.ubuntu.com:6969/announce']
    ]), (b'comment', b'Ubuntu CD releases.ubuntu.com'), (b'creation date', 1461232732), (b'info', OrderedDict([(b'length', 1485881344), (b'name', b'ubuntu-16.04-desktop-amd64.iso'), (b'piece
    length', 524288), (b'pieces', b'\x1at\xfc\x84\xc8\xfaV\xeb\x12\x1c\xc5\xa4\x1c?\xf0\x96\x07P\x87\xb8\xb2\xa5G1\xc8L\x18\x81\x9bc\x81\xfc8*\x9d\xf4k\xe6\xdb6\xa3\x0b\x8d\xbe\xe3L\xfd\xfd4\...')]))])

在这里，您可以看到一些元数据，比如目标文件的名称 (ubuntu-16.04-desktop-amd64.iso) \
和总字节大小 (1485881344)。

注意 OrderedDict 中使用的键是二进制字符串。Bencoding 是一个二进制协议，使用 UTF-8 \
字符串作为键不能工作!

包装类 ``piece.torrent.Torrent`` 揭示这些属性是通过抽象二进制字符串和其他细节来\
实现的，这些细节远离客户端的其他部分。这个类只实现了在 pieces 客户端中使用的属性。

我将不详细说明哪些属性是可用的，而在本文的其余部分将引用 ``.torrent`` / *meta-info* \
中使用的属性。

连接 Tracker
=============================

现在我们可以解码 ``.torrent`` 文件，并且我们有了这些数据的 Python 表示，我们需要\
获得一个要连接的对等点列表。这就是追踪器的作用。一个跟踪器是一个中央服务器，为一个\
给定的种子记录可用的对等点。一个跟踪器不包含任何的torrent数据，只可以连接到的对等\
点和他们的统计数据。

发起请求
*********

元信息中的 announce 属性是使用以下 URL 参数连接到 Tracker 的 HTTP URL：

============  =====
参数           描述
============  =====
info_hash     在 ``.torrent`` 中找到的信息字典的 SHA1 哈希
peer_id       为客户端生成的唯一 ID
uploaded      上传的总字节数
downloaded    下载的总字节数
left          客户端要下载的剩余字节数
port          客户端侦听的 TCP 端口
compact       客户端是否接受一个压缩的对等点列表
============  =====

``peer_id`` 需要精确为20个字节，在如何生成这个ID上有两种主要的约定。Pieces 遵循 \
`Azureus 风格`_ 的惯例产生 ``peer id`` 如下:

.. _`Azureus 风格`: https://wiki.theory.org/BitTorrentSpecification#peer_id

.. code-block:: python

    >>> import random
    # -<2 character id><4 digit version number>-<random numbers>
    >>> '-PC0001-' + ''.join([str(random.randint(0, 9)) for _ in range(12)])
    '-PC0001-478269329936'

使用 httpie_ ，Tracker 的请求可以像这样:

.. _httpie: https://github.com/jkbrzt/httpie

.. code-block::

    http GET "http://torrent.ubuntu.com:6969/announce?info_hash=%90%28%9F%D3M%FC%1C%F8%F3%16%A2h%AD%D85L%853DX&peer_id=-PC0001-706887310628&uploaded=0&downloaded=0&left=699400192&port=6889&compact=1"
    HTTP/1.0 200 OK
    Content-Length: 363
    Content-Type: text/plain
    Pragma: no-cache

    d8:completei3651e10:incompletei385e8:intervali1800e5:peers300:£¬%ËÌyOkÝ.ê@_<K+Ô\Ý Ámb^TnÈÕ^AËO*ÈÕ1*ÈÕ>¥³ÈÕBä)ðþ¸ÐÞ¦Ô/ãÈÕÈuÉæÈÕ
    ...

