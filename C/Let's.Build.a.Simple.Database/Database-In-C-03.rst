##############################################################################
C 语言从头写一个 SQLite 程序
##############################################################################

.. contents::

******************************************************************************
第 04 部分  第一个测试 (和 BUG)
******************************************************************************

但是 sscanf 也有一些 缺点_ 。 如果它所读取的字符串大于它所读入的缓冲区 ， 它将导致\
缓冲区溢出 ， 并开始写到意外的地方 。 我们想在复制到 Row 结构之前检查每个字符串的长\
度 。 而要做到这一点 ， 我们需要将输入的内容除去空格 。 

.. _缺点: https://stackoverflow.com/questions/2430303/disadvantages-of-scanf

我将使用 strtok() 来做这件事 。 我想 ， 如果你看到它的实际效果 ， 就会更容易理解 。

.. code-block:: C 

    PrepareResult prepare_insert(InputBuffer* input_buffer, Statement* statement)
    {
        statement->type = STATEMENT_INSERT;

        char* keyword = strtok(input_buffer->buffer, " ");
        char* id_string = strtok(NULL, " ");
        char* username = strtok(NULL, " ");
        char* email = strtok(NULL, " ");

        if (id_string == NULL || username == NULL || email == NULL)
        {
            return PREPARE_SYNTAX_ERROR;
        }

        int id = atoi(id_string);
        if (strlen(username) > COLUMN_USERNAME_SIZE)
        {
            return PREPARE_STRING_TOO_LONG;
        }
        if (strlen(email) > COLUMN_EMAIL_SIZE)
        {
            return PREPARE_STRING_TOO_LONG;
        }

        statement->row_to_insert.id = id;
        strcpy(statement->row_to_insert.username, username);
        strcpy(statement->row_to_insert.email, email);

        return PREPARE_SUCCESS;
    }

    PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement)
    {
        if (strncmp(input_buffer->buffer, "insert", 6) == 0)
        {
            return prepare_insert(input_buffer, statement);
        }
        if (strcmp(input_buffer->buffer, "select") == 0)
        {
            statement->type = STATEMENT_SELECT;
            return PREPARE_SUCCESS;
        }

        return PREPARE_UNRECOGNIZED_STATEMENT;
    }

在输入缓冲区上连续调用 strtok 将其分成子串 ， 每当它到达一个分隔符 (在我们的例子中是\
空格) 时插入一个空字符 。 它返回一个指向子串起点的指针 。 

我们可以对每个文本值调用 strlen() 函数 ， 看看它是否太长 。 

我们可以像处理其他错误代码一样处理这个错误 。 

.. code-block:: C 

    typedef enum
    {
        PREPARE_SUCCESS,
        PREPARE_STRING_TOO_LONG,
        PREPARE_SYNTAX_ERROR,
        PREPARE_UNRECOGNIZED_STATEMENT
    } PrepareResult;

    [main]
    switch (prepare_statement(input_buffer, &statement))
    {
        case (PREPARE_SUCCESS):
            break;
        case (PREPARE_STRING_TOO_LONG):
            printf("String is too long.\n");
            continue;
        case PREPARE_SYNTAX_ERROR:
            printf("Syntax error. Could not parse statement.\n");
            continue;
        case (PREPARE_UNRECOGNIZED_STATEMENT):
            printf("Unrecognized keyword at start of '%s'.\n", input_buffer->buffer);
            continue;
    }

这使得我们的测试通过 : 

.. code-block:: ruby

    bundle exec rspec
    ....

    Finished in 0.02284 seconds (files took 0.116 seconds to load)
    4 examples, 0 failures

既然我们在这里 ， 我们不妨再处理一个错误案例 。 

.. code-block:: ruby

    it 'prints an error message if id is negative' do
        script = [
            "insert -1 cstack foo@bar.com",
            "select",
            ".exit",
        ]
        result = run_script(script)
        expect(result).to match_array([
            "db > ID must be positive.",
            "db > Executed.",
            "db > ",
        ])
    end

    typedef enum
    {
        PREPARE_SUCCESS,
        PREPARE_NEGATIVE_ID,
        PREPARE_STRING_TOO_LONG,
        PREPARE_SYNTAX_ERROR,
        PREPARE_UNRECOGNIZED_STATEMENT
    } PrepareResult;

    [prepare_insert]
    int id = atoi(id_string);
    if (id < 0)
    {
        return PREPARE_NEGATIVE_ID;
    }
    if (strlen(username) > COLUMN_USERNAME_SIZE)
    {
        return PREPARE_STRING_TOO_LONG;
    }

    [main]
    switch (prepare_statement(input_buffer, &statement))
    {
        case (PREPARE_SUCCESS):
            break;
        case (PREPARE_NEGATIVE_ID):
            printf("ID must be positive.\n");
            continue;
        case (PREPARE_STRING_TOO_LONG):
            printf("String is too long.\n");
            continue;
        case PREPARE_SYNTAX_ERROR:
            printf("Syntax error. Could not parse statement.\n");
            continue;
        case (PREPARE_UNRECOGNIZED_STATEMENT):
            printf("Unrecognized keyword at start of '%s'.\n", input_buffer->buffer);
            continue;
    }

