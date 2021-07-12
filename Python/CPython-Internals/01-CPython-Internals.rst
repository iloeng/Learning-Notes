##############################################################################
CPython Internals: Your Guide to The Python 3 Interpreter
##############################################################################

.. contents::

******************************************************************************
第 1 部分  书籍阅读环境 
******************************************************************************

打算在 Windows 10 系统上边阅读边调试代码 ， IDE 是 CLion 。 

下载代码 ： 

.. code-block:: bash

    git clone https://github.com/python/cpython
    cd cpython
    git checkout tags/v3.9.0b1 -b v3.9.0b1

    或者直接下载代码包：
    https://github.com/python/cpython/archive/v3.9.0b1.zip

下载代码之后 ， 是没有 Makefile 文件的 ， 需要执行 configure 脚本检查环境并生成 \
Makefile 文件 。 之后才能进行下一步操作 。 

当然由于需要在 Windows 上查看源代码 ， 因此最好安装 cygwin 包 。 安装的时候记得勾\
选 bash 。

这时就可以在 Windows 命令行里面使用 shell 脚本了 。 

.. code-block:: 

    bash ./configure

执行这条命令来生成 Makefile 文件 ， 否则是没办法使用书中的 compiledb 命令的 。 

同时安装 Python 的包 

.. code-block:: 

    pip install compiledb

    或者使用国内源加速一下

    pip install -i https://pypi.douban.com/simple compiledb


