###############################################################################
C 语言从头写一个 SQLite 程序
###############################################################################

.. contents:: 目录
    :depth: 3
    :backlinks: top

*******************************************************************************
第 02 部分  世界上最简单的 SQL 编译器和虚拟机 
*******************************************************************************

我们正在制作一个 SQLite 的克隆体， SQLite 的前端是一个 SQL 编译器， 用于解析一个字符\
串， 输出一个叫做字节码的内部表示法。 

这个字节码被传递给虚拟机， 由它来执行。 

.. figure:: img/arch.svg
    :align: center

    sqlite architecture (https://www.sqlite.org/arch.html)

像这样把事情分成两步来做有几个好处:

- 减少每个部分的复杂性 (例如， 虚拟机不担心语法错误)。
- 允许对常见的查询进行一次编译， 并对字节码进行缓存以提高性能。

考虑到这一点， 让我们重构我们的主函数， 并在这个过程中支持两个新的关键字。

.. code-block:: C 

    int main(int argc, char* argv[])
    {
        InputBuffer* input_buffer = new_input_buffer();
        while (true)
        {
            print_prompt();
            read_input(input_buffer);

            if (input_buffer->buffer[0]) == '.')
            {
                switch (do_meta_command(input_buffer))
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
                case (PREPARE_UNRECOGNIZED_STATEMENT):
                    printf("Unrecognized keyword at start of '%s'.\n", input_buffer->buffer);
                    continue;
            }
            execute_statement(&statement);
            printf("Executed.\n");

        }
    }

像 ``.exit`` 这样的非 SQL 语句被称为 "元命令"。 它们都以点开始， 所以我们检查它们并\
在一个单独的函数中处理它们。 

接下来， 我们添加一个步骤， 将输入行转换为我们内部的语句表示。 这就是我们的黑客版本的 \
SQLite 前端。 

最后， 我们将准备好的语句传递给 ``execute_statement``。 这个函数最终将成为我们的虚拟\
机。 

请注意， 我们的两个新函数返回的枚举表示成功或失败。 

.. code-block:: C 

    typedef enum {
        META_COMMAND_SUCCESS,
        META_COMMAND_UNRECOGNIZED_COMMAND
    } MetaCommandResult;

    typedef enum { 
        PREPARE_SUCCESS, 
        PREPARE_UNRECOGNIZED_STATEMENT 
    } PrepareResult;

"Unrecognized statement"? 这似乎有点像一个异常。 但是异常是不好的 (而且 C 语言甚至\
不支持异常)， 所以我在实用的地方使用枚举结果代码。 如果我的 switch 语句没有处理枚举的\
成员， C 编译器会抱怨， 所以我们可以放心地处理函数的每个结果。 预计将来会有更多的结果\
代码加入。 

``do_meta_command`` 只是对现有功能的一个包装， 为更多的命令留下了空间。 

.. code-block:: C  

    MetaCommandResult do_meta_command(InputBuffer* input_buffer) {
        if (strcmp(input_buffer->buffer, ".exit") == 0) {
            exit(EXIT_SUCCESS);
        } else {
            return META_COMMAND_UNRECOGNIZED_COMMAND;
        }
    }

我们的 "prepared statement" 现在只包含一个有两个可能值的枚举。 当我们允许语句中的参\
数时， 它将包含更多的数据。 

.. code-block:: C 

    typedef enum {
        STATEMENT_INSERT,
        STATEMENT_SELECT
    } StatementType;

    typedef struct {
        StatementType type;
    } Statement;

``prepare_statement`` (我们的 "SQL 编译器") 现在还不理解 SQL。 事实上， 它只理解两\
个词。 

.. code-block:: C  

    PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement)
    {
        if (strncmp(input_buffer->buffer, "insert", 6) == 0)
        {
            statement->type = STATEMENT_INSERT;
            return PREPARE_SUCCESS;
        }
        if (strcmp(input_buffer->buffer, "select") == 0)
        {
            statement->type = STATEMENT_SELECT;
            return PREPARE_SUCCESS;
        }

        return PREPARE_UNRECOGNIZED_STATEMENT;
    }

注意， 我们用 strncmp 来表示 "insert" 命令， 因为 "insert" 关键词后面会有数据。 (例\
如: ``insert 1 cstack foo@bar.com``)

