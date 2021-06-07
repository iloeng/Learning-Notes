##############################################################################
使用 Python 编写 NoSQL 数据库
##############################################################################

注 ： 

- 本文发现于 project-based-learning_
- 本文的原始链接 : `Write a NoSQL Database in Python`_ 
- 本文存档链接 : `Write a NoSQL Database in Python Archive`_

.. _project-based-learning: https://github.com/tuvtran/project-based-learning
.. _`Write a NoSQL Database in Python`: https://jeffknupp.com/blog/2014/09/01/what-is-a-nosql-database-learn-by-writing-one-in-python/
.. _`Write a NoSQL Database in Python Archive`: https://web.archive.org/web/20200414132138/https://jeffknupp.com/blog/2014/09/01/what-is-a-nosql-database-learn-by-writing-one-in-python//

.. contents::

什么是 NoSQL 数据库 ? 通过用 Python 编写一个来学习它

NoSQL 是一个近年来变得无处不在的术语 。 但是 "NoSQL" 实际上是什么意思 ? 它如何以及\
为什么有用 ? 在本文中 ， 我们将通过用纯 Python (或者 ， 我喜欢称之为 "稍微结构化的\
伪代码") 创建一个玩具 NoSQL 数据库来回答这些问题 。 

OldSQL
==============================================================================

对大多数人来说 ， SQL 是 "数据库" 的同义词 。 然而 ， SQL 是结构化查询语言的首字母\
缩写词 ， 它本身并不是一种数据库技术 。 相反 ， 它描述了从 RDBMS 或关系数据库管理系\
统中检索数据的语言 。 MySQL 、 PostgreSQL 、 MS SQL Server 和 Oracle 都是 RDBMS \
的示例 。 

首字母缩略词 RDBMS 中的 "Relational" 一词提供的信息最多 。 数据被组织成表 ， 每个\
表都有一组具有关联类型的列 。 所有表 、 它们的列和列的类型的描述被称为数据库的模式 \
。 模式完整地描述了数据库的结构 ， 每个表都有一个描述 。 例如 ， 一个 ``Car`` 表可\
能有以下列 ： 

- Make : a string
- Model : a string
- Year : a four-digit number; alternatively, a date
- Color : a string
- VIN (Vehicle Identification Number): a string

表中的单个条目称为行或记录 。 为了区分一个记录和另一个记录 ， 通常定义一个主键 。 表\
的主键是唯一标识每一行的其中一列 (或其组合) 。 在 ``Car`` 表中 ， VIN 是表的主键的\
自然选择 ， 因为它保证在汽车之间是唯一的 。 两行可能共享完全相同的 Make 、 Model 、 \
Year 和 Color 值 ， 但指的是不同的汽车 ， 这意味着它们将具有不同的 VIN 。 如果两行\
具有相同的 VIN ， 我们甚至不必检查其他列 ， 它们必须指同一辆车 。 

Querying
------------------------------------------------------------------------------

SQL 让我们查询这个数据库以获得有用的信息 。 查询只是意味着以结构化语言向 RDBMS 提出\
问题 ， 并将其返回的行解释为答案 。 想象一下 ， 数据库代表在美国注册的所有车辆 。 要\
获取所有记录 ， 我们可以针对数据库编写以下 SQL 查询 ： 

.. code-block:: SQL 

    SELECT Make, Model FROM Car;

将 SQL 翻译成简单的英语可能是 ： 

- "SELECT": "Show me"
- "Make, Model": "the value of Make and Model"
- "FROM Car": "for each row in the Car table"

或者 ， "向我显示 Car 表中每一行的 Make 和 Model 的值" 。 我们会得到一个结果列表 ， \
每个结果都有品牌和型号 。 如果我们只关心 1994 年汽车的颜色 ， 我们可以说 ： 

.. code-block:: SQL 

    SELECT Color FROM Car WHERE Year = 1994;

在这种情况下 ， 我们会得到如下一个列表 ：

.. code-block:: 

    Black
    Red
    Red
    White
    Blue
    Black
    White
    Yellow

最后 ， 使用表的主键 ， 我们可以通过查找 VIN 来查找特定的汽车 ： 

.. code-block:: SQL 

    SELECT * FROM Car where VIN = '2134AFGER245267';    

