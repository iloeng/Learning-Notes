##############################################################################
C 语言从头写一个 SQLite 程序
##############################################################################

.. contents::

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

