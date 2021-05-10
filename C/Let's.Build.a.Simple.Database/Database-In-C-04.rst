##############################################################################
C 语言从头写一个 SQLite 程序
##############################################################################

.. contents::

******************************************************************************
第 05 部分  持久化到磁盘
******************************************************************************

有了这些变化 ， 我们就能关闭然后重新打开数据库 ， 我们的记录仍然在那里 ！

.. code-block:: C 

    complie: 
    gcc -g -w -Wall -o db simpledb.c

    ~ ./db mydb.db
    db > insert 1 cstack foo@bar.com
    Executed.
    db > insert 2 voltorb volty@example.com
    Executed.
    db > .exit
    ~
    ~ ./db mydb.db
    db > select
    (1, cstack, foo@bar.com)
    (2, voltorb, volty@example.com)
    Executed.
    db > .exit
    ~

为了增加乐趣 ， 让我们看一看 mydb.db ， 看看我们的数据是如何被存储的 。 我将使用 \
vim 作为一个十六进制编辑器来查看文件的内存布局 :

.. code-block:: shell

    vim mydb.db
    :%!xxd

.. image:: img/file-format.png 

Current File Format



