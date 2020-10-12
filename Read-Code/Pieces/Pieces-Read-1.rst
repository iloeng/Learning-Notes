Pieces 源码阅读系列 1
---------------------------------

前言
===================

之前翻译了使用 Python 编写 BitTorrent 客户端的文章，现在开始阅读作者所实现的代码。

不过我当前环境使用的是 Python 3.7，因此需要对源代码做一点小的调整。安装完毕依赖模\
块后在 Python 3.7 中， ``async`` 已经成为一个关键字，因此需要修改 ``aiohttp`` 中\
的 helpers.py。修改如下：

.. code-block:: Python

    ensure_future = asyncio.async

.. code-block:: Python

    ensure_future = getattr(asyncio, 'async')

不要对代码进行大规模修改。

或者升级 ``aiohttp`` 版本，我是通过修改代码来解决这个问题的。

pieces.py
==========================

从这个文件开始阅读，发现就调用了 /pieces/cli.py 文件中的 main 函数。

pieces/cli.py
=========================

.. code-block:: Python

    def main():
        parser = argparse.ArgumentParser()
        parser.add_argument('torrent',
                            help='the .torrent to download')
        parser.add_argument('-v', '--verbose', action='store_true',
                            help='enable verbose output')

        args = parser.parse_args()
        if args.verbose:
            logging.basicConfig(level=logging.INFO)

        loop = asyncio.get_event_loop()
        client = TorrentClient(Torrent(args.torrent))
        task = loop.create_task(client.start())

        def signal_handler(*_):
            logging.info('Exiting, please wait until everything is shutdown...')
            client.stop()
            task.cancel()

        signal.signal(signal.SIGINT, signal_handler)

        try:
            loop.run_until_complete(task)
        except CancelledError:
            logging.warning('Event loop was canceled')

首先是命令行参数，在创建异步事件循环，然后通过 ``Torrent`` 类对传入的 torrent 进行解析，\
解析之后的数据传给 ``TorrentClient`` 用于进行对等连接。

``signal.signal(signal.SIGINT, signal_handler)`` 在这里，调用的是 Signal 信号模块\
中的 ``signal.SIGINT`` 中断信号， ``signal.signal(signalnum, handler)`` 设置信号\
处理的函数，也就是当遇到连接中断信号 ``signal.SIGINT`` 后，就会执行设置的 ``signal_handler`` \
函数，而这个函数内部是停止 Client ，取消 task 任务， 最后是要求整个循环任务执行完毕。\
否则就会抛出被取消的异常。

整个 ``main`` 函数的处理过程就是这些，接下来继续看 ``Torrent`` 类的处理过程：

.. code-block:: Python

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

        def _identify_files(self):
            """
            Identifies the files included in this torrent
            """
            ...
            pass

        @property
        def announce(self) -> str:
            """
            The announce URL to the tracker.
            """
            return self.meta_info[b'announce'].decode('utf-8')

        @property
        def multi_file(self) -> bool:
            """
            Does this torrent contain multiple files?
            """
            # If the info dict contains a files element then it is a multi-file
            return b'files' in self.meta_info[b'info']

        @property
        def piece_length(self) -> int:
            """
            Get the length in bytes for each piece
            """
            return self.meta_info[b'info'][b'piece length']

        @property
        def total_size(self) -> int:
            """
            The total size (in bytes) for all the files in this torrent. For a
            single file torrent this is the only file, for a multi-file torrent
            this is the sum of all files.

            :return: The total size (in bytes) for this torrent's data.
            """
            pass

        @property
        def pieces(self):
            # The info pieces is a string representing all pieces SHA1 hashes
            # (each 20 bytes long). Read that data and slice it up into the
            # actual pieces
            pass

        @property
        def output_file(self):
            return self.meta_info[b'info'][b'name'].decode('utf-8')

        def __str__(self):
            pass

以上部分函数的处理过程已经省略，后面用到后再详细分析。在 ``Torrent`` 类中，初始化会读取\
给定的 torrent 文件名。

