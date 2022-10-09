##############################################################################
C 语言从头写一个 SQLite 程序
##############################################################################

.. contents::

******************************************************************************
第 12 部分  扫描多层级的 B 型树
******************************************************************************

并添加一个新的实现 ， 搜索键 0 (可能的最小键) 。 即使键 0 在表中不存在 ， 这个方法\
也会返回最低 id 的位置 (最左边的叶子节点的开始) 。 

.. code-block:: C 

    Cursor* table_start(Table* table)
    {
        Cursor* cursor = table_find(table, 0);

        void* node = get_page(table->pager, cursor->page_num);
        uint32_t num_cells = *leaf_node_num_cells(node);
        cursor->end_of_table = (num_cells == 0);

        return cursor;
    }

进行了这些更改后 ， 它仍然只打印出一个节点的行数 ： 

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

我们的 btree 有 15 个条目 ， 由一个内部节点和两个叶子节点组成 ， 看起来像这样 ： 

.. image:: img/btree3.png 

structure of our btree

要扫描整个表 ， 我们需要在到达第一个叶子节点末尾后跳到第二个叶子节点 。 为此 ， 我们\
将在叶子节点标头中保存一个名为 "next_leaf" 的新字段 ， 该字段将在右侧保留叶子同级节\
点的页码 。 最右边的叶子节点的 next_leaf 值为 0 ， 表示没有兄弟姐妹 (页面 0 始终为\
表的根节点保留) 。

更新叶节点标头格式以包括新字段 ： 

.. code-block:: C

    const uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);
    const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;
    const uint32_t LEAF_NODE_NEXT_LEAF_SIZE = sizeof(uint32_t);
    const uint32_t LEAF_NODE_NEXT_LEAF_OFFSET = LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE;
    const uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE +
            LEAF_NODE_NUM_CELLS_SIZE +
            LEAF_NODE_NEXT_LEAF_SIZE;

添加方法以访问新字段 ： 

.. code-block:: C 

    uint32_t* leaf_node_next_leaf(void* node) 
    {
        return node + LEAF_NODE_NEXT_LEAF_OFFSET;
    }

初始化新的叶子节点时 ， 默认将 ``next_leaf`` 设置为 0 ： 

.. code-block:: C 

    void initialize_leaf_node(void* node)
    {
        set_node_type(node, NODE_LEAF);
        set_node_root(node, false);
        *leaf_node_num_cells(node) = 0;
        *leaf_node_next_leaf(node) = 0;
    }

每当我们拆分叶节点时 ， 都更新同级指针 。 旧叶子的兄弟姐妹变成新叶子 ， 而新叶子的兄\
弟姐妹变成以前的旧叶子的兄弟姐妹 。 

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

添加一个新字段会更改一些常量 ： 

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

现在 ， 每当我们想把光标推进到一个叶子节点的末端时 ， 我们可以检查该叶子节点是否有一\
个兄弟姐妹 。 如果有 ， 就跳到它 。 否则 ， 我们就到了表的末端 。 

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

更改之后 ， 我们实际上打印了 15 行 ...

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

... 但是其中一个看上去已损坏 

.. code-block:: ruby

    (1919251317, 14, on14@example.com)

经过一些调试后 ， 我发现这是由于我们拆分叶节点的方式存在错误所致 ： 

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

请记住 ， 叶节点中的每个单元格首先包含一个键 ， 然后包含一个值 ： 

.. image:: img/leaf-node-format.png 

Original leaf node format

我们正在将新行 (值) 写入单元格的开头 ， 键应该放在该单元格的开头 。 这意味着用户名的\
一部分进入了 ID 部分 (因此出现了特别大的 ID) 。 

修复该错误之后 ， 我们最终按预期打印出了整个表 ： 

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

呜！ 一个接一个的错误 ， 但我们正在取得进展 。 

下一节再见 。 

这里_ 是本节所有的代码改动 。 

.. _这里: https://github.com/Deteriorator/SimpleDB/commit/4f30d2b2cad9f91a1e28033b6d82788701acb0c2

******************************************************************************
第 13 部分  分割后更新父节点
******************************************************************************

对于我们史诗般的 b 树实现过程的下一步 ， 我们将在拆分叶子之后处理修复父节点的问题 。 \
我将使用以下示例作为参考 ： 

.. image:: img/updating-internal-node.png  

Example of updating internal node

在这个例子中 ， 我们将 Key "3" 添加到树中 。 这导致左叶节点分裂 。 拆分后 ， 我们通\
过执行以下操作来修复树 ： 

1. 将父级中的第一个键更新为左子级中的最大键 ("3")
2. 在更新的 Key 之后添加新的子指针/键对

    - 新指针指向新的子节点
    - 新 Key 是新子节点中的最大密钥 ("5")

