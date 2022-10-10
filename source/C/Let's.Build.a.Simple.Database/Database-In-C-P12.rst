*******************************************************************************
Part 12 - 扫描多层次的 B 型树
*******************************************************************************

.. contents:: 目录
    :depth: 3
    :backlinks: top

我们现在支持构建多级 btree， 但在此过程中， 我们已经破坏了 ``select`` 语句。 这是一\
个测试案例， 该案例插入 15 行， 然后尝试打印它们。 

.. code-block:: ruby

    it 'prints all rows in a multi-level tree' do
        script = []
        (1..15).each do |i|
            script << "insert #{i} user#{i} person#{i}@example.com"
        end
        script << "select"
        script << ".exit"
        result = run_script(script)

        expect(result[15...result.length]).to match_array([
            "db > (1, user1, person1@example.com)",
            "(2, user2, person2@example.com)",
            "(3, user3, person3@example.com)",
            "(4, user4, person4@example.com)",
            "(5, user5, person5@example.com)",
            "(6, user6, person6@example.com)",
            "(7, user7, person7@example.com)",
            "(8, user8, person8@example.com)",
            "(9, user9, person9@example.com)",
            "(10, user10, person10@example.com)",
            "(11, user11, person11@example.com)",
            "(12, user12, person12@example.com)",
            "(13, user13, person13@example.com)",
            "(14, user14, person14@example.com)",
            "(15, user15, person15@example.com)",
            "Executed.", "db > ",
        ])
    end

但是当我们运行该测试用例时， 实际发生的是： 

.. code-block:: ruby

    db > select
    (2, user1, person1@example.com)
    Executed.

那真是怪了。 它仅打印一行， 而该行看起来已损坏 (请注意， 该 ID 与用户名不匹配)。

这种奇怪的现象是因为 ``execute_select()`` 从表头开始， 而我们目前实现的 \
``table_start()`` 返回根节点的 0 号单元格。 但是我们的树的根现在是一个内部节点， 不\
包含任何行。 被打印的数据一定是根节点是叶子时留下的。 ``execute_select()`` 应该真正\
返回最左边叶子节点的 0 号单元格。 

因此需要摆脱旧的执行方式。  

并添加一个新的实现， 搜索键 0 (可能的最小键)。 即使键 0 在表中不存在， 这个方法也会\
返回最低 id 的位置 (最左边的叶子节点的开始)。 

.. code-block:: C 

    Cursor* table_start(Table* table)
    {
        Cursor* cursor = table_find(table, 0);

        void* node = get_page(table->pager, cursor->page_num);
        uint32_t num_cells = *leaf_node_num_cells(node);
        cursor->end_of_table = (num_cells == 0);

        return cursor;
    }

进行了这些更改后， 它仍然只打印出一个节点的行数： 

.. code-block:: bash

    db > select
    (1, user1, person1@example.com)
    (2, user2, person2@example.com)
    (3, user3, person3@example.com)
    (4, user4, person4@example.com)
    (5, user5, person5@example.com)
    (6, user6, person6@example.com)
    (7, user7, person7@example.com)
    Executed.
    db >

我们的 btree 有 15 个条目， 由一个内部节点和两个叶子节点组成， 看起来像这样： 

.. figure:: img/btree3.png 
    :align: center

    structure of our btree

要扫描整个表， 我们需要在到达第一个叶子节点末尾后跳到第二个叶子节点。 为此我们将在叶\
子节点标头中保存一个名为 ``next_leaf`` 的新字段， 该字段将在右侧保留叶子同级节点的页\
码。 最右边的叶子节点的 ``next_leaf`` 值为 0， 表示没有兄弟姐妹 (页面 0 始终为表的\
根节点保留)。

更新叶节点标头格式以包括新字段： 

.. code-block:: C

    const uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);
    const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;
    const uint32_t LEAF_NODE_NEXT_LEAF_SIZE = sizeof(uint32_t);
    const uint32_t LEAF_NODE_NEXT_LEAF_OFFSET = LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE;
    const uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE +
            LEAF_NODE_NUM_CELLS_SIZE +
            LEAF_NODE_NEXT_LEAF_SIZE;

添加方法以访问新字段： 

.. code-block:: C 

    uint32_t* leaf_node_next_leaf(void* node) 
    {
        return node + LEAF_NODE_NEXT_LEAF_OFFSET;
    }

初始化新的叶子节点时， 默认将 ``next_leaf`` 设置为 0： 

.. code-block:: C 

    void initialize_leaf_node(void* node)
    {
        set_node_type(node, NODE_LEAF);
        set_node_root(node, false);
        *leaf_node_num_cells(node) = 0;
        *leaf_node_next_leaf(node) = 0;
    }

每当我们拆分叶节点时， 都更新同级指针。 旧叶子的兄弟姐妹变成新叶子， 而新叶子的兄弟姐\
妹变成以前的旧叶子的兄弟姐妹。 

