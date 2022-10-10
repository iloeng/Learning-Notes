*******************************************************************************
Part 06 - 游标抽象
*******************************************************************************

.. contents:: 目录
    :depth: 3
    :backlinks: top

这一部分应该比上一部分短。 我们只是要重构一下， 使之更容易启动 B-Tree 的实现。 

我们将添加一个 Cursor 对象， 它代表了表中的一个位置。 你可能想用游标做的事情:

- 在表的开头创建一个游标
- 在表的末端创建一个游标
- 访问光标所指向的行
- 将游标推进到下一行

这些是我们现在要实现的行为。 以后我们还将想: 

- 删除游标所指向的行
- 修改游标所指向的记录
- 在表中搜索一个给定的 ID， 并创建一个游标， 指向具有该 ID 的记录。 

不多说了， 这里是光标类型:

.. code-block:: C 

    typedef struct
    {
        Table* table;
        uint32_t row_num;
        bool end_of_table; // Indicates a position one past the last element
    } Cursor;

考虑到我们目前的表数据结构， 你只需要确定表中的一个位置就是行号。 

一个游标也有一个对它所在表的引用 (所以我们的游标函数可以只接受游标作为参数)。 

最后， 它有一个叫做 ``end_of_table`` 的布尔值。 这是为了让我们能够表示一个超过表尾的\
位置 (这是我们可能想要插入一条记录的地方)。 

``table_start()`` 和 ``table_end()`` 创建新的游标: 

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

我们的 ``row_slot()`` 函数将变成 ``cursor_value()``， 它返回一个指针到游标描述的位\
置: 

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

在我们当前的表结构中推进游标， 就像增加行号一样简单。 在 B 型树中， 这将是一个比较复\
杂的过程。 

.. code-block:: C 

    void* cursor_advance(Cursor* cursor)
    {
        cursor->row_num += 1;
        if (cursor->row_num >= cursor->table->num_rows)
        {
            cursor->end_of_table = true;
        }
    }

最后我们可以改变我们的 "虚拟机" 方法来使用游标抽象。 当插入一行时， 我们在表的末端打\
开一个游标， 写到该游标位置， 然后关闭游标。 

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

当选择表中的所有行时， 我们在表的开始处打开一个光标， 打印该行， 然后将光标推进到下一\
行。 重复这个过程， 直到我们到达表的末端。 

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

好了就这样吧! 就像我说的， 这是一个较短的重构， 当我们把表的数据结构重写成 B-Tree 时\
， 它应该能帮助我们。 ``execute_select()`` 和 ``execute_insert()`` 可以完全通过游\
标与表进行交互， 而不需要假设任何关于表的存储方式。 

`这里[4]`_ 是这部分的完整差异。 

.. _`这里[4]`: https://github.com/iloeng/SimpleDB/commit/d0f57e79a1485cd202ffd3e28cd159747d0b5696
