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



