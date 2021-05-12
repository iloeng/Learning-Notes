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

`这里[1]`_ 是本节代码的改变 。 

.. _`这里[1]`: https://github.com/Deteriorator/SimpleDB/commit/691460d0a971d3f1a9bc4b60686da2e2c2dd45f9

******************************************************************************
第 06 部分  游标抽象
******************************************************************************

这一部分应该比上一部分短 。 我们只是要重构一下 ， 使之更容易启动 B-Tree 的实现 。 

我们将添加一个 Cursor 对象 ， 它代表了表中的一个位置 。 你可能想用游标做的事情 :

- 在表的开头创建一个游标
- 在表的末端创建一个游标
- 访问光标所指向的行
- 将游标推进到下一行

这些是我们现在要实现的行为 。 以后我们还将想 : 

- 删除游标所指向的行
- 修改游标所指向的记录
- 在表中搜索一个给定的 ID ， 并创建一个游标 ， 指向具有该 ID 的记录 。 

不多说了 ， 这里是光标类型 :

.. code-block:: C 

    typedef struct
    {
        Table* table;
        uint32_t row_num;
        bool end_of_table; // Indicates a position one past the last element
    } Cursor;

考虑到我们目前的表数据结构 ， 你只需要确定表中的一个位置就是行号 。 

一个游标也有一个对它所在表的引用 (所以我们的游标函数可以只接受游标作为参数) 。 

最后 ， 它有一个叫做 ``end_of_table`` 的布尔值 。 这是为了让我们能够表示一个超过表\
尾的位置 (这是我们可能想要插入一条记录的地方) 。 

``table_start()`` 和 ``table_end()`` 创建新的游标 : 

.. code-block:: C 

    Cursor* table_start(Table* table)
    {
        Cursor* cursor = malloc(sizeof(Cursor));
        cursor->table = table;
        cursor->row_num = 0;
        cursor->end_of_table = (table->num_rows == 0);

        return cursor;
    }

    Cursor* table_end(Table* table)
    {
        Cursor* cursor = malloc(sizeof(Cursor));
        cursor->table = table;
        cursor->row_num = table->num_rows;
        cursor->end_of_table = true;

        return cursor;
    }

我们的 ``row_slot()`` 函数将变成 ``cursor_value()`` ， 它返回一个指针到游标描述的\
位置 : 

.. code-block:: C 

    void* cursor_value(Cursor* cursor)
    {
        uint32_t row_num = cursor->row_num;
        uint32_t page_num = row_num / ROWS_PER_PAGE;
        void* page = get_page(cursor->table->pager, page_num);
        uint32_t row_offset = row_num % ROWS_PER_PAGE;
        uint32_t byte_offset = row_offset * ROW_SIZE;
        return page + byte_offset;
    }

在我们当前的表结构中推进游标 ， 就像增加行号一样简单 。 在 B 型树中 ， 这将是一个比\
较复杂的过程 。 

.. code-block:: C 

    void* cursor_advance(Cursor* cursor)
    {
        cursor->row_num += 1;
        if (cursor->row_num >= cursor->table->num_rows)
        {
            cursor->end_of_table = true;
        }
    }

最后我们可以改变我们的 "虚拟机" 方法来使用游标抽象 。 当插入一行时 ， 我们在表的末端\
打开一个游标 ， 写到该游标位置 ， 然后关闭游标 。 

.. code-block:: C 

    ExecuteResult execute_insert(Statement* statement, Table* table)
    {
        if (table->num_rows >= TABLE_MAX_ROWS)
        {
            return EXECUTE_TABLE_FULL;
        }
        Row* row_to_insert = &(statement->row_to_insert);
        Cursor* cursor = table_end(table);
        serialize_row(row_to_insert, cursor_value(cursor));
        table->num_rows += 1;
        free(cursor);
        return EXECUTE_SUCCESS;
    }

当选择表中的所有行时 ， 我们在表的开始处打开一个光标 ， 打印该行 ， 然后将光标推进到\
下一行 。 重复这个过程 ， 直到我们到达表的末端 。 

.. code-block:: c

    ExecuteResult execute_select(Statement* statement, Table* table)
    {
        Cursor* cursor = table_start(table);
        Row row;
        while (!(cursor->end_of_table))
        {
            deserialize_row(cursor_value(cursor), &row);
            print_row(&row);
            cursor_advance(cursor);
        }
        free(cursor);
        return EXECUTE_SUCCESS;
    }

好了就这样吧 ! 就像我说的 ， 这是一个较短的重构 ， 当我们把表的数据结构重写成 \
B-Tree 时 ， 它应该能帮助我们 。 ``execute_select()`` 和 ``execute_insert()`` \
可以完全通过游标与表进行交互 ， 而不需要假设任何关于表的存储方式 。 

`这里[2]`_ 是这部分的完整差异 。 

.. _`这里[2]`: https://github.com/Deteriorator/SimpleDB/commit/d0f57e79a1485cd202ffd3e28cd159747d0b5696

******************************************************************************
第 07 部分  B 型树简介
******************************************************************************

B 树是 SQLite 用来表示表和索引的数据结构 ， 所以它是一个相当核心的概念 。 这篇文章\
将只是介绍这个数据结构 ， 所以不会有任何代码 。 

为什么说树是数据库的一个好的数据结构 ? 

- 搜索一个特定的值是快速的 (对数时间) 。
- 插入 / 删除一个你已经找到的值是快速的 (重新平衡的时间是恒定的) 。
- 遍历一个值的范围是快速的 (不像哈希图) 。 

B 树不同于二进制树 ("B"可能代表发明者的名字 ， 但也可能代表 "平衡") 。 下面是一个 \
B 树的例子 : 

.. image:: img/B-tree.svg

example B-Tree (https://en.wikipedia.org/wiki/File:B-tree.svg)

与二叉树不同 ， B 树中的每个节点可以有 2 个以上的子节点 。 每个节点最多可以有 m 个子\
节点 ， 其中 m 被称为树的 "顺序" 。 为了保持树的基本平衡 ， 我们还说节点必须至少有 \
m/2 个子节点 (四舍五入) 。

