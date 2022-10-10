*******************************************************************************
Part 00 - 数据库是怎么工作的
*******************************************************************************

.. contents:: 目录
    :depth: 3
    :backlinks: top

..

    How Does a Database Work?

    - What format is data saved in? (in memory and on disk)
    - When does it move from memory to disk?
    - Why can there only be one primary key per table?
    - How does rolling back a transaction work?
    - How are indexes formatted?
    - When and how does a full table scan happen?
    - What format is a prepared statement saved in?

- 数据是以什么格式存储的? (在内存中和硬盘中)
- 它什么时候从内存中移动到磁盘?
- 为什么一个数据表只能有一个主键 (primary key)?
- 回滚是如何进行的?
- 索引的格式是怎样的?
- 全表扫描是何时以及如何进行的?
- 准备好的报表是以什么格式保存的?

..
    
    In short, how does a database work?

总而言之， 一个数据库是怎么工作的?

..

    I’m building a clone of `sqlite`_ from scratch in C in order to understand\
    , and I’m going to document my process as I go.

为了理解数据库工作原理， 我正在用 C 语言从头开始建立一个克隆的 `SQLite`_， 我将在文\
档中记录我的过程。 

.. _SQLite: https://www.sqlite.org/arch.html

..

    "What I cannot create, I do not understand." – Richard Feynman

.. figure:: img/arch.svg
    :align: center
   
    sqlite architecture (https://www.sqlite.org/arch.html)