元信息在读取种子文件的时候，对其进行解码，然后用 ``self.meta_info`` 表示。而信息是从\
``self.meta_info`` dict 字典中 ``b'info'`` 所代表的值， ``self.info_hash`` 是信息\
的 sha1 值， 最后在验证文件。

解析完毕之后，会返回如下格式的信息：

.. code-block::

    Filename: b'ubuntu-16.04-desktop-amd64.iso'
    File length: 1485881344
    Announce URL: b'http://torrent.ubuntu.com:6969/announce'
    Hash: b"CDP;~y~\xbf1X#'\xa5\xba\xae5\xb1\x1b\xda\x01"

他是通过这个函数生成的数据：

.. code-block:: Python

    def __str__(self):
        return 'Filename: {0}\n' \
               'File length: {1}\n' \
               'Announce URL: {2}\n' \
               'Hash: {3}'.format(self.meta_info[b'info'][b'name'],
                                  self.meta_info[b'info'][b'length'],
                                  self.meta_info[b'announce'],
                                  self.info_hash)

接下来看解码部分：

.. code-block:: Python

    class Decoder:
        """
        Decodes a bencoded sequence of bytes.
        """
        def __init__(self, data: bytes):
            if not isinstance(data, bytes):
                raise TypeError('Argument "data" must be of type bytes')
            self._data = data
            self._index = 0

        def decode(self):
            """
            Decodes the bencoded data and return the matching python object.

            :return A python object representing the bencoded data
            """
            c = self._peek()
            if c is None:
                raise EOFError('Unexpected end-of-file')
            elif c == TOKEN_INTEGER:
                self._consume()  # The token
                return self._decode_int()
            elif c == TOKEN_LIST:
                self._consume()  # The token
                return self._decode_list()
            elif c == TOKEN_DICT:
                self._consume()  # The token
                return self._decode_dict()
            elif c == TOKEN_END:
                return None
            elif c in b'01234567899':
                return self._decode_string()
            else:
                raise RuntimeError('Invalid token read at {0}'.format(
                    str(self._index)))

解码的时候，主要函数就是 ``decode()`` 函数。

``Decoder`` 类初始化的时候，首先会判断传入的参数是不是二进制数据，如果不是，就会抛出异常。\
这是因为 Bencoded 后的数据是二进制的。然后使用 ``self._data`` 保存传入的 ``data`` ， 索\
引 ``self._index`` 初始化为 ``0`` 。

解码的第一步，执行了 ``_peek()`` 函数， 其作用是返回下一个 bencoded 编码后的数据段的第一个\
字符，它用于标识分隔符后的数据代表的是什么数据 （字符串， 数字， 列表和字典）。当 ``_peek()`` \
为空时，代表当前数据是无效的。下面是数据的标识：

 | 1. 字符串以长度为前缀的十进制数开头，后跟冒号和字符串。 例如 ``4:spam`` 对应于 'spam' 。

 | 2. 整数由 ``i`` 表示，后跟以10为底的数字，最跟 ``e`` 表示。例如，``i3e`` 对应于 3，\
   ``i-3e`` 对应于 -3。整数没有大小限制。 ``i-0e`` 是无效的。除 ``i0e`` （当然对应于0）\
   之外，所有带有前导零的编码（例如 ``i03e`` ）均无效。

 | 3. 列表被编码为 ``l`` ，后跟元素（也被编码），最后跟 ``e`` 。 例如， ``l4：spam4：eggse`` \
   对应于['spam', 'eggs']。

 | 4. 字典被编码为 ``d`` ，后跟一系列交替的 Key 及其对应的 Value，最后跟 ``e`` 。例如， \
   ``d3:cow3:moo4:spam4:eggse`` 对应于 ``{'cow':'moo', 'spam':'eggs'}`` 和 \
   ``d4:spaml1:a1:bee`` 对应于 ``{'spam':['a','b']}`` 。键必须是字符串并按排序顺序\
   显示（排序为原始字符串，而不是字母数字）。

