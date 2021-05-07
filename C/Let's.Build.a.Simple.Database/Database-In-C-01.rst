##############################################################################
C 语言从头写一个 SQLite 程序
##############################################################################

源项目来自于 : https://cstack.github.io/db_tutorial/ ， 被 `cstack`_ 维护 。 本\
仓库仅作为本人学习时文档 ， 全部以中文书写 ， 相当于翻译 。 

.. _`cstack`: https://github.com/cstack

- `Part 01 REPL 的介绍和设置`_
- `Part 02 世界上最简单的 SQL 编译器和虚拟机`_ 
- `Part 03 一个内存中的只加单表的数据库`_
- `Part 04 第一个测试 (和 BUG)`_
- `Part 05 持久化到磁盘`_
- `Part 06 游标抽象`_
- `Part 07 B 型树简介`_
- `Part 08 B 型树叶子节点格式`_
- `Part 09 二进制搜索和重复 Key`_
- `Part 10 分割一个叶子节点`_
- `Part 11 递归搜索 B 型树`_
- `Part 12 扫描多层次的 B 型树`_
- `Part 13 分割后更新父节点`_

.. _`Part 01 REPL 的介绍和设置`: #repl
.. _`Part 02 世界上最简单的 SQL 编译器和虚拟机`: #sql
.. _`Part 03 一个内存中的只加单表的数据库`: Database-In-C-02.rst#id5
.. _`Part 04 第一个测试 (和 BUG)`: Database-In-C-02.rst#id6
.. _`Part 05 持久化到磁盘`:
.. _`Part 06 游标抽象`:
.. _`Part 07 B 型树简介`:
.. _`Part 08 B 型树叶子节点格式`:
.. _`Part 09 二进制搜索和重复 Key`:
.. _`Part 10 分割一个叶子节点`:
.. _`Part 11 递归搜索 B 型树`:
.. _`Part 12 扫描多层次的 B 型树`:
.. _`Part 13 分割后更新父节点`:

.. contents::

******************************************************************************
第 00 部分  数据库是怎么工作的
******************************************************************************

- 数据是以什么格式存储的 ? (在内存中和硬盘中)
- 它什么时候从内存中移动到磁盘 ?
- 为什么一个数据表只能有一个主键 (primary key) ?
- 回滚是如何进行的 ?
- 索引的格式是怎样的 ?
- 全表扫描是何时以及如何进行的 ?
- 准备好的报表是以什么格式保存的 ?

总而言之 ， 一个数据库是怎么工作的 ?

为了理解数据库工作原理 ， 我正在用 C 语言从头开始建立一个克隆的 SQLite ， 我将在\
文档中记录我的过程 。 

..
    
    "What I cannot create, I do not understand." – Richard Feynman

.. image:: img/arch.svg