.. code-block:: C 

    void leaf_node_split_and_insert(Cursor* cursor, uint32_t key, Row* value)
    {
        /*
        * Create a new node and move half the cells over.
        * Insert the new value in one of the two nodes.
        * Update parent or create a new parent.
        */

        void *old_node = get_page(cursor->table->pager, cursor->page_num);
        uint32_t new_page_num = get_unused_page_num(cursor->table->pager);
        void *new_node = get_page(cursor->table->pager, new_page_num);
        initialize_leaf_node(new_node);
        *leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
        *leaf_node_next_leaf(old_node) = new_page_num;
        ...

添加一个新字段会更改一些常量： 

.. code-block:: ruby

    it 'prints constants' do
        script = [
            ".constants",
            ".exit",
        ]
        result = run_script(script)

        expect(result).to match_array([
            "db > Constants:",
            "ROW_SIZE: 293",
            "COMMON_NODE_HEADER_SIZE: 6",
            "LEAF_NODE_HEADER_SIZE: 14",
            "LEAF_NODE_CELL_SIZE: 297",
            "LEAF_NODE_SPACE_FOR_CELLS: 4082",
            "LEAF_NODE_MAX_CELLS: 13",
            "db > ",
        ])
    end

现在每当我们想把光标推进到一个叶子节点的末端时， 我们可以检查该叶子节点是否有一个兄弟\
姐妹。 如果有就跳到它。 否则我们就到了表的末端。 

.. code-block:: C 

    void* cursor_advance(Cursor* cursor)
    {
        uint32_t page_num = cursor->page_num;
        void* node = get_page(cursor->table->pager, page_num);
        cursor->cell_num += 1;
        if (cursor->cell_num >= (*leaf_node_num_cells(node)))
        {
            /* Advance to next leaf node */
            uint32_t next_page_num = *leaf_node_next_leaf(node);
            if (next_page_num == 0)
            {
                /* This was rightmost leaf */
                cursor->end_of_table = true;
            } else {
                cursor->page_num = next_page_num;
                cursor->cell_num = 0;
            }
        }
    }

更改之后， 我们实际上打印了 15 行 ...

.. code-block:: bash

    db > select
    (1, user1, person1@example.com)
    (2, user2, person2@example.com)
    (3, user3, person3@example.com)
    (4, user4, person4@example.com)
    (5, user5, person5@example.com)
    (6, user6, person6@example.com)
    (7, user7, person7@example.com)
    (8, user8, person8@example.com)
    (9, user9, person9@example.com)
    (10, user10, person10@example.com)
    (11, user11, person11@example.com)
    (12, user12, person12@example.com)
    (13, user13, person13@example.com)
    (1919251317, 14, on14@example.com)
    (15, user15, person15@example.com)
    Executed.
    db >

\... 但是其中一个看上去已损坏 

.. code-block:: ruby

    (1919251317, 14, on14@example.com)

经过一些调试后， 我发现这是由于我们拆分叶节点的方式存在错误所致： 

.. code-block:: C

    [leaf_node_split_and_insert(Cursor* cursor, uint32_t key, Row* value)]
    ...
    uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
    void *destination = leaf_node_cell(destination_node, index_within_node);

    if (i == cursor->cell_num)
    {
        serialize_row(value,
                        leaf_node_value(destination_node, index_within_node));
        *leaf_node_key(destination_node, index_within_node) = key;
    ...

请记住， 叶节点中的每个单元格首先包含一个键， 然后包含一个值： 

.. figure:: img/leaf-node-format.png 
    :align: center

    Original leaf node format

我们正在将新行 (值) 写入单元格的开头， 键应该放在该单元格的开头。 这意味着用户名的一\
部分进入了 ID 部分 (因此出现了特别大的 ID)。 

修复该错误之后， 我们最终按预期打印出了整个表： 

.. code-block:: bash 

    db > select
    (1, user1, person1@example.com)
    (2, user2, person2@example.com)
    (3, user3, person3@example.com)
    (4, user4, person4@example.com)
    (5, user5, person5@example.com)
    (6, user6, person6@example.com)
    (7, user7, person7@example.com)
    (8, user8, person8@example.com)
    (9, user9, person9@example.com)
    (10, user10, person10@example.com)
    (11, user11, person11@example.com)
    (12, user12, person12@example.com)
    (13, user13, person13@example.com)
    (14, user14, person14@example.com)
    (15, user15, person15@example.com)
    Executed.
    db >

呜！ 一个接一个的错误， 但我们正在取得进展。 

下一节再见。 

`这里[10]`_ 是本节所有的代码改动 。 

.. _这里[10]: https://github.com/iloeng/SimpleDB/commit/4f30d2b2cad9f91a1e28033b6d82788701acb0c2
