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


