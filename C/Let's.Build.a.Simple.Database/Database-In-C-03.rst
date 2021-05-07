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