sqlite architecture (https://www.sqlite.org/arch.html)

******************************************************************************
第 01 部分  REPL 的介绍和设置
******************************************************************************

作为一个 Web 开发者 ， 在工作中我每天使用关系型数据库 ， 但是它们对于我来说是一个\
黑盒 。 我有一些问题 ： 

- 数据是以什么格式存储的 ? (在内存中和硬盘中)
- 它什么时候从内存中移动到磁盘 ?
- 为什么一个数据表只能有一个主键 (primary key) ?
- 回滚是如何进行的 ?
- 索引的格式是怎样的 ?
- 全表扫描是何时以及如何进行的 ?
- 准备好的报表是以什么格式保存的 ?

总而言之 ， 一个数据库是怎么工作的 ?

为了弄清这些问题 ， 我正在从头开始写一个数据库 。 它以 SQLite 为模型 ， 因为它被\
设计成比 MySQL 或 PostgreSQL 更小的功能 ， 所以我有更好的希望了解它 。 整个数据库\
被存储在一个文件中 。

1.1 SQLite
==============================================================================

SQLite 官方网站上有很多关于 SQLite 的 `内部文档`_ ， 另外我还有一本 \
`《 SQLite 数据库系统 ： 设计与实现 》`_ 。

.. _`内部文档`: https://www.sqlite.org/arch.html
.. _`《 SQLite 数据库系统 ： 设计与实现 》`: https://play.google.com/store/books/details?id=9Z6IQQnX1JEC

.. image:: img/arch1.gif

sqlite architecture (https://www.sqlite.org/zipvfs/doc/trunk/www/howitworks.wiki)

一个查询要经过一连串的组件 ， 以检索或修改数据 。 前端由以下部分组成 : 

- tokenizer
- parser
- code generator

前端的输入是一个 SQL 查询 。 输出是 sqlite 虚拟机字节码 ( 基本上是一个可以在数据\
库上操作的编译程序 ) 。 

后端由以下部分组成 : 

- virtual machine
- B-tree
- pager
- os interface

虚拟机把由前端生成的字节码作为指令 。 然后它可以对一个或多个表或索引进行操作 ， 每\
个表或索引都存储在一个叫做 B 树的数据结构中 。 虚拟机本质上是一个关于字节码指令类\
型的大开关语句 。 

每个 B 型树由许多节点组成 。 每个节点都是一个页面的长度 。 B 树可以通过向 pager \
发出命令从磁盘上检索一个页面或将其保存回磁盘 。 

pager 接收读取或写入数据页的命令 。 它负责在数据库文件的适当偏移处进行读 / 写 。 \
它还在内存中保存最近访问的页面的缓存 ， 并决定这些页面何时需要写回磁盘 。 

系统接口是根据 SQLite 是在哪个操作系统上编译而不同的一层 。 在本教程中 ， 我不打算\
支持多个平台 。 

千里之行始于足下 ， 所以让我们从更直接的东西开始 ： REPL 。 

1.2 制作一个简单的 REPL
==============================================================================

当你从命令行启动 SQLite 时 ， 它会启动一个读取-执行-打印的循环 。 

.. code-block:: C 

    ~ sqlite3
    SQLite version 3.16.0 2016-11-04 19:09:39
    Enter ".help" for usage hints.
    Connected to a transient in-memory database.
    Use ".open FILENAME" to reopen on a persistent database.
    sqlite> create table users (id int, username varchar(255), email varchar(255));
    sqlite> .tables
    users
    sqlite> .exit
    ~

为了做到这一点 ， 我们的主函数将有一个无限循环 ， 打印提示信息 ， 获得一行输入 ， \
然后处理这一行输入 。 

.. code-block:: C 

    int main(int argc, char* argv[]) {
        InputBuffer* input_buffer = new_input_buffer();
        while (true) {
            print_prompt();
            read_input(input_buffer);

            if (strcmp(input_buffer->buffer, ".exit") == 0) {
                close_input_buffer(input_buffer);
                exit(EXIT_SUCCESS);
            } else {
                printf("Unrecognized command '%s'.\n", input_buffer->buffer);
            }
        }
    }

我们将定义 InputBuffer 作为一个小的围绕我们需要存储的状态的包装 ， 与 getline() 进\
行交互 。 稍后会有更多关于这个问题的内容 。 

接下来 ， print_prompt() 向用户打印一个提示 。 我们在读取每一行的输入之前做这个 。 

.. code-block:: C 

    void print_prompt() { printf("db > "); }

使用 getline() 读取一行输入 :

.. code-block:: C 

    ssize_t getline(char **lineptr, size_t *n, FILE *stream);

lineptr: 指向变量的指针 ， 我们用它来指向包含读行的缓冲区 。 如果它被设置为 NULL \
， 那么它就会被 getline 所 mallocat ， 因此应该被用户释放 ， 即使命令失败 。 

n: 一个指向变量的指针 ， 我们用它来保存分配的缓冲区的大小 。 

stream: 读取的输入流 。 我们将从标准输入中读取 。 

返回值 : 读取的字节数 ， 这可能小于缓冲区的大小 。 

我们告诉 getline 在 ``input_buffer->buffer`` 中存储读取的行 ， 在 \
``input_buffer->buffer_length`` 中存储分配的缓冲区的大小 。 我们将返回值存储在 \
``input_buffer->input_length`` 中 。

buffer 开始时是空的 ， 所以 getline 分配了足够的内存来容纳这一行的输入 ， 并使 \
buffer 指向它 。 

.. code-block:: C 

    void read_input(InputBuffer* input_buffer) {
        ssize_t bytes_read =
                getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);

        if (bytes_read <= 0) {
            printf("Error reading input\n");
            exit(EXIT_FAILURE);
        }

        // Ignore trailing newline
        input_buffer->input_length = bytes_read - 1;
        input_buffer->buffer[bytes_read - 1] = 0;
    }

现在应该定义一个函数 ， 用于释放为 ``InputBuffer *`` 实例和相应结构的缓冲区元素分\
配的内存 (getline 在 read_input 中为 ``input_buffer->buffer`` 分配内存) 。

.. code-block:: C 

    void close_input_buffer(InputBuffer* input_buffer) {
        free(input_buffer->buffer);
        free(input_buffer);
    }

最后 ， 我们解析并执行该命令 。 现在只有一个公认的命令 ： ``.exit`` ， 它可以终止\
程序 。 否则 ， 我们会打印一个错误信息并继续循环 。 

.. code-block:: C 

    if (strcmp(input_buffer->buffer, ".exit") == 0) {
        close_input_buffer(input_buffer);
        exit(EXIT_SUCCESS);
    } else {
        printf("Unrecognized command '%s'.\n", input_buffer->buffer);
    }

让我们来试试吧 ! 

.. code-block:: C 

    ~ ./db
    db > .tables
    Unrecognized command '.tables'.
    db > .exit
    ~

好了 ， 我们已经有了一个可工作的 REPL 。 在下一部分 ， 我们将开始开发我们的命令语\
言 。 同时 ， 这里是本部分的整个程序 :

.. code-block:: C 

    #include <stdlib.h>
    #include <stdio.h>
    #include <string.h>
    #include <stdbool.h>

    typedef struct {
        char* buffer;
        size_t buffer_length;
        ssize_t input_length;
    } InputBuffer;

    InputBuffer* new_input_buffer() {
        InputBuffer* input_buffer = (InputBuffer*)malloc(sizeof(InputBuffer));
        input_buffer->buffer = NULL;
        input_buffer->buffer_length = 0;
        input_buffer->input_length = 0;

        return input_buffer;
    }

    void print_prompt() { printf("db > "); }

    void read_input(InputBuffer* input_buffer) {
        ssize_t bytes_read =
                getline(&(input_buffer->buffer), &(input_buffer->buffer_length), stdin);

        if (bytes_read <= 0) {
            printf("Error reading input\n");
            exit(EXIT_FAILURE);
        }

        // Ignore trailing newline
        input_buffer->input_length = bytes_read - 1;
        input_buffer->buffer[bytes_read - 1] = 0;
    }

    void close_input_buffer(InputBuffer* input_buffer) {
        free(input_buffer->buffer);
        free(input_buffer);
    }

    int main(int argc, char* argv[]) {
        InputBuffer* input_buffer = new_input_buffer();
        while (true) {
            print_prompt();
            read_input(input_buffer);

            if (strcmp(input_buffer->buffer, ".exit") == 0) {
                close_input_buffer(input_buffer);
                exit(EXIT_SUCCESS);
            } else {
                printf("Unrecognized command '%s'.\n", input_buffer->buffer);
            }
        }
    }

******************************************************************************
第 02 部分  世界上最简单的 SQL 编译器和虚拟机 
******************************************************************************

我们正在制作一个 SQLite 的克隆体 ， SQLite 的前端是一个 SQL 编译器 ， 用于解析一\
个字符串 ， 输出一个叫做字节码的内部表示法 。 

这个字节码被传递给虚拟机 ， 由它来执行 。 

.. image:: img/arch.svg

sqlite architecture (https://www.sqlite.org/arch.html)

像这样把事情分成两步来做有几个好处 :

- 减少每个部分的复杂性 (例如 ， 虚拟机不担心语法错误) 。
- 允许对常见的查询进行一次编译 ， 并对字节码进行缓存以提高性能 。

考虑到这一点 ， 让我们重构我们的主函数 ， 并在这个过程中支持两个新的关键字 。

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

像 ``.exit`` 这样的非 SQL 语句被称为 "元命令" 。 它们都以点开始 ， 所以我们检查它\
们并在一个单独的函数中处理它们 。 

接下来 ， 我们添加一个步骤 ， 将输入行转换为我们内部的语句表示 。 这就是我们的黑客\
版本的 SQLite 前端 。 

最后 ， 我们将准备好的语句传递给 ``execute_statement`` 。 这个函数最终将成为我们\
的虚拟机 。 

请注意 ， 我们的两个新函数返回的枚举表示成功或失败 。 

.. code-block:: C 

    typedef enum {
        META_COMMAND_SUCCESS,
        META_COMMAND_UNRECOGNIZED_COMMAND
    } MetaCommandResult;

    typedef enum { 
        PREPARE_SUCCESS, 
        PREPARE_UNRECOGNIZED_STATEMENT 
    } PrepareResult;

"Unrecognized statement" ? 这似乎有点像一个异常 。 但是异常是不好的 (而且 C 语言\
甚至不支持异常) ， 所以我在实用的地方使用枚举结果代码 。 如果我的 switch 语句没有\
处理枚举的成员 ， C编译器会抱怨 ， 所以我们可以放心地处理函数的每个结果 。 预计将\
来会有更多的结果代码加入 。 

``do_meta_command`` 只是对现有功能的一个包装 ， 为更多的命令留下了空间 。 

.. code-block:: C  

    MetaCommandResult do_meta_command(InputBuffer* input_buffer) {
        if (strcmp(input_buffer->buffer, ".exit") == 0) {
            exit(EXIT_SUCCESS);
        } else {
            return META_COMMAND_UNRECOGNIZED_COMMAND;
        }
    }

我们的 "prepared statement" 现在只包含一个有两个可能值的枚举 。 当我们允许语句中\
的参数时 ， 它将包含更多的数据 。 

.. code-block:: C 

    typedef enum {
        STATEMENT_INSERT,
        STATEMENT_SELECT
    } StatementType;

    typedef struct {
        StatementType type;
    } Statement;

``prepare_statement`` (我们的 "SQL 编译器") 现在还不理解 SQL 。 事实上 ， 它只理\
解两个词 。 

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

注意 ， 我们用 strncmp 来表示 "insert" 命令 ， 因为 "insert" 关键词后面会有数据 \
。 (例如 : insert 1 cstack foo@bar.com)

最后 ， execute_statement 包含一些步骤 。 

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

请注意 ， 它没有返回任何错误代码 ， 因为还没有什么可能出错 。 

通过这些重构 ， 我们现在可以识别两个新的关键词了 !

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

我们的数据库的骨架正在形成 ...... 如果它能存储数据 ， 那不是很好吗 ? 在下一部分 ， \
我们将实现插入和选择 ， 创建世界上最糟糕的数据存储 。 同时 ， 这里_ 是本部分的全部内\
容 。 

.. _这里: https://github.com/Deteriorator/SimpleDB/commit/81af30cabcec1b9700f72472fb668cc3c02d602c

未完待续 ...

下一篇文章 ： `下一篇`_ 

.. _`下一篇`: Database-In-C-02.rst