最后， ``execute_statement`` 包含一些步骤。 

.. code-block:: C 

    void execute_statement(Statement* statement)
    {
        switch (statement->type)
        {
            case (STATEMENT_INSERT):
                printf("This is where we would do an insert.\n");
                break;
            case (STATEMENT_SELECT):
                printf("This is where we would do a select.\n");
                break;
        }
    }

请注意， 它没有返回任何错误代码， 因为还没有什么可能出错。 

通过这些重构， 我们现在可以识别两个新的关键词了!

.. code-block:: shell

    ~ ./db
    db > insert foo bar
    This is where we would do an insert.
    Executed.
    db > delete foo
    Unrecognized keyword at start of 'delete foo'.
    db > select
    This is where we would do a select.
    Executed.
    db > .tables
    Unrecognized command '.tables'
    db > .exit
    ~

我们的数据库的骨架正在形成 ...... 如果它能存储数据， 那不是很好吗? 在下一部分 ， 我们\
将实现插入和选择， 创建世界上最糟糕的数据存储。 同时， `这里`_ 是本部分的全部内容。 

.. _这里: https://github.com/iloeng/SimpleDB/commit/81af30cabcec1b9700f72472fb668cc3c02d602c

******************************************************************************
第 03 部分  一个内存中的只加单表的数据库
******************************************************************************

我们将从小处着手 ， 给我们的数据库设置很多限制 。 就目前而言 ， 它将 :

- 支持两种操作 ： 插入行和打印所有行 
- 只存在于内存中 (不存在于磁盘中) 。
- 支持单独的硬编码的表格 。 

我们的硬编码表将存储用户 ， 看起来像这样 ：

====================  ============  
**Column**            **Type**    
====================  ============  
id                    integer     
username              varchar(32)
email                 varchar(255)
====================  ============

这是一个简单的模式 ， 但它让我们支持多种数据类型和多种尺寸的文本数据类型 。 

insert 语句现在看起来是这样的 :

.. code-block:: shell

    insert 1 cstack foo@bar.com

这意味着我们需要升级我们的 ``prepare_statement`` 函数以解析参数 :

.. code-block:: C  

    PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement)
    {
        if (strncmp(input_buffer->buffer, "insert", 6) == 0)
        {
            statement->type = STATEMENT_INSERT;
            int args_assigned = sscanf(
                    input_buffer->buffer, 
                    "insert %d %s %s",
                    &(statement->row_to_insert.id),
                    statement->row_to_insert.username,
                    statement->row_to_insert.email
            )
            if (args_assigned < 3)
            {
                return PREPARE_SYNTAX_ERROR;
            }
            return PREPARE_SUCCESS;
        }
        if (strcmp(input_buffer->buffer, "select") == 0)
        {
            statement->type = STATEMENT_SELECT;
            return PREPARE_SUCCESS;
        }

        return PREPARE_UNRECOGNIZED_STATEMENT;
    }

我们将这些被解析的参数存储到语句对象内部的一个新的 Row 数据结构中 : 

.. code-block:: C  

    #define COLUMN_USERNAME_SIZE 32
    #define COLUMN_EMAIL_SIZE 255

    typedef struct
    {
        uint32_t id;
        char username[COLUMN_USERNAME_SIZE];
        char email[COLUMN_EMAIL_SIZE];
    } Row;

    typedef struct
    {
        StatementType type;
        Row row_to_insert;  // only used by insert statement
    } Statement;

现在我们需要将这些数据复制到代表该表的一些数据结构中 。 SQLite 使用 B 树来进行快速\
查找 、 插入和删除 。 我们将从更简单的东西开始 。 像 B 树一样 ， 它将把行分组到页\
中 ， 但不是把这些页作为树状排列 ， 而是把它们作为数组排列 。 

我的计划是这样的 ：

- 将行存储在称为页的内存块中
- 每页存储的行数越多越好
- 行被序列化为一个紧凑的表示 ， 每页都有
- 页面只在需要时分配
- 保持一个固定大小的页面指针数组

首先我们要定义行的紧凑表示法 ： 

.. code-block:: C 

    #define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)

    const uint32_t ID_SIZE = size_of_attribute(Row, id);
    const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
    const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
    const uint32_t ID_OFFSET = 0;
    const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
    const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
    const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

