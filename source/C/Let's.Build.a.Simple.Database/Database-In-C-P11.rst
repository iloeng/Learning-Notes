*******************************************************************************
Part 11 - 递归搜索 B 型树
*******************************************************************************

.. contents:: 目录
    :depth: 3
    :backlinks: top

上一节， 我们在插入第 15 行时出错： 

.. code-block:: bash

    db > insert 15 user15 person15@example.com
    Need to implement searching an internal node

首先， 用新的函数调用替换代码:

.. code-block:: C 

    Cursor* table_find(Table* table, uint32_t key)
    {
        uint32_t root_page_num = table->root_page_num;
        void* root_node = get_page(table->pager, root_page_num);

        if (get_node_type(root_node) == NODE_LEAF)
        {
            return leaf_node_find(table, root_page_num, key);
        } else {
            return internal_node_find(table, root_page_num, key);
        }
    }

此函数将执行二进制搜索以查找应包含给定键的子级。 请记住每个子指针右边的键是该子指针包\
含的最大键。 

.. figure:: img/btree6.png 
    :align: center

    three-level btree

因此我们的二进制搜索会比较要查找的键和子指针右侧的键： 

.. code-block:: C 

    Cursor* internal_node_find(Table* table, uint32_t page_num, uint32_t key) 
    {
        void* node = get_page(table->pager, page_num);
        uint32_t num_keys = *internal_node_num_keys(node);

        /* Binary search to find index of child to search */
        uint32_t min_index = 0;
        uint32_t max_index = num_keys; /* there is one more child than key */

        while (min_index != max_index) 
        {
            uint32_t index = (min_index + max_index) / 2;
            uint32_t key_to_right = *internal_node_key(node, index);
            if (key_to_right >= key) 
            {
                max_index = index;
            } else {
                min_index = index + 1;
            }
        }
        
        uint32_t child_num = *internal_node_child(node, min_index);
        void* child = get_page(table->pager, child_num);
        switch (get_node_type(child)) 
        {
            case NODE_LEAF:
                return leaf_node_find(table, child_num, key);
            case NODE_INTERNAL:
                return internal_node_find(table, child_num, key);
        }
    }

还要记住， 内部节点的子节点可以是叶节点， 也可以是更多内部节点。 找到正确的子节点后\
， 在其上调用适当的搜索功能。

11.1 Tests
===============================================================================

现在插入一个 Key 到多节点 btree 不再导致错误。 我们可以更新测试： 

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
        "db > Executed.",
        "db > ",
        ])
    end

我也认为是时候重新进行另一项测试了。 尝试插入 1400 行的程序。 它仍然会出错， 但是错误\
消息是新的。 目前程序崩溃时， 我们的测试不能很好地处理它。 如果发生这种情况， 请仅使\
用到目前为止的输出： 

.. code-block:: ruby

    def run_script(commands)
        raw_output = nil
        IO.popen("./db test.db", "r+") do |pipe|
        commands.each do |command|
            begin
            pipe.puts command
            rescue Errno::EPIPE
            break
            end
        end

        pipe.close_write

        # Read entire output
        raw_output = pipe.gets(nil)
        end
        raw_output.split("\n")
    end

同时我们的 1400 行测试输出此错误 ： 

.. code-block:: ruby

    it 'prints error message when table is full' do
        script = (1..1401).map do |i|
        "insert #{i} user#{i} person#{i}@example.com"
        end
        script << ".exit"
        result = run_script(script)
        expect(result.last(2)).to match_array([
        "db > Executed.",
        "db > Need to implement updating parent after split",
        ])
    end

看来这就是我们的待办事项清单上的下一个！ 

`这里[9]`_ 是本节所有的代码改动。 

.. _`这里[9]`: https://github.com/iloeng/SimpleDB/commit/fba4f611f3da1e8688dcc8e5bd1b8205c0a67327
