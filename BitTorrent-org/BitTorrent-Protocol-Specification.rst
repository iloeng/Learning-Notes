BitTorrent 协议规范
============================

原文来自 https://www.bittorrent.org/beps/bep_0003.html

:BEP: 3
:Title: BitTorrent 协议规范
:Version: $Revision$
:Last Modified: $Date$
:Author:  Bram Cohen <bram@bittorrent.com>
:Status:  Final
:Type:    Standard
:Created: 10-Jan-2008
:Post History:  24-6-2009 (arvid@bittorrent.com), 阐明了 torrent 文件中字符串的编码。
	20-10-2012 (arvid@bittorrent.com), 阐明了 info hash 是 .torrent 文件中的 en bencoding摘要。引入了一些对新bep的引用并清理了格式。
	11-10-2013 (arvid@bittorrent.com), 更正请求消息的可接受大小和实际大小
	04-2-2017 (the8472.bep@infinite-source.de), 进一步阐述 info hash，为新的实现者添加了资源



BitTorrent 是一种分发文件的协议。它通过 URL 标识内容，并设计为与 web 无缝集成。\
与普通 HTTP 相比，它的优势在于当同一文件的多个下载同时发生时，下载者会互相上传，\
使得文件源只需稍微增加一点负载就可以支撑大量的下载者。


一个 BitTorrent 文件分发包含这些实体:
----------------------------------------------------------

- 一个普通的 Web Server
- 一个静态的 "metainfo" 文件
- 一个 BitTorrent Tracker
- 一个原始的下载器
- 最终用户的 Web 浏览器
- 最终用户的下载程序

理想的情况是，单个文件有多个最终用户。

要开始服务，主机需要完成以下步骤：
----------------------------------------------------------

#. 开始运行一个 Tracker（或者，更可能的是，已经运行了一个 Tracker ）。
#. 开始运行一个普通的 Web 服务器，比如 Apache，或者已经正在运行一个。
#. 将 .torrent 的拓展名与 Web 服务器上的 mimetype application/x-bitorrent 关联起来（或者已经这样做了）。
#. 使用要提供的完整文件和跟踪器的 URL 生成元信息（.torrent）文件。
#. 将 metainfo (元信息) 文件放在 Web 服务器上。
#. 从其他网页链接到 metainfo（.torrent）文件。
#. 启动一个已经拥有完整文件（作为 “来源”）的下载器。

要开始下载，用户需要执行以下操作：
------------------------------------------------

#. 安装BitTorrent（或已经安装）。
#. 浏览网页。
#. 单击指向 .torrent 文件的链接。
#. 选择在本地保存文件的位置，或选择部分下载以继续。
#. 等待下载完成。
#. 告诉下载者退出（它会一直上传直到这种情况发生）。

bencoding
---------

- 字符串以长度为前缀的十进制数开头，后跟冒号和字符串。 例如 ``4:spam`` 对应于 'spam' 。

- 整数由 ``i`` 表示，后跟以10为底的数字，最跟 ``e`` 表示。例如，``i3e`` 对应于 3，\
  ``i-3e`` 对应于 -3。整数没有大小限制。 ``i-0e`` 是无效的。除 ``i0e`` （当然对应于0）\
  之外，所有带有前导零的编码（例如 ``i03e`` ）均无效。

- 列表被编码为 ``l`` ，后跟元素（也被编码），最后跟 ``e`` 。 例如， ``l4：spam4：eggse`` \
  对应于['spam', 'eggs']。

- 字典被编码为 ``d`` ，后跟一系列交替的 Key 及其对应的 Value，最后跟 ``e`` 。例如， \
  ``d3:cow3:moo4:spam4:eggse`` 对应于 ``{'cow':'moo', 'spam':'eggs'}`` 和 \
  ``d4:spaml1:a1:bee`` 对应于 ``{'spam':['a','b']}`` 。键必须是字符串并按排序顺序\
  显示（排序为原始字符串，而不是字母数字）。

元信息文件
--------------

元信息文件 (Metainfo files) (也称为 .torrent 文件) 被编码成带有以下键的标准词典：

announce
  Tracker 的 URL。

info
  这将映射到一个字典，其键如下所述。

.torrent 文件中包含文本的所有字符串都必须是 UTF-8 编码的。

info 字典
...............

``name`` 映射到一个UTF-8编码的字符串，这是将文件（或目录）另存为的建议名称。这纯粹是\
建议性的。

``piece length`` maps to the number of bytes in each piece
the file is split into. For the purposes of transfer, files are
split into fixed-size pieces which are all the same length except for
possibly the last one which may be truncated. ``piece
length`` is almost always a power of two, most commonly 2 18 =
256 K (BitTorrent prior to version 3.2 uses 2 20 = 1 M as
default).

``pieces`` maps to a string whose length is a multiple of
20. It is to be subdivided into strings of length 20, each of which is
the SHA1 hash of the piece at the corresponding index.

