*******************************************************************************
Part 02 - 世界上最简单的 SQL 编译器和虚拟机 
*******************************************************************************

.. contents:: 目录
    :depth: 3
    :backlinks: top

我们正在制作一个 SQLite 的克隆体， SQLite 的前端是一个 SQL 编译器， 用于解析一个字\
符串， 输出一个叫做字节码的内部表示法。 

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

接下来， 我们添加一个步骤， 将输入行转换为我们内部的语句表示。 这就是我们的黑客版本\
的 SQLite 前端。 

最后， 我们将准备好的语句传递给 ``execute_statement``。 这个函数最终将成为我们的虚\
拟机。 

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

注意， 我们用 strncmp 来表示 "insert" 命令， 因为 "insert" 关键词后面会有数据。 (\
例如: ``insert 1 cstack foo@bar.com``)

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

.. code-block:: bash

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

我们的数据库的骨架正在形成 ...... 如果它能存储数据， 那不是很好吗? 在下一部分 ， 我\
们将实现插入和选择， 创建世界上最糟糕的数据存储。 同时， `这里[1]`_ 是本部分的全部内\
容。 

.. _这里[1]: https://github.com/iloeng/SimpleDB/commit/81af30cabcec1b9700f72472fb668cc3c02d602c