这意味着一个序列化的行的布局将看起来像这样 :

============  ================  ==========
**Column**    **Size (Bytes)**  **offset**  
============  ================  ==========
id            integer           0
username      varchar(32)       4
email         varchar(255)      36
total         291
============  ================  ==========

我们还需要代码来转换为紧凑表示法和从紧凑表示法转换 。 

.. code-block:: C 

    void serialize_row(Row* source, void* destination)
    {
        memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
        memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
        memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
    }

    void deserialize_row(void* source, Row* destination)
    {
        memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
        memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
        memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
    }

接下来是一个表结构 ， 它指向行的页面并记录有多少行 。 

.. code-block:: C  

    const uint32_t PAGE_SIZE = 4096;
    #define TABLE_MAX_PAGES 100
    const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
    const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

    typedef struct
    {
        uint32_t num_rows;
        void* pages[TABLE_MAX_PAGES];
    } Table;

我将我们的页面大小定为4千字节 ， 因为它与大多数计算机架构的虚拟内存系统中使用的页\
面大小相同 。 这意味着我们数据库中的一个页面对应于操作系统使用的一个页面 。 操作系\
统会将页面作为一个完整的单元移入和移出内存 ， 而不是将它们拆开 。 

我设置了一个分配 100 个页面的独断的限制 。 当我们切换到树状结构时 ， 我们的数据库\
的最大尺寸将只受文件最大尺寸的限制 。 虽然我们仍然会限制我们一次在内存中保留多少页 。

行不应该跨越页的边界 。 由于页面在内存中可能不会彼此相邻 ， 这个假设使读 / 写行变\
得更容易 。 

说到这里 ， 我们是如何计算出某一行在内存中的读 / 写位置的 :

.. code-block:: C 

    void* row_slot(Table* table, uint32_t row_num)
    {
        uint32_t page_num = row_num / ROWS_PER_PAGE;
        void* page = table->pages[page_num];
        if (page == NULL) {
            // Allocate memory only when we try to access page
            page = table->pages[page_num] = malloc(PAGE_SIZE);
        }
        uint32_t row_offset = row_num % ROWS_PER_PAGE;
        uint32_t byte_offset = row_offset * ROW_SIZE;
        return page + byte_offset;
    }

现在我们可以使 ``execute_statement`` 从我们的表结构中读 / 写 。 

.. code-block:: C  

    ExecuteResult execute_insert(Statement* statement, Table* table)
    {
        if (table->num_rows >= TABLE_MAX_ROWS)
        {
            return EXECUTE_TABLE_FULL;
        }
        Row* row_to_insert = &(statement->row_to_insert);
        serialize_row(row_to_insert, row_slot(table, table->num_rows));
        table->num_rows += 1;
        return EXECUTE_SUCCESS;
    }

    ExecuteResult execute_select(Statement* statement, Table* table)
    {
        Row row;
        for (uint32_t i = 0; i < table->num_rows; i++)
        {
            deserialize_row(row_slot(table, i), &row);
            print_row(&row);
        }
        return EXECUTE_SUCCESS
    }

    ExecuteResult execute_statement(Statement* statement, Table* table)
    {
        switch (statement->type)
        {
            case (STATEMENT_INSERT):
                return execute_insert(statement, table);
            case (STATEMENT_SELECT):
                return execute_select(statement, table);
        }
    }

最后 ， 我们需要初始化表 ， 创建相应的内存释放函数 ， 并处理一些更多的错误情况 。 

.. code-block:: C 

    Table* new_table() 
    {
        Table* table = malloc(sizeof(Table));
        table->num_rows = 0;
        for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) 
        {
            table->pages[i] = NULL;
        }
        return table;
    }

    void free_table(Table* table) 
    {
        for (int i = 0; table->pages[i]; i++) 
        {
            free(table->pages[i]);
        }
        free(table);
    }

    int main(int argc, char* argv[])
    {
        Table* table = new_table();
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
                case PREPARE_SYNTAX_ERROR:
                    printf("Syntax error. Could not parse statement.\n");
                    continue;
                case (PREPARE_UNRECOGNIZED_STATEMENT):
                    printf("Unrecognized keyword at start of '%s'.\n", input_buffer->buffer);
                    continue;
            }
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

