##############################################################################
C 语言从头写一个 SQLite 程序
##############################################################################

.. contents::

******************************************************************************
第 04 部分  第一个测试 (和 BUG)
******************************************************************************

但是 sscanf 也有一些 缺点_ 。 如果它所读取的字符串大于它所读入的缓冲区 ， 它将导致缓冲\
区溢出 ， 并开始写到意外的地方 。 我们想在复制到 Row 结构之前检查每个字符串的长度 。 \
而要做到这一点 ， 我们需要将输入的内容除去空格 。 

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