好了 ， 现在的测试就到此为止 。 接下来是一个非常重要的功能 ： 持久性 。 我们要把我们\
的数据库保存到一个文件中 ， 然后再把它读出来 。 

这将会是很好的 。 

这是这部分的 完整差异_ 。

.. _完整差异: https://github.com/Deteriorator/SimpleDB/commit/4252a9ba1dc5493df75601774c305fa4b42f2b80#diff-337fddf8c00f79f08b214c804fab533b9e07b92fb88e5629015421cb32887a27

我们还增加了 测试_ 。

.. _测试: https://github.com/Deteriorator/SimpleDB/commit/4252a9ba1dc5493df75601774c305fa4b42f2b80#diff-cd059b64c879760da651c87b92f415003bbadb2e3b4c49ef961d7ba26b8f80a8

******************************************************************************
第 05 部分  持久化到磁盘
******************************************************************************

.. 

    "Nothing in the world can take the place of persistence." – `Calvin Coolidge`_

.. _`Calvin Coolidge`: https://en.wikiquote.org/wiki/Calvin_Coolidge

我们的数据库允许你插入记录并读出它们 ， 但只有在你保持程序运行的情况下 。 如果你关闭\
程序并重新启动它 ， 你的所有记录就会消失 。 下面是我们想要的行为规范 :

.. code-block:: ruby

    it 'keeps data after closing connection' do
        result1 = run_script([
            "insert 1 user1 person1@example.com",
            ".exit",
        ])
        expect(result1).to match_array([
            "db > Executed.",
            "db > ",
        ])
        result2 = run_script([
            "select",
            ".exit",
        ])
        expect(result2).to match_array([
            "db > (1, user1, person1@example.com)",
            "Executed.",
            "db > ",
        ])
    end

像 SQLite 一样 ， 我们将通过把整个数据库保存到一个文件中来持久化记录 。 

我们已经通过将行序列化为页面大小的内存块来为自己做准备了 。 为了增加持久性 ， 我们可\
以简单地将这些内存块写入一个文件 ， 并在下次程序启动时将其读回内存中 。 

为了使这个问题更简单 ， 我们要做一个抽象的东西 ， 叫做 pager 。 我们向 pager 索取第 \
x 页 ， pager 给我们返回一个内存块 。 它首先在其缓存中寻找 。 在缓存缺失时 ， 它将\
数据从磁盘复制到内存中 (通过读取数据库文件) 。 

.. image:: img/arch-part5.gif

上图是我们的程序是如何与 SQLite 架构相匹配的

Pager 访问页面缓存和文件 。 表对象通过 pager 对页面发出请求 :

.. code-block:: C  

    typedef struct {
        int file_descriptor;
        uint32_t file_length;
        void* pages[TABLE_MAX_PAGES];
    } Pager;

    typedef struct
    {
        Pager* pager;
        uint32_t num_rows;
    } Table;

我把 new_table() 重命名为 db_open() ， 因为它现在具有打开数据库连接的效果 。 我所\
说的打开连接是指 :

- 打开数据库文件
- 初始化一个 pager 数据结构
- 初始化一个 table 数据结构

.. code-block:: C 

    Table* db_open(const char* filename)
    {
        Pager* pager = pager_open(filename);
        uint32_t num_rows = pager->file_length / ROW_SIZE;
        Table* table = malloc(sizeof(Table));
        table->pager = pager;
        table->num_rows = num_rows;
        return table;
    }

``db_open()`` 依次调用 ``pager_open()`` ， 它打开数据库文件并跟踪其大小 。 它还将\
页面缓存全部初始化为 NULL 。 

.. code-block:: C 

    Pager* pager_open(const char* filename){
        int fd = open(filename,
                O_RDWR |    // Read/Write mode
                O_CREAT,          // Create file if it does not exist
                S_IWUSR |         // User write permission
                S_IRUSR           // User read permission
        );

        if (fd == -1){
            printf("Unable to open file\n");
            exit(EXIT_FAILURE);
        }

        off_t file_length = lseek(fd, 0, SEEK_END);

        Pager* pager = malloc(sizeof(Pager));
        pager->file_descriptor = fd;
        pager->file_length = file_length;

        for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
            pager->pages[i] = NULL;
        }
        return pager;
    }

按照我们新的抽象逻辑 ， 我们把获取页面的逻辑移到自己的方法中 ：

.. code-block:: C  

    void* row_slot(Table* table, uint32_t row_num)
    {
        uint32_t page_num = row_num / ROWS_PER_PAGE;
        void* page = get_page(table->pager, page_num);
        uint32_t row_offset = row_num % ROWS_PER_PAGE;
        uint32_t byte_offset = row_offset * ROW_SIZE;
        return page + byte_offset;
    }

