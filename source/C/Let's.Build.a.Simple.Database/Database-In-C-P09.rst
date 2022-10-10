*******************************************************************************
Part 09 - 二进制搜索和重复 Key
*******************************************************************************

.. contents:: 目录
    :depth: 3
    :backlinks: top

上次我们注意到， 我们仍然以未排序的顺序存储密钥。 我们将解决该问题， 并检测并拒绝重复\
的键。 

现在我们的 ``execute_insert()`` 函数始终选择在表的末尾插入。 相反， 我们应该在表格\
中搜索正确的要插入的位置， 然后在此处插入。 如果密钥已经存在， 则返回错误。

.. code-block:: C

    ExecuteResult execute_insert(Statement* statement, Table* table)
    {
        void* node = get_page(table->pager, table->root_page_num);
        uint32_t num_cells = (*leaf_node_num_cells(node));
        if (num_cells >= LEAF_NODE_MAX_CELLS)
        {
            return EXECUTE_TABLE_FULL;
        }
        Row* row_to_insert = &(statement->row_to_insert);
        uint32_t key_to_insert = row_to_insert->id;
        Cursor* cursor = table_find(table, key_to_insert);

        if (cursor->cell_num < num_cells)
        {
            uint32_t key_at_index = *leaf_node_key(node, cursor->cell_num);
            if (key_at_index == key_to_insert)
            {
                return EXECUTE_DUPLICATE_KEY;
            }
        }

        leaf_node_insert(cursor, row_to_insert->id, row_to_insert);
        free(cursor);
        return EXECUTE_SUCCESS;
    }

我们不再需要 ``table_end()`` 函数。 

我们将用一个方法来代替它， 在树上搜索一个给定的键。 

.. code-block:: C 

    /*
    Return the position of the given key.
    If the key is not present, return the position
    where it should be inserted
    */
    Cursor* table_find(Table* table, uint32_t key) 
    {
        uint32_t root_page_num = table->root_page_num;
        void* root_node = get_page(table->pager, root_page_num);

        if (get_node_type(root_node) == NODE_LEAF) 
        {
            return leaf_node_find(table, root_page_num, key);
        } else {
            printf("Need to implement searching an internal node\n");
            exit(EXIT_FAILURE);
        }
    }

我正在为内部节点建立分支， 因为我们尚未实现内部节点。 我们可以用二进制搜索来搜索叶节点。 

.. code-block:: C 

    Cursor* leaf_node_find(Table* table, uint32_t page_num, uint32_t key)
    {
        void* node = get_page(table->pager, page_num);
        uint32_t num_cells = *leaf_node_num_cells(node);
        Cursor* cursor = malloc(sizeof(Cursor));
        cursor->table = table;
        cursor->page_num = page_num;

        // Binary search
        uint32_t min_index = 0;
        uint32_t one_past_max_index = num_cells;
        while (one_past_max_index != min_index)
        {
            uint32_t index = (min_index + one_past_max_index) / 2;
            uint32_t key_at_index = *leaf_node_key(node, index);
            if (key == key_at_index)
            {
                cursor->cell_num = index;
                return cursor;
            }
            if (key < key_at_index)
            {
                one_past_max_index = index;
            } else {
                min_index = index + 1;
            }
        }
        cursor->cell_num = min_index;
        return cursor;
    }

这将返回: 

- 键的位置。
- 另一个键的位置， 如果我们想插入新的键， 我们需要移动这个键的位置， 或
- 最后一个键之后的位置

由于我们现在要检查节点类型， 我们需要函数来获取和设置节点中的那个值。 

.. code-block:: C 

    NodeType get_node_type(void* node)
    {
        uint8_t value = *((uint8_t*)(node + NODE_TYPE_OFFSET));
        return (NodeType)value;
    }

    void set_node_type(void* node, NodeType type)
    {
        uint8_t value = type;
        *((uint8_t*)(node + NODE_TYPE_OFFSET)) = value;
    }

我们必须先强制​​转换为 ``uint8_t``， 以确保将其序列化为单个字节。 

我们还需要初始化节点类型。 

.. code-block:: C 

    void initialize_leaf_node(void* node)
    {
        set_node_type(node, NODE_LEAF);
        *leaf_node_num_cells(node) = 0;
    }

最后， 我们需要制作并处理一个新的错误代码。 

.. code-block:: C

    typedef enum
    {
        EXECUTE_SUCCESS,
        EXECUTE_DUPLICATE_KEY,
        EXECUTE_TABLE_FULL
    } ExecuteResult;

    [main]
    switch (execute_statement(&statement, table))
    {
        case (EXECUTE_SUCCESS):
            printf("Executed!\n");
            break;
        case (EXECUTE_DUPLICATE_KEY):
            printf("Error: Duplicate key.\n");
            break;
        case (EXECUTE_TABLE_FULL):
            printf("Error: Table full.\n");
            break;
    }

通过这些更改， 我们的测试可以更改为检查排序顺序： 

.. code-block:: ruby 

    it 'allows printing out the structure of a one-node btree' do
        script = [3, 1, 2].map do |i|
        "insert #{i} user#{i} person#{i}@example.com"
        end
        script << ".btree"
        script << ".exit"
        result = run_script(script)

        expect(result).to match_array([
        "db > Executed.",
        "db > Executed.",
        "db > Executed.",
        "db > Tree:",
        "leaf (size 3)",
        "  - 0 : 1",
        "  - 1 : 2",
        "  - 2 : 3",
        "db > "
        ])
    end

我们可以为重复的键添加新的测试： 

.. code-block:: ruby

    it 'prints an error message if there is a duplicate id' do
        script = [
        "insert 1 user1 person1@example.com",
        "insert 1 user1 person1@example.com",
        "select",
        ".exit",
        ]
        result = run_script(script)
        expect(result).to match_array([
        "db > Executed.",
        "db > Error: Duplicate key.",
        "db > (1, user1, person1@example.com)",
        "Executed.",
        "db > ",
        ])
    end

就是这样！ 下一步： 实现拆分叶节点并创建内部节点。 

`这里[7]`_ 是本节代码所有的改动。 

.. _这里[7]: https://github.com/Deteriorator/SimpleDB/commit/4e0343d37213667a8064a8936c6d8dbe13be0375
