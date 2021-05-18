##############################################################################
C 语言从头写一个 SQLite 程序
##############################################################################

.. contents::

******************************************************************************
第 08 部分  B 型树叶子节点格式
******************************************************************************

8.4 Pager 和 Table 对象的变化
==============================================================================

每一个节点都将正好占用一个页面 ， 即使它不是满的 。 这意味着我们的 Pager 不再需要支\
持读 / 写部分页面 。 

.. code-block:: C 

    void pager_flush(Pager* pager, uint32_t page_num)
    {
        if (pager->pages[page_num] == NULL)
        {
            printf("Tried to flush null page\n");
            exit(EXIT_FAILURE);
        }
        off_t offset = lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
        if (offset == -1)
        {
            printf("Error seeking: %d\n", errno);
            exit(EXIT_FAILURE);
        }
        ssize_t bytes_written = write(pager->file_descriptor, pager->pages[page_num], PAGE_SIZE);
        if (bytes_written == -1)
        {
            printf("Error writing: %d\n", errno);
            exit(EXIT_FAILURE);
        }
    }

    void db_close(Table* table)
    {
        Pager* pager = table->pager;
        for (uint32_t i = 0; i < pager->num_pages; i++)
        {
            if (pager->pages[i] == NULL)
            {
                    continue;
            }
            pager_flush(pager, i);
            free(pager->pages[i]);
            pager->pages[i] = NULL;
        }

        int result = close(pager->file_descriptor);
        if (result == -1)
        {
            printf("Error closing db file.\n");
            exit(EXIT_FAILURE);
        }
        for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
        {
            void* page = pager->pages[i];
            if (page)
            {
                free(page);
                pager->pages[i] = NULL;
            }
        }
        free(pager);
        free(table);
    }

现在在我们的数据库中存储页数比存储行数更有意义 。 页数应该与 pager 对象相关联 ， 而\
不是与表相关联 ， 因为它是数据库使用的页数 ， 而不是一个特定的表 。 一个 btree 是由\
它的根节点的页数来识别的 ， 所以表对象需要跟踪它 。 

.. code-block:: C 

    const uint32_t PAGE_SIZE = 4096;
    const uint32_t TABLE_MAX_PAGES = 100;

    typedef struct
    {
        int file_descriptor;
        uint32_t file_length;
        uint32_t num_pages;
        void* pages[TABLE_MAX_PAGES];
    } Pager;

    typedef struct
    {
        Pager* pager;
        uint32_t root_page_num;
    } Table;

    void* get_page(Pager* pager, uint32_t page_num)
    {
        if (page_num > TABLE_MAX_PAGES)
        {
            printf("Tried to fetch page number out of bounds. %d > %d\n",
                    page_num, TABLE_MAX_PAGES);
            exit(EXIT_FAILURE);
        }

        if (pager->pages[page_num] == NULL)
        {
            // Cache miss. Allocate memory and load from file.
            void* page = malloc(PAGE_SIZE);
            uint32_t  num_pages = pager->file_length / PAGE_SIZE;

            // We might save a partial page at the end of the file
            if (pager->file_length % PAGE_SIZE)
            {
                num_pages += 1;
            }

            if (page_num <= num_pages)
            {
                lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
                ssize_t bytes_read = read(pager->file_descriptor, page, PAGE_SIZE);
                if (bytes_read == -1)
                {
                    printf("Error reading file: %d\n", errno);
                    exit(EXIT_FAILURE);
                }
            }
            pager->pages[page_num] = page;

            if (page_num >= pager->num_pages)
            {
                pager->num_pages = page_num + 1;
            }
        }
        return pager->pages[page_num];
    }

    Pager* pager_open(const char* filename)
    {
        int fd = open(filename,
                O_RDWR |      // Read/Write mode
                O_CREAT,  // Create file if it does not exist
                S_IWUSR |     // User write permission
                S_IRUSR   // User read permission
                );

        if (fd == -1)
        {
            printf("Unable to open file\n");
            exit(EXIT_FAILURE);
        }

        off_t file_length = lseek(fd, 0, SEEK_END);

        Pager* pager = malloc(sizeof(Pager));
        pager->file_descriptor = fd;
        pager->file_length = file_length;
        pager->num_pages = (file_length / PAGE_SIZE);

        if (file_length % PAGE_SIZE !=0)
        {
            printf("Db file is not a whole number of pages. Corrupt file.\n");
            exit(EXIT_FAILURE);
        }

        for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
        {
            pager->pages[i] = NULL;
        }
        return pager;
    }

8.5 Cursor 对象的变化
==============================================================================

