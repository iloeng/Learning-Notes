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

首先是命令行参数，在创建异步循环。

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

在 ``Torrent`` 类中，初始化会打开给定的文件名。

元信息在读取种子文件的时候，对其进行解码，然后用 ``self.meta_info`` 表示。而信息是从\
``self.meta_info`` dict 字典中 ``b'info'`` 所代表的值， ``self.info_hash`` 是信息\
的 sha1 值， 最后在验证文件。