而在代码中，是这样实现的：

.. code-block:: python

    # Indicates start of integers
    TOKEN_INTEGER = b'i'

    # Indicates start of list
    TOKEN_LIST = b'l'

    # Indicates start of dict
    TOKEN_DICT = b'd'

    # Indicate end of lists, dicts and integer values
    TOKEN_END = b'e'

    # Delimits string length from string data
    TOKEN_STRING_SEPARATOR = b':'

其中字符串类型的并没有标识出来，因为字符串是以数字开头的。

然后根据 ``_peek()`` 函数返回的结果进行解码操作。其源码如下：

.. code-block:: python

    def _peek(self):
        """
        Return the next character from the bencoded data or None
        """
        if self._index + 1 >= len(self._data):
            return None
        return self._data[self._index:self._index + 1]

首先判断当前索引值加 1 是否大于或等于当前的数据长度，如果大于或等于，则表明数据已经\
解码完毕，因此返回 ``None`` ，小于的情况就直接返回当前索引开始的第一个字符。拿一个\
数据进行演示：

::

    b'l4:spam4:eggsi123ee'

解码函数第一次运行时，初始化的索引值为 0 ，执行 ``_peek`` 函数时，首先会返回字符 ``l`` \
，也就是 ``decode()`` 函数中局部变量 ``c`` 为 ``l`` ，代表的是 list ，需要执行的\
过程为：

.. code-block:: python

    elif c == TOKEN_LIST:
        self._consume()  # The token
        return self._decode_list()

在这里，调用了 ``_consume()`` 函数，它的作用是对当前的索引值加 1 ，因为前面已经读取\
了数据标识符，因此而加一，代表的是下一个字符。最后返回的是 ``_decode_list()`` 函数值。\
看一下源码：

.. code-block:: python

    def _consume(self) -> bytes:
        """
        Read (and therefore consume) the next character from the data
        """
        self._index += 1

    def _decode_list(self):
        res = []
        # Recursive decode the content of the list
        while self._data[self._index: self._index + 1] != TOKEN_END:
            res.append(self.decode())
        self._consume()  # The END token
        return res

``_decode_list`` 函数执行时，局部变量 ``res`` 存储解码结果，先判断当标识符后的第一个字符不\
是结束符，在此对数据进行解码操作，通过嵌套循环实现解码。

然后，执行 ``_peek`` 函数获取到 c=4 ，这时就会执行如下步骤：

.. code-block:: python 

    def decode(self):
        ...
        elif c in b'01234567899':
            return self._decode_string()
        ...

    def _decode_string(self):
        bytes_to_read = int(self._read_until(TOKEN_STRING_SEPARATOR))
        data = self._read(bytes_to_read)
        return data

在 ``_decode_string`` 函数内，首先会计算需要读取多少个字节的数据需要读取，一直读取到字符串\
分隔符 ``:`` 。然后将需要读取的数据返回出来。

在字符串解码函数 ``_decode_string`` 中调用了 ``_read_until`` 和 ``_read`` 函数，源码如下：

.. code-block:: python

    def _read(self, length: int) -> bytes:
        """
        Read the `length` number of bytes from data and return the result
        """
        if self._index + length > len(self._data):
            raise IndexError('Cannot read {0} bytes from current position {1}'
                             .format(str(length), str(self._index)))
        res = self._data[self._index:self._index+length]
        self._index += length
        return res

    def _read_until(self, token: bytes) -> bytes:
        """
        Read from the bencoded data until the given token is found and return
        the characters read.
        """
        try:
            occurrence = self._data.index(token, self._index)
            result = self._data[self._index:occurrence]
            self._index = occurrence + 1
            return result
        except ValueError:
            raise RuntimeError('Unable to find token {0}'.format(
                str(token)))

首先分析 ``_read_until`` 函数，因为首先调用的是它。传入的参数是 ``TOKEN_STRING_SEPARATOR`` \
字符串分隔符，在函数内部的过程是：