一个游标代表了表中的一个位置 。 当我们的表是一个简单的行数组时 ， 我们可以通过行号来\
访问一个行 。 现在它是一棵树 ， 我们通过节点的页码和该节点中的单元格编号来确定一个位\
置 。 

.. code-block:: C 

    typedef struct
    {
        Table* table;
        uint32_t page_num;
        uint32_t cell_num;
        bool end_of_table; // Indicates a position one past the last element
    } Cursor;

    Cursor* table_start(Table* table)
    {
        Cursor* cursor = malloc(sizeof(Cursor));
        cursor->table = table;
        cursor->page_num = table->root_page_num;
        cursor->cell_num = 0;

        void* root_node = get_page(table->pager, table->root_page_num);
        uint32_t num_cells = *leaf_node_num_cells(root_node);
        cursor->end_of_table = (num_cells == 0);

        return cursor;
    }

    Cursor* table_end(Table* table)
    {
        Cursor* cursor = malloc(sizeof(Cursor));
        cursor->table = table;
        cursor->page_num = table->root_page_num;

        void* root_node = get_page(table->pager, table->root_page_num);
        uint32_t num_cells = *leaf_node_num_cells(root_node);
        cursor->cell_num = num_cells;
        cursor->end_of_table = true;

        return cursor;
    }

    void* cursor_value(Cursor* cursor)
    {
        uint32_t page_num = cursor->page_num;
        void* page = get_page(cursor->table->pager, page_num);
        return leaf_node_value(page, cursor->cell_num);
    }

    void* cursor_advance(Cursor* cursor)
    {
        uint32_t page_num = cursor->page_num;
        void* node = get_page(cursor->table->pager, page_num);
        cursor->cell_num += 1;
        if (cursor->cell_num >= (*leaf_node_num_cells(node)))
        {
            cursor->end_of_table = true;
        }
    }

8.6 插入叶子节点
==============================================================================

在这篇文章中 ， 我们只打算实现足以得到一个单节点的树 。 回顾一下上一篇文章 ， 树开始\
时是一个空的叶子节点 :

.. image:: img/btree1.png 

empty btree

键 / 值对可以被添加 ， 直到叶子节点被填满 :

.. image:: img/btree2.png 

one-node btree

当我们第一次打开数据库时 ， 数据库文件将是空的 ， 所以我们将第 0 页初始化为一个空的\
叶节点 (根节点) :

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
        }

        return table;
    }

接下来我们将制作一个函数 ， 用于将键 / 值对插入到叶子节点中 。 它将接受一个光标作为\
输入 ， 以表示这对键值应被插入的位置 。 

.. code-block:: C 

    void leaf_node_insert(Cursor* cursor, uint32_t key, Row* value)
    {
        void* node = get_page(cursor->table->pager, cursor->page_num);
        uint32_t num_cells = *leaf_node_num_cells(node);
        if (num_cells >= LEAF_NODE_MAX_CELLS)
        {
            // Node full
            printf("Need to implement splitting a leaf node.\n");
            exit(EXIT_FAILURE);
        }
        if (cursor->cell_num < num_cells)
        {
            // Make room for new cell
            for (uint32_t i = num_cells; i > cursor->cell_num; i--)
            {
                memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1),LEAF_NODE_CELL_SIZE);
            }
        }
        *(leaf_node_num_cells(node)) += 1;
        *(leaf_node_key(node, cursor->cell_num)) = key;
        serialize_row(value, leaf_node_value(node, cursor->cell_num));
    }

我们还没有实现拆分 ， 所以如果节点已满会出错 。 接下来我们将单元格向右移动一个空格 ， \
为新的单元格腾出空间 。 然后我们把新的键 / 值写进空位 。 

由于我们假设树只有一个节点 ， 我们的 ``execute_insert()`` 函数只需要调用这个辅助方\
法 :

.. code-block:: C 

    ExecuteResult execute_insert(Statement* statement, Table* table)
    {
        void* node = get_page(table->pager, table->root_page_num);
        if ((*leaf_node_num_cells(node) >= LEAF_NODE_MAX_CELLS))
        {
            return EXECUTE_TABLE_FULL;
        }
        Row* row_to_insert = &(statement->row_to_insert);
        Cursor* cursor = table_end(table);
        leaf_node_insert(cursor, row_to_insert->id, row_to_insert);
        free(cursor);
        return EXECUTE_SUCCESS;
    }

有了这些变化 ， 我们的数据库应该像以前一样工作了 ， 除了现在它更快地返回一个 "表满" \
的错误 ， 因为我们还不能分割根节点 。 

