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



