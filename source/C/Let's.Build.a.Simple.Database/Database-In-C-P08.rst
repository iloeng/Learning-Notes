*******************************************************************************
Part 08 - B 型树叶子节点格式
*******************************************************************************

.. contents:: 目录
    :depth: 3
    :backlinks: top

我们正在改变我们的表的格式， 从一个无序的行数组到一个 B-Tree。 这是一个相当大的变化\
， 需要多篇文章来实现。 在本文结束时， 我们将定义叶子节点的布局， 并支持将键 / 值对插\
入单节点树中。 但首先， 让我们回顾一下切换到树形结构的原因。 

8.1 备用的表格式
===============================================================================

在目前的格式下， 每个页面只存储行 (没有元数据)， 所以它的空间效率相当高。 插入的速度\
也很快， 因为我们只是追加到最后。 然而要找到某一行， 只能通过扫描整个表来完成。 而且\
如果我们想删除某一行， 我们必须通过移动它后面的每一行来填补这个漏洞。 

如果我们将表存储为一个数组， 但将行按 ``id`` 排序， 我们可以使用二进制搜索来找到一个\
特定的 ``id``。 然而插入的速度会很慢， 因为我们必须移动大量的行来腾出空间。 

相反我们要用一个树形结构。 树中的每个节点可以包含数量不等的行， 所以我们必须在每个节\
点中存储一些信息来跟踪它包含多少行。 另外还有所有内部节点的存储开销， 这些节点不存储\
任何行。 作为对较大数据库文件的交换， 我们得到了快速插入、 删除和查询。 

=============  ======================  ====================  ================================
Row            Unsorted Array of rows  Sorted Array of rows  Tree of nodes
=============  ======================  ====================  ================================
Pages contain  only data               only data             metadata, primary keys, and data
Rows per page  more                    more                  fewer
Insertion      O(1)                    O(n)                  O(log(n))
Deletion       O(n)                    O(n)                  O(log(n))
Lookup by id   O(n)                    O(log(n))             O(log(n))
=============  ======================  ====================  ================================

8.2 节点头部格式
===============================================================================

叶子结点和内部结点有不同的布局。 让我们做一个枚举来跟踪节点的类型: 

.. code-block:: C 

    typedef enum
    {
        NODE_INTERNAL, NODE_LEAF
    } NodeType;

每个节点将对应于一个页面。 内部节点将通过存储子节点的页号来指向它们的子节点。 btree \
向 pager 询问一个特定的页码， 并得到一个进入页面缓存的指针。 页面按照页码的顺序一个接\
一个地存储在数据库文件中。 

节点需要在页面开头的头中存储一些元数据。 每个节点都将存储它是什么类型的节点， 它是否\
是根节点， 以及它的父节点的指针 (以便于找到节点的兄弟姐妹)。 我为每个头字段的大小和偏\
移量定义了常数。 

.. code-block:: C 

    /*
    * Common Node Header Layout
    */
    const uint32_t NODE_TYPE_SIZE = sizeof(uint8_t);
    const uint32_t NODE_TYPE_OFFSET = 0;
    const uint32_t IS_ROOT_SIZE = sizeof(uint8_t);
    const uint32_t IS_ROOT_OFFSET = NODE_TYPE_SIZE;
    const uint32_t PARENT_POINTER_SIZE = sizeof(uint32_t);
    const uint32_t PARENT_POINTER_OFFSET = IS_ROOT_OFFSET + IS_ROOT_SIZE;
    const uint8_t COMMON_NODE_HEADER_SIZE = NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE;

8.3 叶子节点格式
===============================================================================

除了这些常见的头字段外， 叶子节点还需要存储它们包含多少个 "单元"。 一个单元是一个键 \
/ 值对。 

.. code-block:: C 

    /*
    * Leaf Node Header Layout
    */
    const uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);
    const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;
    const uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE;

叶子节点的主体是一个单元格的数组。 每个单元格是一个键， 后面是一个值 (一个序列化的行)。 

.. code-block:: C 

    /*
    * Leaf Node Body Layout
    */
    const uint32_t LEAF_NODE_KEY_SIZE = sizeof(uint32_t);
    const uint32_t LEAF_NODE_KEY_OFFSET = 0;
    const uint32_t LEAF_NODE_VALUE_SIZE = ROW_SIZE;
    const uint32_t LEAF_NODE_VALUE_OFFSET = LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE;
    const uint32_t LEAF_NODE_CELL_SIZE = LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE;
    const uint32_t LEAF_NODE_SPACE_FOR_CELLS = PAGE_SIZE - LEAF_NODE_HEADER_SIZE;
    const uint32_t LEAF_NODE_MAX_CELLS = LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE;

基于这些常数， 下面是一个叶子节点的布局， 目前看起来是这样的: 