``get_page()`` 方法有处理缓存丢失的逻辑 。 我们假设页面是一个接一个地保存在数据库文\
件中 。 第 0 页在偏移量 0 处 ， 第 1 页在偏移量 4096 处 ， 第 2 页在偏移量 8192 处\
等等 。 如果请求的页面位于文件的边界之外 ， 我们知道它应该是空白的 ， 所以我们只是分\
配一些内存并将其返回 。 当我们稍后刷新缓存到磁盘时 ， 该页将被添加到文件中 。 

.. code-block:: C 

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
        }
        return pager->pages[page_num];
    }

现在我们将等待缓存刷入磁盘 ， 直到用户关闭与数据库的连接 。 当用户退出时 ， 我们将调\
用一个叫做 ``db_close()`` 的新方法 :

- 将页面缓存刷入磁盘
- 关闭数据库文件
- 释放 Pager 和 Table 数据结构的内存

.. code-block:: C 

    void db_close(Table* table)
    {
        Pager* pager = table->pager;
        uint32_t num_full_pages = table->num_rows / ROWS_PER_PAGE;
        for (uint32_t i = 0; i < num_full_pages; i++)
        {
            if (pager->pages[i] == NULL)
            {
                    continue;
            }
            pager_flush(pager, i, PAGE_SIZE);
            free(pager->pages[i]);
            pager->pages[i] = NULL;
        }
        // There may be a partial page to write to the end of the file
        // This should not be needed after we switch to a B-tree
        uint32_t num_additional_rows = table->num_rows % ROWS_PER_PAGE;
        if (num_additional_rows > 0)
        {
            uint32_t page_num = num_full_pages;
            if (pager->pages[page_num] != NULL)
            {
                pager_flush(pager, page_num, num_additional_rows * ROW_SIZE);
                free(pager->pages[page_num]);
                pager->pages[page_num] = NULL;
            }
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

    MetaCommandResult do_meta_command(InputBuffer* input_buffer, Table *table)
    {
        if (strcmp(input_buffer->buffer, ".exit") == 0)
        {
    //        close_input_buffer(input_buffer);
            db_close(table);
            exit(EXIT_SUCCESS);
        } else {
            return META_COMMAND_UNRECOGNIZED_COMMAND;
        }
    }

在我们目前的设计中 ， 文件的长度编码了数据库中的行数 ， 所以我们需要在文件的最后写入\
部分页面 。 这就是为什么 ``pager_flush()`` 同时需要一个页码和一个大小 。 这不是最好\
的设计 ， 但是当我们开始实现 B-tree 时 ， 它将很快消失 。 

.. code-block:: C 

    void pager_flush(Pager* pager, uint32_t page_num, uint32_t size)
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
        ssize_t bytes_written = write(pager->file_descriptor, pager->pages[page_num], size);
        if (bytes_written == -1)
        {
            printf("Error writing: %d\n", errno);
            exit(EXIT_FAILURE);
        }
    }

最后我们需要接受文件名作为一个命令行参数 。 不要忘了也给 ``do_meta_command`` 添加额\
外的参数 。 

.. code-block:: C 

    int main(int argc, char* argv[])
    {
        if (argc < 2)
        {
            printf("Must supply a database filename.\n");
            exit(EXIT_FAILURE);
        }

        char* filename = argv[1];
        Table* table = db_open(filename);

        InputBuffer* input_buffer = new_input_buffer();
        while (true)
        {
            print_prompt();
            read_input(input_buffer);

            if (input_buffer->buffer[0] == '.')
            {
                switch (do_meta_command(input_buffer, table))
                {
                    case (META_COMMAND_SUCCESS):
                        continue;
                    case (META_COMMAND_UNRECOGNIZED_COMMAND):
                        printf("Unrecognized command '%s'.\n", input_buffer->buffer);
                        continue;
                }
            }
            Statement statement;
            switch (prepare_statement(input_buffer, &statement))
            {
                case (PREPARE_SUCCESS):
                    break;
                case (PREPARE_NEGATIVE_ID):
                    printf("ID must be positive.\n");
                    continue;
                case (PREPARE_STRING_TOO_LONG):
                    printf("String is too long.\n");
                    continue;
                case PREPARE_SYNTAX_ERROR:
                    printf("Syntax error. Could not parse statement.\n");
                    continue;
                case (PREPARE_UNRECOGNIZED_STATEMENT):
                    printf("Unrecognized keyword at start of '%s'.\n", input_buffer->buffer);
                    continue;
            }
            // execute_statement(&statement);
            // printf("Executed.\n");
            switch (execute_statement(&statement, table))
            {
                case (EXECUTE_SUCCESS):
                    printf("Executed!\n");
                    break;
                case (EXECUTE_TABLE_FULL):
                    printf("Error: Table full.\n");
                    break;
            }
        }
    }

未完待续 ...

上一篇文章 ： `上一篇`_

下一篇文章 ： `下一篇`_ 

.. _`上一篇`: Database-In-C-02.rst
.. _`下一篇`: Database-In-C-04.rst
