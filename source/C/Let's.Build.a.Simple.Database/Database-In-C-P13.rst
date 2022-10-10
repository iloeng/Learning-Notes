*******************************************************************************
Part 13 - 分割后更新父节点
*******************************************************************************

.. contents:: 目录
    :depth: 3
    :backlinks: top

对于我们史诗般的 b 树实现过程的下一步， 我们将在拆分叶子之后处理修复父节点的问题。 我\
将使用以下示例作为参考： 

.. figure:: img/updating-internal-node.png  
    :align: center

    Example of updating internal node

在这个例子中， 我们将 Key "3" 添加到树中。 这导致左叶节点分裂。 拆分后， 我们通过执\
行以下操作来修复树： 

1. 将父级中的第一个键更新为左子级中的最大键 ("3")
2. 在更新的 Key 之后添加新的子指针 / 键对

   - 新指针指向新的子节点
   - 新 Key 是新子节点中的最大密钥 ("5")

所以首先要做的是， 用两个新的函数调用替换我们的代码： 步骤 1 的 \
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

为了获得对父节点的引用， 我们需要在每个节点中开始记录指向其父节点的指针。 

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

现在我们需要在父节点中找到受影响的单元。 这个子节点不知道自己的页码， 因此我们无法找\
到该页码。 但是它确实知道自己的最大 Key， 因此我们可以在父级中搜索该 Key。

.. code-block:: C

    void update_internal_node_key(void* node, uint32_t old_key, uint32_t new_key)
    {
        uint32_t old_child_index = internal_node_find_child(node, old_key);
        *internal_node_key(node, old_child_index) = new_key;
    }

在 ``internal_node_find_child()`` 内部， 我们将重用一些已经在内部节点中查找密钥的\
代码。 重构 ``internal_node_find()`` 以使用新的帮助器方法。 

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

现在我们开始了解本文的核心， 实现 ``internal_node_insert()``。 我会逐个解释。 


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

应在其中插入新单元格 (子项 / Key 对) 的索引取决于新子项中的最大 Key。 在我们查看的示\
例中， ``child_max_key`` 将为 5，索引将为 1。 

如果内部节点中没有其他单元的空间， 则抛出错误。 我们稍后会实现。

现在让我们看一下其余的功能。 

因为我们将最右边的子节点指针与其余的子节点 / Key 对分开存储， 所以如果新的孩子要成为\
最右边的子节点， 我们必须以不同的方式处理事情。 

在我们的示例中， 我们将进入 else 代码块。 首先我们通过将其他单元格向右移动一个空间为\
新单元格腾出空间。 (尽管在我们的示例中， 有 0 个要移动的单元格)

接下来， 我们将新的子指针和键写入由索引确定的单元格中。 

为了减少所需测试用例的大小， 我现在对 ``INTERNAL_NODE_MAX_CELLS`` 进行硬编码:

.. code-block:: C 

    /* Keep this small for testing */
    const uint32_t INTERNAL_NODE_MAX_CELLS = 3;

说到测试， 我们的大数据集测试通过了旧的代码， 并进入了新的代码： 

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

我很满意。 

我将添加另一个测试， 该测试将打印四节点树。 只是为了让我们测试更多的案例而不是顺序的 \
ID， 此测试将以伪随机顺序添加记录。 

.. code-block:: ruby

    it 'allows printing out the structure of a 4-leaf-node btree' do
        script = [
            "insert 18 user18 person18@example.com",
            "insert 7 user7 person7@example.com",
            "insert 10 user10 person10@example.com",
            "insert 29 user29 person29@example.com",
            "insert 23 user23 person23@example.com",
            "insert 4 user4 person4@example.com",
            "insert 14 user14 person14@example.com",
            "insert 30 user30 person30@example.com",
            "insert 15 user15 person15@example.com",
            "insert 26 user26 person26@example.com",
            "insert 22 user22 person22@example.com",
            "insert 19 user19 person19@example.com",
            "insert 2 user2 person2@example.com",
            "insert 1 user1 person1@example.com",
            "insert 21 user21 person21@example.com",
            "insert 11 user11 person11@example.com",
            "insert 6 user6 person6@example.com",
            "insert 20 user20 person20@example.com",
            "insert 5 user5 person5@example.com",
            "insert 8 user8 person8@example.com",
            "insert 9 user9 person9@example.com",
            "insert 3 user3 person3@example.com",
            "insert 12 user12 person12@example.com",
            "insert 27 user27 person27@example.com",
            "insert 17 user17 person17@example.com",
            "insert 16 user16 person16@example.com",
            "insert 13 user13 person13@example.com",
            "insert 24 user24 person24@example.com",
            "insert 25 user25 person25@example.com",
            "insert 28 user28 person28@example.com",
            ".btree",
            ".exit",
        ]
        result = run_script(script)

照原样， 它将输出以下内容： 

.. code-block:: bash

    - internal (size 3)
    - leaf (size 7)
        - 1
        - 2
        - 3
        - 4
        - 5
        - 6
        - 7
    - key 1
    - leaf (size 8)
        - 8
        - 9
        - 10
        - 11
        - 12
        - 13
        - 14
        - 15
    - key 15
    - leaf (size 7)
        - 16
        - 17
        - 18
        - 19
        - 20
        - 21
        - 22
    - key 22
    - leaf (size 8)
        - 23
        - 24
        - 25
        - 26
        - 27
        - 28
        - 29
        - 30
    db >
 
仔细看， 您会发现一个错误： 

.. code-block:: bash 

    - 5
    - 6
    - 7
  - key 1

Key 应该是 7， 而不是 1！

经过一堆调试， 我发现这是由于一些错误的指针算法造成的。 

``INTERNAL_NODE_CHILD_SIZE`` 为 4。 我的目的是在 ``internal_node_cell()`` 的结果\
中添加 4 个字节， 但是由于 ``internal_node_cell()`` 返回的是 ``uint32_t *``， 因\
此实际上是在添加 ``4 * sizeof(uint32_t)`` 个字节。 在进行算术运算之前， 我通过将其\
强制转换为 ``void *`` 来对其进行了修复。 

注意！ 空指针的指针算术不是 C 标准的一部分， 可能无法与您的编译器一起使用。 以后我可\
能会写一篇关于可移植性的文章， 但现在暂时不做空指针算法。 

好了， 向全面运行 btree 的实现又迈出了一步。 下一步应该是分割内部节点。 直到那时！

`这里[11]`_ 是本节所有的代码改动。 

.. _这里[11]: https://github.com/iloeng/SimpleDB/commit/95b97742f6c1883eba0a09b3fdb0dbd2109b5f85
