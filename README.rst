###############################################################################
Learning Notes
###############################################################################

.. contents:: 

.. note:: 

    本仓库用于记录本人学习笔记， 内容比较杂。 建议查看 http://docs.liangz.org。

    github 由于渲染问题， 部分样式不太美观

*******************************************************************************
约定
*******************************************************************************

1. 使用 vscode 作为编辑器
2. 每行 79 个 ASCII 字符长度
3. 以每行最后一个可见文字作为分割， 在其前插入 rst 文档连接符 '\\' 并回车 ， 作为行\
   的分割
4. "\\" 符号不能超出 vscode 79 字符分割线
5. 符号后添加空格
6. 以上约定在代码段中忽略

符号使用： 

.. code-block:: 

  # with overline, for parts
  * with overline, for chapters
  =, for sections
  -, for subsections
  ^, for subsubsections
  ", for paragraphs

暂时这样约定

*******************************************************************************
目录
*******************************************************************************

.. list-table:: 笔记列表
    :widths: auto
    :header-rows: 1
    :align: center

    * - 序号
      - 语言
      - 名称
    * - 001
      - C
      - |C-Hash-Table|_
    * - 002
      - C
      - `Let's Build a Simple Database`_
    * - 003
      - Python
      - `Write NoSQL Database in Python`_
    * - 004
      - Python
      - `A Simple BitTorrent Decoder`_
    * - 005
      - Python
      - `A BitTorrent client in Python 3.5`_
    * - 006
      - Golang
      - |BitTorrent-in-Go|_

.. |BitTorrent-in-Go| replace:: Building a BitTorrent client from the ground up in Go
.. _`BitTorrent-in-Go`: source/Other/BitTorrent-in-Go
.. _`A BitTorrent client in Python 3.5`: source/Python/BitTorrent-client-in-Python3.5
.. _`A Simple BitTorrent Decoder`: source/Python/Simple-BitTorrent-Decoder
.. _`Write NoSQL Database in Python`: source/Python/Write.NoSQL.Database.in.Python
.. _`Let's Build a Simple Database`: source/C/Let's.Build.a.Simple.Database
.. |C-Hash-Table| replace:: Write a Hash Table
.. _`C-Hash-Table`: source/C/Write.a.Hash.Table

*******************************************************************************
TODO 计划
*******************************************************************************

- Flask

  - Flask 0.1 - Python - Done
  - Flask 0.4 - Python
  - Flask 0.5 - Python
  - Flask 0.7 - Python
  - Flask 1.0 - Python

- TinyHTTPd 0.1.0 - C - Done
- TinyDB

  - TinyDB 1.0.0 - Python - 50%

- Asyncio - Python
- Pieces - Python 
- Python - C 
- Nginx - C 
- Redis - C 
- Lua - C 
- SQLite - C
- Python Standard Library - Python

  - String - Python
  - SocketServer - Python 3 - Done
  - Socket - Python 3

- Git - C