There is also a key ``length`` or a key ``files``,
but not both or neither. If ``length`` is present then the
download represents a single file, otherwise it represents a set of
files which go in a directory structure.

In the single file case, ``length`` maps to the length of
the file in bytes.

For the purposes of the other keys, the multi-file case is treated as
only having a single file by concatenating the files in the order they
appear in the files list. The files list is the value
``files`` maps to, and is a list of dictionaries containing
the following keys:

``length`` - The length of the file, in bytes.

``path`` - A list of UTF-8 encoded strings corresponding to subdirectory
names, the last of which is the actual file name (a zero length list
is an error case).

In the single file case, the name key is the name of a file, in the 
muliple file case, it's the name of a directory.

trackers
--------

Tracker GET requests have the following keys:

info_hash
  The 20 byte sha1 hash of the bencoded form of the info value from the
  metainfo file. This value will almost certainly have to be escaped.
  
  Note that this is a substring of the metainfo file.
  The info-hash must be the hash of the encoded form as found
  in the .torrent file, which is identical to bdecoding the metainfo file,
  extracting the info dictionary and encoding it *if and only if* the
  bdecoder fully validated the input (e.g. key ordering, absence of leading zeros).
  Conversely that means clients must either reject invalid metainfo files 
  or extract the substring directly.
  They must not perform a decode-encode roundtrip on invalid data.
    
  

peer_id
  A string of length 20 which this downloader uses as its id. Each
  downloader generates its own id at random at the start of a new
  download. This value will also almost certainly have to be escaped.

ip
  An optional parameter giving the IP (or dns name) which this peer is
  at. Generally used for the origin if it's on the same machine as the
  tracker.

port
  The port number this peer is listening on. Common behavior is for a
  downloader to try to listen on port 6881 and if that port is taken try
  6882, then 6883, etc. and give up after 6889.

uploaded
  The total amount uploaded so far, encoded in base ten ascii.

downloaded
  The total amount downloaded so far, encoded in base ten ascii.

left
  The number of bytes this peer still has to download, encoded in
  base ten ascii. Note that this can't be computed from downloaded and
  the file length since it might be a resume, and there's a chance that
  some of the downloaded data failed an integrity check and had to be
  re-downloaded.

event
  This is an optional key which maps to ``started``,
  ``completed``, or ``stopped`` (or
  ``empty``, which is the same as not being present). If not
  present, this is one of the announcements done at regular
  intervals. An announcement using ``started`` is sent when a
  download first begins, and one using ``completed`` is sent
  when the download is complete. No ``completed`` is sent if
  the file was complete when started. Downloaders send an announcement
  using ``stopped`` when they cease downloading.

Tracker responses are bencoded dictionaries. If a tracker response
has a key ``failure reason``, then that maps to a human
readable string which explains why the query failed, and no other keys
are required. Otherwise, it must have two keys: ``interval``,
which maps to the number of seconds the downloader should wait between
regular rerequests, and ``peers``. ``peers`` maps to
a list of dictionaries corresponding to ``peers``, each of
which contains the keys ``peer id``, ``ip``, and
``port``, which map to the peer's self-selected ID, IP
address or dns name as a string, and port number, respectively. Note
that downloaders may rerequest on nonscheduled times if an event
happens or they need more peers.

More commonly is that trackers return a compact representation of
the peer list, see `BEP 23`_.

.. _`BEP 23`: bep_0023.html

If you want to make any extensions to metainfo files or tracker
queries, please coordinate with Bram Cohen to make sure that all
extensions are done compatibly.

It is common to announce over a `UDP tracker protocol`_ as well.

.. _`UDP tracker protocol`: bep_0015.html

peer protocol
-------------

BitTorrent's peer protocol operates over TCP or `uTP`_.

.. _uTP: bep_0029.html

Peer connections are symmetrical. Messages sent in both directions
look the same, and data can flow in either direction.

The peer protocol refers to pieces of the file by index as
described in the metainfo file, starting at zero. When a peer finishes
downloading a piece and checks that the hash matches, it announces
that it has that piece to all of its peers.

Connections contain two bits of state on either end: choked or not,
and interested or not. Choking is a notification that no data will be
sent until unchoking happens. The reasoning and common techniques
behind choking are explained later in this document.

Data transfer takes place whenever one side is interested and the
other side is not choking. Interest state must be kept up to date at
all times - whenever a downloader doesn't have something they
currently would ask a peer for in unchoked, they must express lack of
interest, despite being choked. Implementing this properly is tricky,
but makes it possible for downloaders to know which peers will start
downloading immediately if unchoked.

Connections start out choked and not interested.

When data is being transferred, downloaders should keep several
piece requests queued up at once in order to get good TCP performance
(this is called 'pipelining'.) On the other side, requests which can't
be written out to the TCP buffer immediately should be queued up in
memory rather than kept in an application-level network buffer, so
they can all be thrown out when a choke happens.

The peer wire protocol consists of a handshake followed by a
never-ending stream of length-prefixed messages. The handshake starts
with character ninteen (decimal) followed by the string 'BitTorrent
protocol'. The leading character is a length prefix, put there in the
hope that other new protocols may do the same and thus be trivially
distinguishable from each other.

