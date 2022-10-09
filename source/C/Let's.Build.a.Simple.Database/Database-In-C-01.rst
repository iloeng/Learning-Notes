###############################################################################
C 语言从头写一个 SQLite 程序
###############################################################################

源项目来自于: https://cstack.github.io/db_tutorial/， 被 `cstack`_ 维护。 本仓库\
仅作为本人学习时文档， 全部以中文书写， 相当于翻译。 

.. _`cstack`: https://github.com/cstack

.. contents:: 目录
    :depth: 3
    :backlinks: top

*******************************************************************************
第 00 部分  数据库是怎么工作的
*******************************************************************************

- 数据是以什么格式存储的? (在内存中和硬盘中)
- 它什么时候从内存中移动到磁盘?
- 为什么一个数据表只能有一个主键 (primary key)?
- 回滚是如何进行的?
- 索引的格式是怎样的?
- 全表扫描是何时以及如何进行的?
- 准备好的报表是以什么格式保存的?

总而言之， 一个数据库是怎么工作的?

为了理解数据库工作原理， 我正在用 C 语言从头开始建立一个克隆的 SQLite， 我将在文档中\
记录我的过程。 

    "What I cannot create, I do not understand." – Richard Feynman

.. figure:: img/arch.svg
    :align: center
   
    sqlite architecture (https://www.sqlite.org/arch.html)

*******************************************************************************
第 01 部分  REPL 的介绍和设置
*******************************************************************************

作为一个 Web 开发者， 在工作中我每天使用关系型数据库， 但是它们对于我来说是一个黑盒。 \
我有一些问题： 

- 数据是以什么格式存储的? (在内存中和硬盘中)
- 它什么时候从内存中移动到磁盘?
- 为什么一个数据表只能有一个主键 (primary key)?
- 回滚是如何进行的?
- 索引的格式是怎样的?
- 全表扫描是何时以及如何进行的?
- 准备好的报表是以什么格式保存的?

总而言之， 一个数据库是怎么工作的?

为了弄清这些问题， 我正在从头开始写一个数据库。 它以 SQLite 为模型， 因为它被设计成比 \
MySQL 或 PostgreSQL 更小的功能， 所以我有更好的希望了解它。 整个数据库被存储在一个文\
件中。

1.1 SQLite
===============================================================================

SQLite 官方网站上有很多关于 SQLite 的 `内部文档`_， 另外我还有一本 \
`《SQLite 数据库系统： 设计与实现》`_。

.. _`内部文档`: https://www.sqlite.org/arch.html
.. _`《SQLite 数据库系统： 设计与实现》`: https://play.google.com/store/books/details?id=9Z6IQQnX1JEC

.. figure:: img/arch1.gif
    :align: center

    sqlite architecture (https://www.sqlite.org/zipvfs/doc/trunk/www/howitworks.wiki)

一个查询要经过一连串的组件， 以检索或修改数据。 前端由以下部分组成: 

- tokenizer
- parser
- code generator

前端的输入是一个 SQL 查询。 输出是 sqlite 虚拟机字节码 (基本上是一个可以在数据\
库上操作的编译程序)。 

后端由以下部分组成: 

- virtual machine
- B-tree
- pager
- os interface

虚拟机把由前端生成的字节码作为指令。 然后它可以对一个或多个表或索引进行操作， 每个表或\
索引都存储在一个叫做 B 树的数据结构中。 虚拟机本质上是一个关于字节码指令类型的大开关语\
句。 

每个 B 型树由许多节点组成。 每个节点都是一个页面的长度。 B 树可以通过向 pager 发出命\
令从磁盘上检索一个页面或将其保存回磁盘。 

pager 接收读取或写入数据页的命令。 它负责在数据库文件的适当偏移处进行读 / 写。 它还在\
内存中保存最近访问的页面的缓存， 并决定这些页面何时需要写回磁盘。 

系统接口是根据 SQLite 是在哪个操作系统上编译而不同的一层。 在本教程中， 我不打算支持\
多个平台。 

千里之行始于足下， 所以让我们从更直接的东西开始： REPL。 

1.2 制作一个简单的 REPL
===============================================================================

当你从命令行启动 SQLite 时， 它会启动一个读取-执行-打印的循环。 

.. code-block:: shell

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

为了做到这一点， 我们的主函数将有一个无限循环， 打印提示信息， 获得一行输入， 然后处理\
这一行输入。 

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

我们将定义 InputBuffer 作为一个小的围绕我们需要存储的状态的包装， 与 ``getline()`` \
进行交互。 稍后会有更多关于这个问题的内容。 

接下来， ``print_prompt()`` 向用户打印一个提示。 我们在读取每一行的输入之前做这个。 

.. code-block:: C 

    void print_prompt() { printf("db > "); }

使用 ``getline()`` 读取一行输入:

.. code-block:: C 

    ssize_t getline(char **lineptr, size_t *n, FILE *stream);

lineptr: 指向变量的指针， 我们用它来指向包含读行的缓冲区。 如果它被设置为 NULL ， 那\
么它就会被 ``getline`` 所 ``mallocat``， 因此应该被用户释放， 即使命令失败。 

n: 一个指向变量的指针， 我们用它来保存分配的缓冲区的大小。 

stream: 读取的输入流。 我们将从标准输入中读取。 

返回值: 读取的字节数， 这可能小于缓冲区的大小。 

我们告诉 ``getline`` 在 ``input_buffer->buffer`` 中存储读取的行， 在 \
``input_buffer->buffer_length`` 中存储分配的缓冲区的大小。 我们将返回值存储在 \
``input_buffer->input_length`` 中。

buffer 开始时是空的， 所以 ``getline`` 分配了足够的内存来容纳这一行的输入， 并使 \
buffer 指向它。 

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

现在应该定义一个函数， 用于释放为 ``InputBuffer *`` 实例和相应结构的缓冲区元素分配的\
内存 (``getline`` 在 ``read_input`` 中为 ``input_buffer->buffer`` 分配内存)。

.. code-block:: C 

    void close_input_buffer(InputBuffer* input_buffer) {
        free(input_buffer->buffer);
        free(input_buffer);
    }

最后， 我们解析并执行该命令。 现在只有一个公认的命令： ``.exit``， 它可以终止程序 。 \
否则， 我们会打印一个错误信息并继续循环。 

.. code-block:: C 

    if (strcmp(input_buffer->buffer, ".exit") == 0) {
        close_input_buffer(input_buffer);
        exit(EXIT_SUCCESS);
    } else {
        printf("Unrecognized command '%s'.\n", input_buffer->buffer);
    }

让我们来试试吧! 

.. code-block:: shell

    ~ ./db
    db > .tables
    Unrecognized command '.tables'.
    db > .exit
    ~

好了， 我们已经有了一个可工作的 REPL。 在下一部分， 我们将开始开发我们的命令语言。 同\
时， 这里是本部分的整个程序:

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
