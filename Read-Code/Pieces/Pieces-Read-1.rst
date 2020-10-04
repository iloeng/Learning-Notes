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

pieces.py
==========================

从这个文件开始阅读，发现就调用了 /pieces/cli.py 文件中的 main 函数。

pieces/cli.py
=========================


