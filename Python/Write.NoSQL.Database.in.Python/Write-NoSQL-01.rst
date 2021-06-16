##############################################################################
使用 Python 编写 NoSQL 数据库
##############################################################################

注 ： 

- 本文发现于 : project-based-learning_
- 本文原始链接 : `Write a NoSQL Database in Python`_ 
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

这将为我们提供那辆车的特定属性 。 

主键被定义为唯一的 。 也就是说 ， 具有特定 VIN 的特定汽车最多只能出现在表中一次 。 \
为什么这很重要 ? 让我们看一个例子 ： 

Relations
------------------------------------------------------------------------------

想象一下 ， 我们正在经营一家汽车维修业务 。 除其他事项外 ， 我们需要跟踪车辆的服务历\
史 ： 我们对该车进行的所有维修和调整的记录 。 我们可以创建一个包含以下列的 \
``ServiceHistory`` 表 ： 

- VIN
- Make
- Model
- Year
- Color
- Service Performed
- Mechanic
- Price
- Date

因此 ， 每次汽车进厂维修时 ， 我们都会在表中添加一个新行 ， 其中包含汽车的所有信息以\
及我们对它做了什么 、 机械师是谁 、 费用多少以及何时执行服务 。

可是等等 。 对于同一辆车 ， 与汽车本身相关的所有列始终相同 。 也就是说 ， 如果我将我\
的 Black 2014 Lexus RX 350 维修 10 次 ， 我每次都需要记录品牌 、 型号 、 年份和颜\
色 ， 即使它们不会改变 。 与其重复所有这些信息 ， 不如将其存储一次并在必要时进行查找 。

我们将如何做到这一点 ? 我们将创建第二个表 ： ``Vehicle`` ， 包含以下列 ： 

- VIN
- Make
- Model
- Year
- Color

对于 ``ServiceHistory`` 表 ， 我们现在要缩减为以下列 ： 

- VIN
- Service Performed
- Mechanic
- Price
- Date

为什么 VIN 出现在两个表中 ? 因为我们需要一种方法来指定 ``ServiceHistory`` 表中的这\
辆车是指 ``Vehicle`` 表中的那辆车 。 这样 ， 我们只需存储一次特定汽车的信息 。 每次\
维修时 ， 我们在 ``ServiceHistory`` 表中创建一个新行 ， 而不是 ``Vehicle`` 表 ； \
毕竟是同一辆车 。 

我们还可以发出跨越 ``Vehicle`` 和 ``ServiceHistory`` 之间隐式关系的查询 ： 

.. code-block:: SQL 

    SELECT Vehicle.Model, Vehicle.Year FROM Vehicle, ServiceHistory WHERE Vehicle.VIN = ServiceHistory.VIN AND ServiceHistory.Price > 75.00;

此查询旨在确定维修成本大于 75.00 美元的所有汽车的型号和年份 。 请注意 ， 我们指定将 \
``Vehicle`` 表中的行与 ``ServiceHistory`` 表中的行进行匹配的方式是匹配 ``VIN`` 值 \
。 它返回给我们的是一组包含两个表列的行 。 我们通过说我们只需要 "Vehicle" 表的 \
"Model" 和 "Year" 列来改进它 。 

如果我们的数据库没有索引 (或更准确地说 ， 没有索引) ， 上面的查询将需要执行表扫描以\
定位与我们的查询匹配的行 。 表扫描是按顺序检查表中的每一行 ， 并且速度非常慢 。 事实\
上 ， 它们代表了最慢的查询执行方法 。

可以通过在列或列集上使用索引来避免表扫描 。 将索引视为允许我们通过对值进行预排序来非\
常快速地在索引列中找到特定值 (或值范围) 的数据结构 。 也就是说 ， 如果我们在 Price \
列上有一个索引 ， 而不是一次查看所有行以确定价格是否大于 ``75.00`` ， 我们可以简单地\
使用索引中包含的信息来 "跳转" 到价格大于 ``75.00`` 的第一行并返回每个后续行 (价格至\
少高达 ``75.00`` ， 因为索引已排序) 。 

