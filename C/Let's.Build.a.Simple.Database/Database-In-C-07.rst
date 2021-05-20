##############################################################################
C 语言从头写一个 SQLite 程序
##############################################################################

.. contents::

******************************************************************************
第 10 部分  分割叶子节点
******************************************************************************

10.5 内部节点格式
==============================================================================

对于内部节点 ， 最大键始终是其右键 。 对于叶节点 ， 这是最大索引处的关键 ： 

.. code-block:: C 

    uint32_t get_node_max_key(void* node) {
        switch (get_node_type(node))
        {
            case NODE_INTERNAL:
                return *internal_node_key(node, *internal_node_num_keys(node) - 1);
            case NODE_LEAF:
                return *leaf_node_key(node, *leaf_node_num_cells(node) - 1);
        }
    }

10.6 追踪根源
==============================================================================

我们最后使用普通节点头中的 ``is_root`` 字段 。 回顾一下 ， 我们用它来决定如何分割一\
个叶子节点 : 

.. code-block:: C 

    [leaf_node_split_and_insert(Cursor* cursor, uint32_t key, Row* value)]
    if (is_node_root(old_node))
    {
        return create_new_root(cursor->table, new_page_num);
    } else {
        printf("Need to implement updating parent after split\n");
        exit(EXIT_FAILURE);
    }

这是 ``getter`` 和 ``setter`` ：

.. code-block:: C 

    bool is_node_root(void* node) 
    {
        uint8_t value = *((uint8_t*)(node + IS_ROOT_OFFSET));
        return (bool)value;
    }

    void set_node_root(void* node, bool is_root) 
    {
        uint8_t value = is_root;
        *((uint8_t*)(node + IS_ROOT_OFFSET)) = value;
    }

初始化这两种类型的节点是应该默认将 ``is_root`` 设置为 false :

.. code-block:: C 

    void initialize_leaf_node(void* node)
    {
        set_node_type(node, NODE_LEAF);
        set_node_root(node, false);
        *leaf_node_num_cells(node) = 0;
    }

    void initialize_internal_node(void* node) 
    {
        set_node_type(node, NODE_INTERNAL);
        set_node_root(node, false);
        *internal_node_num_keys(node) = 0;
    }

我们应该在创建表的第一个节点时将 is_root 设置为 true :

.. code-block:: C 

    Table* db_open(const char* filename)
    {
        Pager* pager = pager_open(filename);

        Table* table = malloc(sizeof(Table));
        table->pager = pager;
        table->root_page_num = 0;

        if (pager->num_pages == 0)
        {
            // New database file. Initialize page 0 as leaf node.
            void* root_node = get_page(pager, 0);
            initialize_leaf_node(root_node);
            set_node_root(root_node, true);
        }

        return table;
    }

10.7 打印树
==============================================================================

为了帮助我们可视化数据库的状态 ， 我们应该更新 ``.btree`` 元指令以打印多级树 。 

我将替换当前的 ``print_leaf_node()`` 函数 。

一个新的递归函数 ， 该函数接受任何节点 ， 然后打印该节点及其子节点 。 它以缩进级别作\
为参数 ， 每次递归调用时都会增加 。 我还添加了一个小的辅助函数来缩进 。 

.. code-block:: C 

    void indent(uint32_t level) 
    {
        for (uint32_t i = 0; i < level; i++) 
        {
            printf("  ");
        }
    }

    void print_tree(Pager* pager, uint32_t page_num, uint32_t indentation_level) 
    {
        void* node = get_page(pager, page_num);
        uint32_t num_keys, child;
        
        switch (get_node_type(node)) 
        {
            case (NODE_LEAF):
                num_keys = *leaf_node_num_cells(node);
                indent(indentation_level);
                printf("- leaf (size %d)\n", num_keys);
                for (uint32_t i = 0; i < num_keys; i++)
                {
                    indent(indentation_level + 1);
                    printf("- %d\n", *leaf_node_key(node, i));
                }
                break;
            case (NODE_INTERNAL):
                num_keys = *internal_node_num_keys(node);
                indent(indentation_level);
                printf("- internal (size %d)\n", num_keys);
                for (uint32_t i = 0; i < num_keys; i++) 
                {
                    child = *internal_node_child(node, i);
                    print_tree(pager, child, indentation_level + 1);
            
                    indent(indentation_level + 1);
                    printf("- key %d\n", *internal_node_key(node, i));
                }
                child = *internal_node_right_child(node);
                print_tree(pager, child, indentation_level + 1);
                break;
        }
    }

并更新对打印函数的调用 ， 缩进级别为零 。

.. code-block:: C

    else if(strcmp(input_buffer->buffer, ".btree") == 0){
            printf("Tree:\n");
            print_tree(table->pager, 0, 0);
            return META_COMMAND_SUCCESS;
        }

这是新打印功能的测试用例 ！ 

.. code-block:: ruby

    it 'allows printing out the structure of a 3-leaf-node btree' do
        script = (1..14).map do |i|
        "insert #{i} user#{i} person#{i}@example.com"
        end
        script << ".btree"
        script << "insert 15 user15 person15@example.com"
        script << ".exit"
        result = run_script(script)
    
        expect(result[14...(result.length)]).to match_array([
        "db > Tree:",
        "- internal (size 1)",
        "  - leaf (size 7)",
        "    - 1",
        "    - 2",
        "    - 3",
        "    - 4",
        "    - 5",
        "    - 6",
        "    - 7",
        "  - key 7",
        "  - leaf (size 7)",
        "    - 8",
        "    - 9",
        "    - 10",
        "    - 11",
        "    - 12",
        "    - 13",
        "    - 14",
        "db > Need to implement searching an internal node",
        ])
    end

新格式有所简化 ， 因此我们需要更新现有的 ``.btree`` 测试 ： 

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
        "- leaf (size 3)",
        "  - 1",
        "  - 2",
        "  - 3",
        "db > "
        ])
    end

这是新测试本身的 ``.btree`` 输出 : 

.. code-block:: 

    Tree:
    - internal (size 1)
        - leaf (size 7)
            - 1
            - 2
            - 3
            - 4
            - 5
            - 6
            - 7
        - key 7
        - leaf (size 7)
            - 8
            - 9
            - 10
            - 11
            - 12
            - 13
            - 14

在最小缩进级别上 ， 我们看到根节点 （内部节点） 。 之所以说是 1 号 ， 是因为它有一\
个 key 。 缩进一个级别 ， 我们看到一个叶节点 ， 一个键和另一个叶节点 。 根节点 (7) \
中的密钥是第一个叶节点中的最大密钥 。 每个大于 7 的键都在第二个叶子节点中 。 

10.8 严重问题
==============================================================================

如果你一直在密切关注 ， 你可能会发现我们错过了一些重要的东西 。 看看如果我们尝试插入\
一个额外的行会发生什么 :

.. code-block:: bash

    db > insert 15 user15 person15@example.com
    Need to implement searching an internal node

哎呀 ！ 谁写了那个 TODO 消息 ? :P 

接下来 ， 我们将通过在多级树上执行搜索来继续史诗般的 B 树传奇 。 

这里_ 是本节代码所有的改动 。 

.. _这里: https://github.com/Deteriorator/SimpleDB/commit/6144de6401b24b7848fdd8fe865379c663e241cb

******************************************************************************
第 11 部分  递归搜索 B 型树
******************************************************************************















未完待续 ...

上一篇文章 ： `上一篇`_

下一篇文章 ： `下一篇`_ 

.. _`上一篇`: Database-In-C-05.rst
.. _`下一篇`: Database-In-C-07.rst
