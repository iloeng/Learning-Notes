cmake_minimum_required(VERSION 3.18)
project(SQLite-1.0)

set(CMAKE_C_STANDARD 11)

include_directories(src)

add_executable(SQLite-1.0
        src/build.c
        src/dbbe.c
        src/dbbe.h
        src/delete.c
        src/expr.c
        src/insert.c
        src/main.c
        src/select.c
        src/shell.c
        src/sqliteInt.h
        src/tclsqlite.c
        src/tokenize.c
        src/update.c
        src/util.c
        src/vdbe.c
        src/vdbe.h
        src/where.c
        tool/gdbmdump.c
        tool/lemon.c
        tool/lempar.c)
