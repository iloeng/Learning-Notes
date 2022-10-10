*******************************************************************************
Part 04 - 第一个测试 (和 BUG)
*******************************************************************************

.. contents:: 目录
    :depth: 3
    :backlinks: top

我们已经具备了向数据库插入行和打印出所有行的能力。 让我们花点时间来测试一下我们目前得\
到的东西。 

我打算用 rspec_ 来写我的测试， 因为我对它很熟悉， 而且语法也相当可读。 

.. _rspec: http://rspec.info/

我将定义一个简短的辅助工具， 向我们的数据库程序发送一个命令列表， 然后对输出进行断言:

.. code-block:: ruby 

    describe 'database' do
        def run_script(commands)
            raw_output = nil
            IO.popen("./db", "r+") do |pipe|
            commands.each do |command|
                pipe.puts command
            end

            pipe.close_write

            # Read entire output
            raw_output = pipe.gets(nil)
            end
            raw_output.split("\n")
        end

        it 'inserts and retrieves a row' do
            result = run_script([
                "insert 1 user1 person1@example.com",
                "select",
                ".exit",
            ])
            expect(result).to match_array([
                "db > Executed.",
                "db > (1, user1, person1@example.com)",
                "Executed.",
                "db > ",
            ])
        end
    end

这个简单的测试确保了我们的投入能得到回报。 而事实上， 它通过了:

.. code-block:: shell

    bundle exec rspec
    .

    Finished in 0.00871 seconds (files took 0.09506 seconds to load)
    1 example, 0 failures

现在， 测试向数据库插入大量的行是可行的。 

.. code-block:: ruby

    it 'prints error message when table is full' do
        script = (1..1401).map do |i|
            "insert #{i} user#{i} person#{i}@example.com"
        end
        script << ".exit"
        result = run_script(script)
        expect(result[-2]).to eq('db > Error: Table full.')
    end

再次运行测试 ... 

.. code-block:: shell 

    bundle exec rspec
    ..

    Finished in 0.01553 seconds (files took 0.08156 seconds to load)
    2 examples, 0 failures

很好， 成功了! 我们的数据库现在可以容纳 1400 行， 因为我们把最大的页数设置为 100， \
而 14 行可以放在一个页面中。 

通过阅读我们到目前为止的代码， 我意识到我们可能没有正确处理存储文本字段。 用这个例子\
很容易测试:

.. code-block:: ruby

    it 'allows inserting strings that are the maximum length' do
        long_username = "a"*32
        long_email = "a"*255
        script = [
            "insert 1 #{long_username} #{long_email}",
            "select",
            ".exit",
        ]
        result = run_script(script)
        expect(result).to match_array([
            "db > Executed.",
            "db > (1, #{long_username}, #{long_email})",
            "Executed.",
            "db > ",
        ])
    end

然而测试失败了! 

.. code-block:: shell 

    Failures:

    1) database allows inserting strings that are the maximum length
        Failure/Error: raw_output.split("\n")

        ArgumentError:
        invalid byte sequence in UTF-8
        # ./spec/main_spec.rb:14:in `split`
        # ./spec/main_spec.rb:14:in `run_script`
        # ./spec/main_spec.rb:48:in `block (2 levels) in <top (required)>`

如果我们自己尝试一下， 就会发现当我们试图打印出这一行时， 有一些奇怪的字符。 (我对长\
字符串进行了缩写)。 

.. code-block:: shell

    db > insert 1 aaaaa... aaaaa...
    Executed.
    db > select
    (1, aaaaa...aaa\�, aaaaa...aaa\�)
    Executed.
    db >

发生了什么事? 如果你看一下我们对行的定义， 我们为用户名分配了正好 32 个字节， 为电子\
邮件分配了正好 255 个字节。 但是， C 语言的字符串应该以空字符结束， 而我们并没有为它\
分配空间。 解决的办法是多分配一个字节:

.. code-block:: C 

    typedef struct
    {
        uint32_t id;
        char username[COLUMN_USERNAME_SIZE + 1];
        char email[COLUMN_EMAIL_SIZE + 1];
    } Row;

而这确实解决了这个问题。 

.. code-block:: shell

    bundle exec rspec
    ...

    Finished in 0.0188 seconds (files took 0.08516 seconds to load)
    3 examples, 0 failures

我们不应该允许插入比列大小更长的用户名或电子邮件。 这方面的规范是这样的:

.. code-block:: ruby

    it 'prints error message if strings are too long' do
        long_username = "a"*33
        long_email = "a"*256
        script = [
            "insert 1 #{long_username} #{long_email}",
            "select",
            ".exit",
        ]
        result = run_script(script)
        expect(result).to match_array([
            "db > String is too long.",
            "db > Executed.",
            "db > ",
        ])
    end

为了做到这一点， 我们需要升级我们的分析器。 作为提醒， 我们目前正在使用 ``sscanf()``。

.. code-block:: C 

    if (strncmp(input_buffer->buffer, "insert", 6) == 0)
    {
        statement->type = STATEMENT_INSERT;
        int args_assigned = sscanf(
                input_buffer->buffer,
                "insert %d %s %s",
                &(statement->row_to_insert.id),
                statement->row_to_insert.username,
                statement->row_to_insert.email
        );
        if (args_assigned < 3)
        {
            return PREPARE_SYNTAX_ERROR;
        }
        return PREPARE_SUCCESS;
    }

但是 ``sscanf`` 也有一些 `缺点`_。 如果它所读取的字符串大于它所读入的缓冲区， 它将导\
致缓冲区溢出， 并开始写到意外的地方。 我们想在复制到 Row 结构之前检查每个字符串的长度\
。 而要做到这一点， 我们需要将输入的内容除去空格。 

.. _缺点: https://stackoverflow.com/questions/2430303/disadvantages-of-scanf

我将使用 ``strtok()`` 来做这件事。 我想， 如果你看到它的实际效果， 就会更容易理解。

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

在输入缓冲区上连续调用 ``strtok`` 将其分成子串， 每当它到达一个分隔符 (在我们的例子\
中是空格) 时插入一个空字符。 它返回一个指向子串起点的指针。 

我们可以对每个文本值调用 ``strlen()`` 函数， 看看它是否太长。 

我们可以像处理其他错误代码一样处理这个错误。 

.. code-block:: C 

    typedef enum
    {
        PREPARE_SUCCESS,
        PREPARE_STRING_TOO_LONG,
        PREPARE_SYNTAX_ERROR,
        PREPARE_UNRECOGNIZED_STATEMENT
    } PrepareResult;

    //[main]
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

这使得我们的测试通过: 

.. code-block:: ruby

    bundle exec rspec
    ....

    Finished in 0.02284 seconds (files took 0.116 seconds to load)
    4 examples, 0 failures

既然我们在这里， 我们不妨再处理一个错误案例。 

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

好了， 现在的测试就到此为止。 接下来是一个非常重要的功能： 持久性。 我们要把我们的数\
据库保存到一个文件中， 然后再把它读出来。 

这将会是很好的。 

这是这部分的 `完整差异`_ 。

.. _完整差异: https://github.com/iloeng/SimpleDB/commit/4252a9ba1dc5493df75601774c305fa4b42f2b80#diff-337fddf8c00f79f08b214c804fab533b9e07b92fb88e5629015421cb32887a27

我们还增加了 `测试`_ 。

.. _测试: https://github.com/iloeng/SimpleDB/commit/4252a9ba1dc5493df75601774c305fa4b42f2b80#diff-cd059b64c879760da651c87b92f415003bbadb2e3b4c49ef961d7ba26b8f80a8