有了这些变化 ， 我们就可以在我们的数据库中实际保存数据了 ! 

.. code-block:: shell

    ~ ./db
    db > insert 1 cstack foo@bar.com
    Executed.
    db > insert 2 bob bob@example.com
    Executed.
    db > select
    (1, cstack, foo@bar.com)
    (2, bob, bob@example.com)
    Executed.
    db > insert foo bar 1
    Syntax error. Could not parse statement.
    db > .exit
    ~

现在是写一些测试的好时机 ， 有几个原因 :

- 我们正计划大幅改变存储我们表格的数据结构 ， 而测试会捕捉回归 。
- 有几个边缘情况我们还没有手动测试 (例如 : 填表) 。 

我们将在下一部分中解决这些问题 。 现在 ， 这里_ 是本部分的完整差异 。 

.. _这里: https://github.com/Deteriorator/SimpleDB/commit/86cc806da9e94391498c9c5a15f04fe4f2c90d56

******************************************************************************
第 04 部分  第一个测试 (和 BUG)
******************************************************************************

我们已经具备了向数据库插入行和打印出所有行的能力 。 让我们花点时间来测试一下我们目前\
得到的东西 。 

我打算用 rspec_ 来写我的测试 ， 因为我对它很熟悉 ， 而且语法也相当可读 。 

.. _rspec: http://rspec.info/

我将定义一个简短的辅助工具 ， 向我们的数据库程序发送一个命令列表 ， 然后对输出进行断\
言 :

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

这个简单的测试确保了我们的投入能得到回报 。 而事实上 ， 它通过了 :

.. code-block:: shell

    bundle exec rspec
    .

    Finished in 0.00871 seconds (files took 0.09506 seconds to load)
    1 example, 0 failures

现在 ， 测试向数据库插入大量的行是可行的 。 

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

很好 ， 成功了 ! 我们的数据库现在可以容纳 1400 行 ， 因为我们把最大的页数设置为 \
100 ， 而 14 行可以放在一个页面中 。 

通过阅读我们到目前为止的代码 ， 我意识到我们可能没有正确处理存储文本字段 。 用这个例\
子很容易测试 :

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

然而测试失败了 ! 

.. code-block:: shell 

    Failures:

    1) database allows inserting strings that are the maximum length
        Failure/Error: raw_output.split("\n")

        ArgumentError:
        invalid byte sequence in UTF-8
        # ./spec/main_spec.rb:14:in `split`
        # ./spec/main_spec.rb:14:in `run_script`
        # ./spec/main_spec.rb:48:in `block (2 levels) in <top (required)>`

如果我们自己尝试一下 ， 就会发现当我们试图打印出这一行时 ， 有一些奇怪的字符 。 (我\
对长字符串进行了缩写) 。 

.. code-block:: shell

    db > insert 1 aaaaa... aaaaa...
    Executed.
    db > select
    (1, aaaaa...aaa\�, aaaaa...aaa\�)
    Executed.
    db >

发生了什么事 ? 如果你看一下我们对行的定义 ， 我们为用户名分配了正好 32 个字节 ， 为\
电子邮件分配了正好 255 个字节 。 但是 ， C 语言的字符串应该以空字符结束 ， 而我们并\
没有为它分配空间 。 解决的办法是多分配一个字节 :

.. code-block:: C 

    typedef struct
    {
        uint32_t id;
        char username[COLUMN_USERNAME_SIZE + 1];
        char email[COLUMN_EMAIL_SIZE + 1];
    } Row;

而这确实解决了这个问题 。 

.. code-block:: shell

    bundle exec rspec
    ...

    Finished in 0.0188 seconds (files took 0.08516 seconds to load)
    3 examples, 0 failures

我们不应该允许插入比列大小更长的用户名或电子邮件 。 这方面的规范是这样的 :

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

为了做到这一点 ， 我们需要升级我们的分析器 。 作为提醒 ， 我们目前正在使用 sscanf() 。

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

未完待续 ...

上一篇文章 ： `上一篇`_

下一篇文章 ： `下一篇`_ 

.. _`上一篇`: Database-In-C-01.rst
.. _`下一篇`: Database-In-C-03.rst
