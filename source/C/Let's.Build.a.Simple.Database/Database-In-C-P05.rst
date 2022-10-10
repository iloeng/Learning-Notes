*******************************************************************************
Part 05 - 持久化到磁盘
*******************************************************************************

.. contents:: 目录
    :depth: 3
    :backlinks: top

.. 

    "Nothing in the world can take the place of persistence." – `Calvin Coolidge`_

.. _`Calvin Coolidge`: https://en.wikiquote.org/wiki/Calvin_Coolidge

我们的数据库允许你插入记录并读出它们， 但只有在你保持程序运行的情况下。 如果你关闭程\
序并重新启动它， 你的所有记录就会消失。 下面是我们想要的行为规范:

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

像 SQLite 一样， 我们将通过把整个数据库保存到一个文件中来持久化记录。 

我们已经通过将行序列化为页面大小的内存块来为自己做准备了。 为了增加持久性， 我们可以\
简单地将这些内存块写入一个文件， 并在下次程序启动时将其读回内存中。 

为了使这个问题更简单， 我们要做一个抽象的东西， 叫做 pager。 我们向 pager 索取第 x \
页， pager 给我们返回一个内存块。 它首先在其缓存中寻找。 在缓存缺失时， 它将数据从磁\
盘复制到内存中 (通过读取数据库文件)。 

.. figure:: img/arch-part5.gif
    :align: center

    我们的程序是如何与 SQLite 架构相匹配的

Pager 访问页面缓存和文件。 表对象通过 pager 对页面发出请求:

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

我把 ``new_table()`` 重命名为 ``db_open()``， 因为它现在具有打开数据库连接的效果\
。 我所说的打开连接是指:

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

``db_open()`` 依次调用 ``pager_open()``， 它打开数据库文件并跟踪其大小。 它还将页\
面缓存全部初始化为 NULL。 

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

按照我们新的抽象逻辑， 我们把获取页面的逻辑移到自己的方法中：

.. code-block:: C  

    void* row_slot(Table* table, uint32_t row_num)
    {
        uint32_t page_num = row_num / ROWS_PER_PAGE;
        void* page = get_page(table->pager, page_num);
        uint32_t row_offset = row_num % ROWS_PER_PAGE;
        uint32_t byte_offset = row_offset * ROW_SIZE;
        return page + byte_offset;
    }

``get_page()`` 方法有处理缓存丢失的逻辑。 我们假设页面是一个接一个地保存在数据库文件\
中。 第 0 页在偏移量 0 处， 第 1 页在偏移量 4096 处， 第 2 页在偏移量 8192 处等等\
。 如果请求的页面位于文件的边界之外， 我们知道它应该是空白的， 所以我们只是分配一些内\
存并将其返回。 当我们稍后刷新缓存到磁盘时， 该页将被添加到文件中。 

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

现在我们将等待缓存刷入磁盘， 直到用户关闭与数据库的连接。 当用户退出时， 我们将调用一\
个叫做 ``db_close()`` 的新方法:

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

在我们目前的设计中， 文件的长度编码了数据库中的行数， 所以我们需要在文件的最后写入部\
分页面。 这就是为什么 ``pager_flush()`` 同时需要一个页码和一个大小。 这不是最好的设\
计， 但是当我们开始实现 B-tree 时， 它将很快消失。 

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

最后我们需要接受文件名作为一个命令行参数。 不要忘了也给 ``do_meta_command`` 添加额\
外的参数。 

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

有了这些变化， 我们就能关闭然后重新打开数据库， 我们的记录仍然在那里！

.. code-block:: bash 

    complie: 
    gcc -g -w -Wall -o db simpledb.c

    ~ ./db mydb.db
    db > insert 1 cstack foo@bar.com
    Executed.
    db > insert 2 voltorb volty@example.com
    Executed.
    db > .exit
    ~
    ~ ./db mydb.db
    db > select
    (1, cstack, foo@bar.com)
    (2, voltorb, volty@example.com)
    Executed.
    db > .exit
    ~

为了增加乐趣， 让我们看一看 ``mydb.db``， 看看我们的数据是如何被存储的。 我将使用 \
vim 作为一个十六进制编辑器来查看文件的内存布局:

.. code-block:: shell

    vim mydb.db
    :%!xxd

.. figure:: img/file-format.png
    :align: center

    Current File Format

前四个字节是第一行的 ID (4 个字节， 因为我们存储的是 ``uint32_t``)。 它是以小端 (\
``little-endian``) 的字节顺序存储的， 所以最低字节在前 (``01``)， 后面是高序字节 \
(``00 00 00``)。 我们使用 ``memcpy()`` 将字节从我们的 Row 结构复制到页面缓存中， \
所以这意味着该结构在内存中是以小端字节顺序排列的。 这是我编译程序机器的一个属性。 如\
果我们想在我的机器上写一个数据库文件， 然后在大端机器上读取它， 我们就必须改变我们的 \
``serialize_row()`` 和 ``deserialize_row()`` 方法， 以便始终以相同的顺序存储和读\
取字节。 

接下来的 33 个字节将用户名存储为一个空尾字符串。 显然 "cstack" 的 ASCII 码以十六进\
制表示是 ``63 73 74 61 63 6b`` ， 后面是一个空字符 (00)。 其余的 33 个字节没有使用。 

接下来的 256 字节以同样的方式存储电子邮件。 在这里我们可以看到在结束的空字符之后有一\
些随机的垃圾。 这很可能是由于我们的 Row 结构中未初始化的内存造成的。 我们将整个 256 \
字节的电子邮件缓冲区复制到文件中， 包括字符串结束后的任何字节。 当我们分配该结构时， \
内存中的任何东西都还在那里。 但由于我们使用了一个结束性的空字符， 所以它对行为没有影响。 

注意： 如果我们想确保所有字节都被初始化， 那么在 ``serialize_row`` 中复制行的用户名\
和电子邮件字段时， 使用 ``strncpy`` 就足够了， 而不是 ``memcpy``， 像这样： 

.. code-block:: C 

    void serialize_row(Row* source, void* destination)
    {
        memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
        strncpy(destination + USERNAME_OFFSET, source->username, USERNAME_SIZE);
        strncpy(destination + EMAIL_OFFSET, source->email, EMAIL_SIZE);
    }

总结： 

好了！ 我们已经有了持久化。 这不是最好的实现。 例如， 如果你不输入 ``.exit`` 就关闭\
程序， 你就会失去你的改变。 此外， 我们正在把所有的页面写回磁盘， 即使是在我们从磁盘\
上读取后没有改变的页面。 这些问题我们可以在以后解决。 

下一次我们将介绍 cursors， 这将使 B 树的实现变得更加容易。 

在那之前! 

`这里[3]`_ 是本节代码的改变。 

.. _`这里[3]`: https://github.com/iloeng/SimpleDB/commit/691460d0a971d3f1a9bc4b60686da2e2c2dd45f9
