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

前四个字节是第一行的 ID (4个字节 ， 因为我们存储的是 uint32_t) 。 它是以小端 \
(little-endian) 的字节顺序存储的 ， 所以最低字节在前 (01) ， 后面是高序字节 \
(00 00 00) 。 我们使用 ``memcpy()`` 将字节从我们的 Row 结构复制到页面缓存中 ， 所\
以这意味着该结构在内存中是以小端字节顺序排列的 。 这是我编译程序机器的一个属性 。 如\
果我们想在我的机器上写一个数据库文件 ， 然后在大端机器上读取它 ， 我们就必须改变我们\
的 ``serialize_row()`` 和 ``deserialize_row()`` 方法 ， 以便始终以相同的顺序存储\
和读取字节 。 

接下来的 33 个字节将用户名存储为一个空尾字符串 。 显然 ， "cstack" 的 ASCII 码以十\
六进制表示是 63 73 74 61 63 6b ， 后面是一个空字符 (00) 。 其余的 33 个字节没有使\
用 。 

接下来的 256 字节以同样的方式存储电子邮件 。 在这里我们可以看到在结束的空字符之后有\
一些随机的垃圾 。 这很可能是由于我们的 Row 结构中未初始化的内存造成的 。 我们将整个 \
256 字节的电子邮件缓冲区复制到文件中 ， 包括字符串结束后的任何字节 。 当我们分配该结\
构时 ， 内存中的任何东西都还在那里 。 但由于我们使用了一个结束性的空字符 ， 所以它对\
行为没有影响 。 

注意 ： 如果我们想确保所有字节都被初始化 ， 那么在 ``serialize_row`` 中复制行的用户\
名和电子邮件字段时 ， 使用 ``strncpy`` 就足够了 ， 而不是 ``memcpy`` ， 像这样 ： 

.. code-block:: C 

    void serialize_row(Row* source, void* destination)
    {
        memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
        strncpy(destination + USERNAME_OFFSET, source->username, USERNAME_SIZE);
        strncpy(destination + EMAIL_OFFSET, source->email, EMAIL_SIZE);
    }

总结 ： 

好了 ！ 我们已经有了持久化 。 这不是最好的实现 。 例如 ， 如果你不输入 ``.exit`` 就\
关闭程序 ， 你就会失去你的改变 。 此外 ， 我们正在把所有的页面写回磁盘 ， 即使是在我\
们从磁盘上读取后没有改变的页面 。 这些问题我们可以在以后解决 。 

下一次我们将介绍 cursors ， 这将使 B 树的实现变得更加容易 。 

在那之前 ! 

这里_ 是本节代码的改变 。 

.. _这里: https://github.com/Deteriorator/SimpleDB/commit/691460d0a971d3f1a9bc4b60686da2e2c2dd45f9