All later integers sent in the protocol are encoded as four bytes
big-endian.

After the fixed headers come eight reserved bytes, which are all
zero in all current implementations. If you wish to extend the
protocol using these bytes, please coordinate with Bram Cohen to make
sure all extensions are done compatibly.

Next comes the 20 byte sha1 hash of the bencoded form of the info
value from the metainfo file. (This is the same value which is
announced as ``info_hash`` to the tracker, only here it's raw
instead of quoted here). If both sides don't send the same value, they
sever the connection. The one possible exception is if a downloader
wants to do multiple downloads over a single port, they may wait for
incoming connections to give a download hash first, and respond with
the same one if it's in their list.

After the download hash comes the 20-byte peer id which is reported
in tracker requests and contained in peer lists in tracker
responses. If the receiving side's peer id doesn't match the one the
initiating side expects, it severs the connection.

That's it for handshaking, next comes an alternating stream of
length prefixes and messages. Messages of length zero are keepalives,
and ignored. Keepalives are generally sent once every two minutes, but
note that timeouts can be done much more quickly when data is
expected.

peer messages
-------------

All non-keepalive messages start with a single byte which gives their type.

The possible values are:

- 0 - choke
- 1 - unchoke
- 2 - interested
- 3 - not interested
- 4 - have
- 5 - bitfield
- 6 - request
- 7 - piece
- 8 - cancel

'choke', 'unchoke', 'interested', and 'not interested' have no payload.

'bitfield' is only ever sent as the first message. Its payload is a
bitfield with each index that downloader has sent set to one and the
rest set to zero. Downloaders which don't have anything yet may skip
the 'bitfield' message. The first byte of the bitfield corresponds to
indices 0 - 7 from high bit to low bit, respectively. The next one
8-15, etc. Spare bits at the end are set to zero.

The 'have' message's payload is a single number, the index which
that downloader just completed and checked the hash of.

'request' messages contain an index, begin, and length. The last
two are byte offsets. Length is generally a power of two unless it
gets truncated by the end of the file. All current implementations use
2^14 (16 kiB), and close connections which request an amount greater than
that.

'cancel' messages have the same payload as request messages. They
are generally only sent towards the end of a download, during what's
called 'endgame mode'. When a download is almost complete, there's a
tendency for the last few pieces to all be downloaded off a single
hosed modem line, taking a very long time. To make sure the last few
pieces come in quickly, once requests for all pieces a given
downloader doesn't have yet are currently pending, it sends requests
for everything to everyone it's downloading from. To keep this from
becoming horribly inefficient, it sends cancels to everyone else every
time a piece arrives.

'piece' messages contain an index, begin, and piece. Note that they
are correlated with request messages implicitly. It's possible for an
unexpected piece to arrive if choke and unchoke messages are sent in
quick succession and/or transfer is going very slowly.

Downloaders generally download pieces in random order, which does a
reasonably good job of keeping them from having a strict subset or
superset of the pieces of any of their peers.

Choking is done for several reasons. TCP congestion control behaves
very poorly when sending over many connections at once. Also, choking
lets each peer use a tit-for-tat-ish algorithm to ensure that they get
a consistent download rate.

The choking algorithm described below is the currently deployed
one. It is very important that all new algorithms work well both in a
network consisting entirely of themselves and in a network consisting
mostly of this one.

There are several criteria a good choking algorithm should meet. It
should cap the number of simultaneous uploads for good TCP
performance. It should avoid choking and unchoking quickly, known as
'fibrillation'. It should reciprocate to peers who let it
download. Finally, it should try out unused connections once in a
while to find out if they might be better than the currently used
ones, known as optimistic unchoking.

The currently deployed choking algorithm avoids fibrillation by
only changing who's choked once every ten seconds. It does
reciprocation and number of uploads capping by unchoking the four
peers which it has the best download rates from and are
interested. Peers which have a better upload rate but aren't
interested get unchoked and if they become interested the worst
uploader gets choked. If a downloader has a complete file, it uses its
upload rate rather than its download rate to decide who to
unchoke.

For optimistic unchoking, at any one time there is a single peer
which is unchoked regardless of its upload rate (if interested, it
counts as one of the four allowed downloaders.) Which peer is
optimistically unchoked rotates every 30 seconds. To give them a
decent chance of getting a complete piece to upload, new connections
are three times as likely to start as the current optimistic unchoke
as anywhere else in the rotation.

Resources
---------

* The `BitTorrent Economics Paper`__ outlines some request and choking
  algorithms clients should implement for optimal performance 

  __ http://bittorrent.org/bittorrentecon.pdf
  
* When developing a new implementation the Wireshark protocol analyzer and
  its `dissectors for bittorrent`__ can be useful to debug and compare with
  existing ones. 

  __ https://wiki.wireshark.org/BitTorrent
 


Copyright
---------

This document has been placed in the public domain.