1. 获取以当前索引值开始， ``TOKEN_STRING_SEPARATOR`` 的索引值 ``occurrence`` 。详细可以看一\
   下内置函数 ``index`` 的声明：

   ::

        def index(self, sub, start=None, end=None):

2. 然后用 ``result`` 保存从当前索引值（已经解码到哪里了）到分隔符之间的数据，这个数据就是要读\
   取的字符串数据的长度（多少字节），然后将当前索引值改成字符串分隔符的索引值加 1 ，这是因为\
   已经把长度解码出来了，更新解码状态，只需要从分隔符后开始进行解码 ``b'l4:spam4:eggsi123ee'`` \
   在这个例子中就是 ``l4:`` 已经解码完毕了。

``_read_until`` 函数分析完毕，得出的长度是 4 字节，然后执行 ``_read`` 函数。其过程如下：

1. 首先判断当前索引值加上要读取的字节长度是否大于整个编码后的文本，如果大于了，就表明有错误。

2. 字符串切片获取从当前索引值开始的 4 字节长度的字符串

3. 将当前索引值更新为加上字节长度的索引值，因为字节长度的字符已经解码了

4. 最后返回字符串切片获取的 4 字节长度的字符串

到这里，字符串解码分析完毕。

当前的索引值 ``self._index`` 就被更新为指向 ``l4:spam`` 后面的 4 了。但是在 ``_decode_list`` \
函数内只完成了一轮循环，接下来的一轮循环是解析 ``4:eggs`` ，与上文相同，就不在详细分析，接下\
来解析 ``i123e`` 。

同样的步骤，通过第一个字符 ``i`` 得知是数字，需要执行的是：

.. code-block:: python

    def decode(self):
        ...
        elif c == TOKEN_INTEGER:
            self._consume()  # The token
            return self._decode_int()
        ...

    def _decode_int(self):
        return int(self._read_until(TOKEN_END))

对当前索引值加一后，执行 ``_decode_int`` 函数，一直读取到结束符 ``e`` ，从而获取到 123 。\
同时，当前索引值也被更新为指向最后一个字符 ``e`` ，然后由于与结束字符相等，结束整个解析列\
表的循环，并把当前索引值更新为最后字符的索引加上 1 ，表示解码完毕。

最终结果如下：

::

    b'l4:spam4:eggsi123ee' => [b'spam', b'eggs', 123]

找的这个示例字符串不太好，没有 dict 的分析。重新在单元测试中找了一个 dict 数据进行分析：

:: 

    b'd3:cow3:moo4:spam4:eggse'

同理，获取到第一个字符 ``d`` 得知是 dict 数据，需要执行下面的过程：

.. code-block:: python 

    def decode(self):
        ...
        elif c == TOKEN_DICT:
            self._consume()  # The token
            return self._decode_dict()
        ...
    
    def _decode_dict(self):
        res = OrderedDict()
        while self._data[self._index: self._index + 1] != TOKEN_END:
            key = self.decode()
            obj = self.decode()
            res[key] = obj
        self._consume()  # The END token
        return res

第一步也是更新当前的索引值，然后执行 ``_decode_dict`` 函数。该函数的过程如下：

1. 创建一个有序字典 ``res`` ，使插入其中的数据安装插入顺序输入。

2. 创建循环，当当前索引值开始的第一个字符不是结束符时，进入循环，对 dict 数据进行解码。\
   以同样的方式解码字符串，从而得到 ``key=cow obj=moo`` ， 完成第一轮循环，然后以相同\
   的方式得到 ``key=spam obj=eggs`` 。 ``e`` 为结束符。

最终的返回结果为：

::

    b'd3:cow3:moo4:spam4:eggse' => OrderedDict([(b'cow', b'moo'), (b'spam', b'eggs')])

到此，整个 Bencoded 编码后数据的解码器分析完毕，接下来就跟随 ``Torrent`` 类中的步骤 \
``info = bencoding.Encoder(self.meta_info[b'info']).encode()`` 来分析编码器。