.. figure:: img/leaf-node-format.png 
    :align: center

    Our leaf node format

在 header 里每个布尔值使用一整个字节， 这样空间利用率低， 但这使得编写访问这些值的代\
码更容易。 

还注意到在最后有一些浪费的空间。 我们在 header 之后尽可能多地存储单元格， 但剩下的空\
间不能容纳整个单元格。 我们把它留空， 以避免在节点之间分割单元格。 

8.4 访问叶子节点字段
===============================================================================

访问键、 值和元数据的代码都涉及到使用我们刚刚定义的常数的指针运算。 

.. code-block:: C 

    uint32_t* leaf_node_num_cells(void* node)
    {
        return node + LEAF_NODE_NUM_CELLS_OFFSET;
    }

    void* leaf_node_cell(void* node, uint32_t cell_num)
    {
        return node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
    }

    uint32_t* leaf_node_key(void* node, uint32_t cell_num)
    {
        return leaf_node_cell(node, cell_num);
    }

    void* leaf_node_value(void* node, uint32_t cell_num)
    {
        return leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
    }

    void initialize_leaf_node(void* node)
    {
        *leaf_node_num_cells(node) = 0;
    }

这些方法返回一个指向相关值的指针， 所以它们既可以作为一个获取器， 也可以作为一个设置\
器使用。 

8.5 Pager 和 Table 对象的变化
===============================================================================

每一个节点都将正好占用一个页面， 即使它不是满的。 这意味着我们的 Pager 不再需要支持\
读 / 写部分页面。 

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

现在在我们的数据库中存储页数比存储行数更有意义。 页数应该与 pager 对象相关联， 而不是\
与表相关联， 因为它是数据库使用的页数， 而不是一个特定的表。 一个 btree 是由它的根节\
点的页数来识别的， 所以表对象需要跟踪它。 

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

8.6 Cursor 对象的变化
===============================================================================

一个游标代表了表中的一个位置。 当我们的表是一个简单的行数组时， 我们可以通过行号来访\
问一个行。 现在它是一棵树， 我们通过节点的页码和该节点中的单元格编号来确定一个位置。 

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

8.7 插入叶子节点
===============================================================================

在这篇文章中， 我们只打算实现足以得到一个单节点的树。 回顾一下上一篇文章， 树开始时是\
一个空的叶子节点:

.. figure:: img/btree1.png 
    :align: center

    empty btree

键 / 值对可以被添加， 直到叶子节点被填满:

.. figure:: img/btree2.png 
    :align: center

    one-node btree

当我们第一次打开数据库时， 数据库文件将是空的， 所以我们将第 0 页初始化为一个空的叶节\
点 (根节点):

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

接下来我们将制作一个函数， 用于将键 / 值对插入到叶子节点中。 它将接受一个光标作为输入\
， 以表示这对键值应被插入的位置。 

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

我们还没有实现拆分， 所以如果节点已满会出错。 接下来我们将单元格向右移动一个空格， 为\
新的单元格腾出空间。 然后我们把新的键 / 值写进空位。 

由于我们假设树只有一个节点， 我们的 ``execute_insert()`` 函数只需要调用这个辅助方法:

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

有了这些变化， 我们的数据库应该像以前一样工作了， 除了现在它更快地返回一个 "表满" 的\
错误， 因为我们还不能分割根节点。 

叶子节点可以容纳多少行?

8.8 打印常量的命令
===============================================================================

我正在添加一个新的元命令， 以打印出一些感兴趣的常数。 

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

我还添加了一个测试， 这样当这些常数发生变化时， 我们就会得到提醒:

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

因此， 我们的表格现在可以容纳 13 行!

8.9 树的可视化
===============================================================================

为了帮助调试和可视化， 我还添加了一个元命令来打印出 btree 的表示。

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

和一个测试用例： 

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

啊哦， 我们还是没有按排序的顺序来存储行。 你会注意到 ``execute_insert()`` 是在 \
``table_end()`` 返回的位置插入叶子节点的。 所以行是按照它们被插入的顺序来存储的， 就\
像以前一样。 

8.10 下一节
===============================================================================

这一切可能看起来像是一种退步。 我们的数据库现在存储的行数比以前少了， 而且我们仍然是\
以未排序的顺序存储行数。 但是就像我一开始说的， 这是一个很大的变化， 重要的是要把它分\
成可管理的步骤。 

下一次， 我们将实现通过主键查找记录， 并开始按排序顺序存储记录。 

`这里[5]`_ 和 `这里[6]`_ 是代码的改变部分 。

.. _`这里[5]`: https://github.com/iloeng/SimpleDB/commit/56b1757aa1872b6130c27209bc215449db02f0a9
.. _`这里[6]`: https://github.com/iloeng/SimpleDB/commit/bf9acafa7a00d68798fbc884e4f16535cbd928c5
