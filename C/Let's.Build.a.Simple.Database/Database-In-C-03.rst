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