所以首先要做的是 ， 用两个新的函数调用替换我们的代码 ： 步骤 1 的 \
``update_internal_node_key()`` 和步骤 2 的 ``internal_node_insert()`` 

.. code-block:: C 

    void leaf_node_split_and_insert(Cursor* cursor, uint32_t key, Row* value)
    {
        /*
        * Create a new node and move half the cells over.
        * Insert the new value in one of the two nodes.
        * Update parent or create a new parent.
        */

        void *old_node = get_page(cursor->table->pager, cursor->page_num);
        uint32_t old_max = get_node_max_key(old_node);
        uint32_t new_page_num = get_unused_page_num(cursor->table->pager);
        void *new_node = get_page(cursor->table->pager, new_page_num);
        initialize_leaf_node(new_node);
        *node_parent(new_node) = *node_parent(old_node);
        *leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
        *leaf_node_next_leaf(old_node) = new_page_num;

        /*
        * All existing keys plus new key should be divided
        * evenly between old (left) and new (right) nodes.
        * Starting from the right, move each key to correct position.
        */
        for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--)
        {
            void *destination_node;
            if (i >= LEAF_NODE_LEFT_SPLIT_COUNT)
            {
                destination_node = new_node;
            } else {
                destination_node = old_node;
            }
            uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
            void *destination = leaf_node_cell(destination_node, index_within_node);

            if (i == cursor->cell_num)
            {
                serialize_row(value,
                            leaf_node_value(destination_node, index_within_node));
                *leaf_node_key(destination_node, index_within_node) = key;
            } else if (i > cursor->cell_num)
            {
                memcpy(destination, leaf_node_cell(old_node, i - 1), LEAF_NODE_CELL_SIZE);
            } else {
                memcpy(destination, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
            }
        }

        /* Update cell count on both leaf nodes */
        *(leaf_node_num_cells(old_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;
        *(leaf_node_num_cells(new_node)) = LEAF_NODE_RIGHT_SPLIT_COUNT;

        if (is_node_root(old_node))
        {
            return create_new_root(cursor->table, new_page_num);
        } else {
            uint32_t parent_page_num = *node_parent(old_node);
            uint32_t new_max = get_node_max_key(old_node);
            void* parent = get_page(cursor->table->pager, parent_page_num);

            update_internal_node_key(parent, old_max, new_max);
            internal_node_insert(cursor->table, parent_page_num, new_page_num);
            return;
        }
    }

为了获得对父节点的引用 ， 我们需要在每个节点中开始记录指向其父节点的指针 。 

.. code-block:: C  

    uint32_t* node_parent(void* node) 
    { 
        return node + PARENT_POINTER_OFFSET; 
    }

    void create_new_root(Table* table, uint32_t right_child_page_num)
    {
        /*
        * Handle splitting the root.
        * Old root copied to new page, becomes left child.
        * Address of right child passed in.
        * Re-initialize root page to contain the new root node.
        * New root node points to two children.
        */

        void* root = get_page(table->pager, table->root_page_num);
        void* right_child = get_page(table->pager, right_child_page_num);
        uint32_t left_child_page_num = get_unused_page_num(table->pager);
        void* left_child = get_page(table->pager, left_child_page_num);

        /* Left child has data copied from old root */
        memcpy(left_child, root, PAGE_SIZE);
        set_node_root(left_child, false);

        /* Root node is a new internal node with one key and two children */
        initialize_internal_node(root);
        set_node_root(root, true);
        *internal_node_num_keys(root) = 1;
        *internal_node_child(root, 0) = left_child_page_num;
        uint32_t left_child_max_key = get_node_max_key(left_child);
        *internal_node_key(root, 0) = left_child_max_key;
        *internal_node_right_child(root) = right_child_page_num;
        *node_parent(left_child) = table->root_page_num;
        *node_parent(right_child) = table->root_page_num;
    }

现在我们需要在父节点中找到受影响的单元 。 这个子节点不知道自己的页码 ， 因此我们无法\
找到该页码 。 但是它确实知道自己的最大 Key ， 因此我们可以在父级中搜索该 Key 。

.. code-block:: C

    void update_internal_node_key(void* node, uint32_t old_key, uint32_t new_key)
    {
        uint32_t old_child_index = internal_node_find_child(node, old_key);
        *internal_node_key(node, old_child_index) = new_key;
    }

在 ``internal_node_find_child()`` 内部 ， 我们将重用一些已经在内部节点中查找密钥的\
代码 。 重构 ``internal_node_find()`` 以使用新的帮助器方法 。 

.. code-block:: C 

    uint32_t internal_node_find_child(void* node, uint32_t key)
    {
        /*Return the index of the child which should contain
        * the given key.
        */

        uint32_t num_keys = *internal_node_num_keys(node);

        /* Binary search */
        uint32_t min_index = 0;
        uint32_t max_index = num_keys; /* there is one more child than key */

        while (min_index != max_index)
        {
            uint32_t index = (min_index + max_index) / 2;
            uint32_t key_to_right = *internal_node_key(node, index);
            if (key_to_right >= key) {
                max_index = index;
            } else {
                min_index = index + 1;
            }
        }

        return min_index;
    }

    Cursor* internal_node_find(Table* table, uint32_t page_num, uint32_t key)
    {
        void* node = get_page(table->pager, page_num);

        uint32_t child_index = internal_node_find_child(node, key);
        uint32_t child_num = *internal_node_child(node, child_index);
        void* child = get_page(table->pager, child_num);
        switch (get_node_type(child))
        {
            case NODE_LEAF:
                return leaf_node_find(table, child_num, key);
            case NODE_INTERNAL:
                return internal_node_find(table, child_num, key);
        }
    }

现在我们开始了解本文的核心 ， 实现 ``internal_node_insert()`` 。 我会逐个解释 。 


.. code-block:: C 

    void internal_node_insert(Table* table, uint32_t parent_page_num, uint32_t child_page_num) {

        /* Add a new child/key pair to parent that corresponds to child
        */

        void* parent = get_page(table->pager, parent_page_num);
        void* child = get_page(table->pager, child_page_num);
        uint32_t child_max_key = get_node_max_key(child);
        uint32_t index = internal_node_find_child(parent, child_max_key);

        uint32_t original_num_keys = *internal_node_num_keys(parent);
        *internal_node_num_keys(parent) = original_num_keys + 1;

        if (original_num_keys >= INTERNAL_NODE_MAX_CELLS)
        {
            printf("Need to implement splitting internal node\n");
            exit(EXIT_FAILURE);
        }

        uint32_t right_child_page_num = *internal_node_right_child(parent);
        void* right_child = get_page(table->pager, right_child_page_num);

        if (child_max_key > get_node_max_key(right_child)) {
            /* Replace right child */
            *internal_node_child(parent, original_num_keys) = right_child_page_num;
            *internal_node_key(parent, original_num_keys) =
                    get_node_max_key(right_child);
            *internal_node_right_child(parent) = child_page_num;
        } else {
            /* Make room for the new cell */
            for (uint32_t i = original_num_keys; i > index; i--) {
                void* destination = internal_node_cell(parent, i);
                void* source = internal_node_cell(parent, i - 1);
                memcpy(destination, source, INTERNAL_NODE_CELL_SIZE);
            }
            *internal_node_child(parent, index) = child_page_num;
            *internal_node_key(parent, index) = child_max_key;
        }
    }

应在其中插入新单元格 (子项/ Key 对) 的索引取决于新子项中的最大 Key 。 在我们查看的示\
例中 ， child_max_key 将为 5 ，索引将为 1 。 

如果内部节点中没有其他单元的空间 ， 则抛出错误 。 我们稍后会实现 。

现在让我们看一下其余的功能 。 

因为我们将最右边的子节点指针与其余的子节点 / Key 对分开存储 ， 所以如果新的孩子要成\
为最右边的子节点 ， 我们必须以不同的方式处理事情 。 

在我们的示例中 ， 我们将进入 else 代码块 。 首先 ， 我们通过将其他单元格向右移动一个\
空间为新单元格腾出空间 。 (尽管在我们的示例中 ， 有 0 个要移动的单元格)

接下来 ， 我们将新的子指针和键写入由索引确定的单元格中 。 

为了减少所需测试用例的大小 ， 我现在对 INTERNAL_NODE_MAX_CELLS 进行硬编码 :

.. code-block:: C 

    /* Keep this small for testing */
    const uint32_t INTERNAL_NODE_MAX_CELLS = 3;

说到测试 ， 我们的大数据集测试通过了旧的代码 ， 并进入了新的代码 ： 

.. code-block:: ruby

  it 'prints error message when table is full' do
    script = (1..1401).map do |i|
      "insert #{i} user#{i} person#{i}@example.com"
    end
    script << ".exit"
    result = run_script(script)
    expect(result.last(2)).to match_array([
      "db > Executed.",
      "db > Need to implement splitting internal node",
    ])
  end

未完待续 ...

上一篇文章 ： `上一篇`_

下一篇文章 ： `下一篇`_ 

.. _`上一篇`: Database-In-C-07.rst
.. _`下一篇`: Database-In-C-09.rst