叶子节点可以容纳多少行 ?

8.7 打印常量的命令
==============================================================================

我正在添加一个新的元命令 ， 以打印出一些感兴趣的常数 。 

.. code-block:: C 

    void print_constants()
    {
        printf("ROW_SIZE: %d\n", ROW_SIZE);
        printf("COMMON_NODE_HEADER_SIZE: %d\n", COMMON_NODE_HEADER_SIZE);
        printf("LEAF_NODE_HEADER_SIZE: %d\n", LEAF_NODE_HEADER_SIZE);
        printf("LEAF_NODE_CELL_SIZE: %d\n", LEAF_NODE_CELL_SIZE);
        printf("LEAF_NODE_SPACE_FOR_CELLS: %d\n", LEAF_NODE_SPACE_FOR_CELLS);
        printf("LEAF_NODE_MAX_CELLS: %d\n", LEAF_NODE_MAX_CELLS);
    }

    MetaCommandResult do_meta_command(InputBuffer* input_buffer, Table *table)
    {
        if (strcmp(input_buffer->buffer, ".exit") == 0)
        {
            close_input_buffer(input_buffer);
            db_close(table);
            exit(EXIT_SUCCESS);
        } else if (strcmp(input_buffer->buffer, ".constants") == 0){
            printf("Constants:\n");
            print_constants();
            return META_COMMAND_SUCCESS;
        } else {
            return META_COMMAND_UNRECOGNIZED_COMMAND;
        }
    }

我还添加了一个测试 ， 这样当这些常数发生变化时 ， 我们就会得到提醒 :

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
        "LEAF_NODE_HEADER_SIZE: 10",
        "LEAF_NODE_CELL_SIZE: 297",
        "LEAF_NODE_SPACE_FOR_CELLS: 4086",
        "LEAF_NODE_MAX_CELLS: 13",
        "db > ",
        ])
    end

因此 ， 我们的表格现在可以容纳 13 行 !

8.8 树的可视化
==============================================================================

为了帮助调试和可视化 ， 我还添加了一个元命令来打印出 btree 的表示 。

.. code-block:: C 

    void print_leaf_node(void* node)
    {
        uint32_t num_cells = *leaf_node_num_cells(node);
        printf("leaf (size %d)\n", num_cells);
        for (uint32_t i = 0; i < num_cells; i++) {
            uint32_t key = *leaf_node_key(node, i);
            printf("  - %d : %d\n", i, key);
        }
    }

    MetaCommandResult do_meta_command(InputBuffer* input_buffer, Table *table)
    {
        if (strcmp(input_buffer->buffer, ".exit") == 0)
        {
            close_input_buffer(input_buffer);
            db_close(table);
            exit(EXIT_SUCCESS);
        } else if(strcmp(input_buffer->buffer, ".btree") == 0){
            printf("Tree:\n");
            print_leaf_node(get_page(table->pager, 0));
            return META_COMMAND_SUCCESS;
        } else if(strcmp(input_buffer->buffer, ".constants") == 0){
            printf("Constants:\n");
            print_constants();
            return META_COMMAND_SUCCESS;
        } else {
            return META_COMMAND_UNRECOGNIZED_COMMAND;
        }
    }

和一个测试用例 ： 

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
        "  - 0 : 3",
        "  - 1 : 1",
        "  - 2 : 2",
        "db > "
        ])
    end

啊哦 ， 我们还是没有按排序的顺序来存储行 。 你会注意到 ``execute_insert()`` 是在 \
``table_end()`` 返回的位置插入叶子节点的 。 所以行是按照它们被插入的顺序来存储的 ， \
就像以前一样 。 

8.9 下一节
==============================================================================

这一切可能看起来像是一种退步 。 我们的数据库现在存储的行数比以前少了 ， 而且我们仍然\
是以未排序的顺序存储行数 。 但是就像我一开始说的 ， 这是一个很大的变化 ， 重要的是要\
把它分成可管理的步骤 。 

下一次 ， 我们将实现通过主键查找记录 ， 并开始按排序顺序存储记录 。 

`这里[1]`_ 和 `这里[2]`_ 是代码的改变部分 。

.. _`这里[1]`: https://github.com/Deteriorator/SimpleDB/commit/56b1757aa1872b6130c27209bc215449db02f0a9
.. _`这里[2]`: https://github.com/Deteriorator/SimpleDB/commit/bf9acafa7a00d68798fbc884e4f16535cbd928c5

******************************************************************************
第 09 部分  二进制搜索和重复 Key
******************************************************************************