在处理大量数据时 ， 索引成为提高查询速度不可或缺的工具 。 然而 ， 与所有事物一样 ， \
它们也是有代价的 ： 索引的数据结构会消耗内存 ， 否则这些内存可用于在数据库中存储更多\
数据 。 这是一种必须在每种情况下进行检查的权衡 ， 但对经常查询的列进行索引是很常见的 。

The Clear Box
------------------------------------------------------------------------------

由于数据库能够检查表的模式 (每列保存的数据类型的描述) 并根据数据做出合理的决策 ， 因\
此索引等高级功能成为可能 。 也就是说 ， 对于数据库来说 ， 表是 "黑盒" (明盒?) 的对立\
面 。 

当我们谈论 NoSQL 数据库时 ， 请记住这一事实 。 它成为有关查询不同类型数据库引擎的能\
力的讨论的重要部分 。 

Schemas
------------------------------------------------------------------------------

我们了解到 ， 表的模式是对列名称及其包含的数据类型的描述 。 它还包含诸如哪些列可以为\
空 、 哪些列必须唯一以及对列值的所有其他约束等信息 。 在任何给定时间 ， 一张表可能只\
有一个模式 ， 并且表中的所有行都必须符合该模式 。 

这是一个重要的限制 。 假设您有一个包含数百万行客户信息的数据库表 。 您的销售团队希望\
开始捕获额外的数据 (例如 ， 用户的年龄) 以提高他们的电子邮件营销算法的精确度 。 这需\
要您通过添加列来更改表 。 您还需要决定表中的每一行是否需要该列的值 。 很多时候 ， 需\
要一列是有意义的 ， 但这样做需要我们根本无法访问的信息 (例如数据库中每个用户的年龄) \
。 因此 ， 在这方面经常进行权衡 。 

此外 ， 对非常大的数据库表进行模式更改很少是一件简单的事情 。 制定一个万一出现问题的\
回滚计划很重要 ， 但架构更改一旦发生就无法撤消 。 模式维护可能是 DBA 工作中最困难的\
部分之一 。 

Key/Value Stores
==============================================================================

早在 "NoSQL" 这个术语出现之前 ， 像 memcached 这样的键 / 值数据存储就提供了数据存\
储 ， 而没有表模式的开销 。 事实上 ， 在 K/V 存储中 ， 根本没有 "Tables" 。 只有键\
和值 。 如果键 / 值存储听起来很熟悉 ， 那是因为它建立在与 Python 的 dict 和 set 类\
相同的原则之上 ： 使用哈希表提供对数据的基于键的快速访问 。 最原始的基于 Python 的 \
NoSQL 数据库只是一个大字典 。 

要了解它们的工作原理 ， 让我们自己编写一个 ！ 我们将从一个非常简单的设计开始 ： 

- 作为主要数据存储的 Python 字典
- 只支持字符串作为键
- 支持存储整数 、 字符串和列表
- 使用 ASCII 字符串进行消息传递的简单 TCP/IP 服务器
- INCREMENT 、 DELETE 、 APPEND 和 STATS 等稍微高级的命令

使用基于 ASCII 的 TCP/IP 接口构建数据存储的好处是我们可以使用简单的 telnet 程序与我\
们的服务器进行交互 ； 不需要特殊的客户端 (尽管写一个是一个很好的练习 ， 可以在大约 \
15 行内完成) 。

对于我们发送到服务器的消息和它发回的响应 ， 我们需要一个 "有线格式" 。 这是一个松散的\
规范 ： 

Commands Supported
------------------------------------------------------------------------------

- PUT

  - 参数 : Key , Value 
  - 目的 : 将新条目插入数据存储

- GET

  - 参数 : Key 
  - 目的 : 从数据存储中检索存储的值

