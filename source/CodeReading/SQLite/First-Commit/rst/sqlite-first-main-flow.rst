###############################################################################
SQLite First Commit 源码阅读笔记
###############################################################################

.. contents::

*******************************************************************************
第 1 部分  源码阅读环境 
*******************************************************************************

我的源码阅读环境为 ： WSL2 + CLion on Windows 10

可编译代码已包含在此仓库中。

*******************************************************************************
第 2 部分  开始阅读源码
*******************************************************************************

SQLite 项目是一个纯 C 项目， 我们从 main 函数开始看起。

.. _main-func:
.. main-func

2.1 main 函数
===============================================================================

main 函数的代码如下：

.. topic:: [src/shell.c]

    .. code-block:: c 

        int main(int argc, char **argv){
            sqlite *db;
            char *zErrMsg = 0;
            struct callback_data data;

            if( argc!=2 && argc!=3 ){
                fprintf(stderr,"Usage: %s FILENAME ?SQL?\n", *argv);
                exit(1);
            }
            db = sqlite_open(argv[1], 0666, &zErrMsg);
            if( db==0 ){
                fprintf(stderr,"Unable to open database \"%s\": %s\n", argv[1], zErrMsg);
                exit(1);
            }
            memset(&data, 0, sizeof(data));
            data.out = stdout;
            if( argc==3 ){
                data.mode = MODE_List;
                strcpy(data.separator,"|");
                if( sqlite_exec(db, argv[2], callback, &data, &zErrMsg)!=0 && zErrMsg!=0 ){
                fprintf(stderr,"SQL error: %s\n", zErrMsg);
                exit(1);
                }
            }else{
                char *zLine;
                char *zSql = 0;
                int nSql = 0;
                int istty = isatty(0);
                data.mode = MODE_Line;
                strcpy(data.separator,"|");
                data.showHeader = 0;
                if( istty ){
                printf(
                    "Enter \".help\" for instructions\n"
                );
                }
                while( (zLine = readline(istty ? (zSql==0 ? "sql> " : ".... ") : 0))!=0 ){
                if( zLine && zLine[0]=='.' ){
                    do_meta_command(zLine, db, &data);
                    free(zLine);
                    continue;
                }
                if( zSql==0 ){
                    nSql = strlen(zLine);
                    zSql = malloc( nSql+1 );
                    strcpy(zSql, zLine);
                }else{
                    int len = strlen(zLine);
                    zSql = realloc( zSql, nSql + len + 2 );
                    if( zSql==0 ){
                    fprintf(stderr,"%s: out of memory!\n", *argv);
                    exit(1);
                    }
                    strcpy(&zSql[nSql++], "\n");
                    strcpy(&zSql[nSql], zLine);
                    nSql += len;
                }
                free(zLine);
                if( sqlite_complete(zSql) ){
                    data.cnt = 0;
                    if( sqlite_exec(db, zSql, callback, &data, &zErrMsg)!=0 
                        && zErrMsg!=0 ){
                    printf("SQL error: %s\n", zErrMsg);
                    free(zErrMsg);
                    zErrMsg = 0;
                    }
                    free(zSql);
                    zSql = 0;
                    nSql = 0;
                }
                }
            }
            sqlite_close(db);
            return 0;
        }
