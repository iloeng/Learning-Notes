cmake_minimum_required(VERSION 3.15)
project(SQLite-0.0.1)

set(CMAKE_CXX_STANDARD 14)

add_executable(
        sqlite-0.0.1
        src/build.c
        src/dbbe.h
        src/dbbe.c
        src/main.c
        src/shell.c
        src/sqlite.h
        src/sqliteInt.h
        src/tclsqlite.c
        src/tokenize.c
        src/util.c
        src/vdbe.c
        src/vdbe.h
        src/where.c
)