- PUTLIST

  - 参数 : Key , Value 
  - 目的 : 在数据存储中插入一个新的列表条目

- GETLIST

  - 参数 : Key
  - 目的 : 从数据存储中检索存储的列表

- APPEND

  - 参数 : Key , Value 
  - 目的 : 将元素添加到数据存储中的现有列表

- INCREMENT

  - 参数 : Key
  - 目的 : 增加数据存储中整数值的值

- DELETE

  - 参数 : Key
  - 目的 : 从数据存储中删除条目

- STATS

  - 参数 : N/A
  - 目的 : 请求统计每个命令执行成功 / 失败的次数

现在让我们定义消息结构本身 。 

Message Structure
------------------------------------------------------------------------------

Request Messages
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

请求消息由命令 、 键 、 值和值类型组成 。 根据消息的不同最后三个是可选的 。 ``;`` \
被用作分隔符 。 必须总是三个 ``;`` 在消息中 ， 即使不包含某些可选字段 。 

.. code-block:: bash 

    COMMAND;[KEY];[VALUE];[VALUE TYPE]

- **COMMAND** : 是上面列表中的命令
- **KEY** : 是用作数据存储键的字符串 (可选)
- **VALUE** : 是要存储在数据存储中的整数 、 列表或字符串 (可选)
  
  - 列表表示为以逗号分隔的一系列字符串 ， 例如 "red,green,blue"

- **VALUE TYPE** :  描述应该解释为什么类型的 **VALUE**

  - 可能的值 ： INT , STRING , LIST

Example
""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

.. code-block:: SQL 

    "PUT;foo;1;INT"

    "GET;foo;;"

    "PUTLIST;bar;a,b,c;LIST"

    "APPEND;bar;d;STRING

    "GETLIST;bar;;"

    "STATS;;;"

    "INCREMENT;foo;;"

    "DELETE;foo;;"

Response Messages
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

响应消息由两部分组成 ， 以 ``;`` 分隔 。 根据命令是否成功 ， 第一部分始终为 \
``True|False`` 。 第二部分是命令消息 。 关于错误 ， 这将描述错误 。 对于不希望返回\
值的成功命令 (如 PUT) ， 这将是一条成功消息 。 对于期望返回值的命令 (如 GET) ， 这\
将是值本身 。 

Example
""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

.. code-block:: bash 

    "True;Key [foo] set to [1]"

    "True;1"

    "True;Key [bar] set to [['a', 'b', 'c']]"

    "True;Key [bar] had value [d] appended"

    "True;['a', 'b', 'c', 'd']

    "True;{'PUTLIST': {'success': 1, 'error': 0}, 'STATS': {'success': 0, 'error': 0}, 'INCREMENT': {'success': 0, 'error': 0}, 'GET': {'success': 0, 'error': 0}, 'PUT': {'success': 0, 'error': 0}, 'GETLIST': {'success': 1, 'error': 0}, 'APPEND': {'success': 1, 'error': 0}, 'DELETE': {'success': 0, 'error': 0}}"

Show Me The Code!
==============================================================================

我将以可消化的块呈现代码 。 整个服务器只有不到 180 行代码 ， 因此可以快速阅读 。 

Set Up
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

以下是我们服务器所需的许多样板设置代码 ： 

.. code-block:: python 

    """NoSQL database written in Python."""

    # Standard library imports
    import socket

    HOST = 'localhost'
    PORT = 50505
    SOCKET = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    STATS = {
        'PUT': {'success': 0, 'error': 0},
        'GET': {'success': 0, 'error': 0},
        'GETLIST': {'success': 0, 'error': 0},
        'PUTLIST': {'success': 0, 'error': 0},
        'INCREMENT': {'success': 0, 'error': 0},
        'APPEND': {'success': 0, 'error': 0},
        'DELETE': {'success': 0, 'error': 0},
        'STATS': {'success': 0, 'error': 0},
        }

这里没什么可看的 ， 只是导入和一些数据初始化 。 

