Python 源码阅读系列 1
---------------------------------

前言
===================

此文档为学习 《Python 源码剖析》 笔记。 代码基于 Python 2.5

Python 源代码组织
===================

Python 2.5 的代码结构如下：

.. image:: img/code-structure.png

- **Include** : 该目录下包含了 Python 提供的所有头文件，如果用户需要自己用 \
  C 或 C++ 来编写自定义模块拓展 Python ，那么久需要用到这里提供的文件。

- **Lib** : 该目录包含了 Python 自带的所有标准库， Lib 中的库都是用 Python \
  语言编写的。

- **Modules** : 该目录中包含了所有用 C 语言编写的模块，比如 ``random`` 、 \
  ``cStringIO`` 等。 ``Modules`` 中的模块是那些对速度要求非常严格的模块，而\
  有一些对速度没有太严格要求的模块，比如 ``os`` ，就是用 Python 编写， 而且\
  放在 Lib 目录下。

- **Parser** : 该目录中包含了 Python 解释器中的 Scanner 和 Parser 部分，即\
  对 Python 源代码进行词法分析和语法分析的部分，。除了这些， Parser 目录下还\
  包含了一些有用的工具，这些工具能够根据 Python 语言的语法自动生成 Python 语\
  言的词法和语法分析器，于 YACC 非常类似。

- **Objects** : 该目录中包含了所有 Python 的内建对象，包括整数、List、Dict\
  等。同时，该目录还包括了 Python 在运行时所需要的所有的内部使用对象的实现。

- **Python** : 该目录下包含了 Python 解释器中的 Compiler 和执行引擎部分，是\
  Python 运行的核心所在。

- **PCBuild** : 包含了 Visual Studio 2003 的工程文件，研究 Python 源代码就\
  从这里开始（书中使用 VS2003 对 Python 进行编译）

- **PCBuild8** : 包含了 Visual Studio 2005 使用的工程文件

