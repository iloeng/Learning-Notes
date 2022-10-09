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

  - 可能的值： INT, STRING, LIST

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
------------------------------------------------------------------------------

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

Set Up (Cont'd)
------------------------------------------------------------------------------

我现在将跳过一些代码 ， 以便我可以显示其余的设置代码 。 请注意 ， 它指的是尚不存在的\
函数 。 没关系 ， 因为我在跳来跳去 。 在完整版中 (在最后展示) ， 一切都按正确的顺序\
排列 。 这是设置代码的其余部分 ： 

.. code-block:: python 

  COMMAND_HANDLERS = {
      'PUT': handle_put,
      'GET': handle_get,
      'GETLIST': handle_getlist,
      'PUTLIST': handle_putlist,
      'INCREMENT': handle_increment,
      'APPEND': handle_append,
      'DELETE': handle_delete,
      'STATS': handle_stats,
      }
  DATA = {}

  def main():
      """Main entry point for script."""
      SOCKET.bind((HOST, PORT))
      SOCKET.listen(1)
      while 1:
          connection, address = SOCKET.accept()
          print 'New connection from [{}]'.format(address)
          data = connection.recv(4096).decode()
          command, key, value = parse_message(data)
          if command == 'STATS':
              response = handle_stats()
          elif command in (
              'GET',
              'GETLIST',
              'INCREMENT',
              'DELETE'
                  ):
              response = COMMAND_HANDLERS[command](key)
          elif command in (
              'PUT',
              'PUTLIST',
              'APPEND',
                  ):
              response = COMMAND_HANDLERS[command](key, value)
          else:
              response = (False, 'Unknown command type [{}]'.format(command))
          update_stats(command, response[0])
          connection.sendall('{};{}'.format(response[0], response[1]))
          connection.close()

  if __name__ == '__main__':
      main()

我们创建了通常称为 ``COMMAND_HANDLERS`` 的查找表 。 它的工作原理是将命令的名称与用\
于处理该类型命令的函数相关联 。 所以 ， 例如如果我们得到一个 GET 命令 ， 说 \
``COMMAND_HANDLERS[command](key)`` 和说 ``handle_get(key)`` 是一样的 。 请记住 \
， 函数可以被视为值 ， 并且可以像任何其他值一样存储在 dict 中 。 

在上面的代码中 ， 我决定分别处理需要相同数量参数的每组命令 。 我可以简单地强制所有的 \
``handle_`` 函数接受一个 ``key`` 和 ``value`` ， 我只是决定这样处理函数更清晰 ， \
更容易测试 ， 并且更不容易出错 。 

请注意 ， 套接字代码是最少的 。 尽管我们的整个服务器都是基于 TCP/IP 通信的 ， 但实际\
上与低级网络代码的交互并不多 。 

要注意的最后一件事是如此无害 ， 您可能已经错过了它 ： ``DATA`` 字典 。 这是我们实际\
存储构成数据库的键值对的地方 。 

Command Parser
------------------------------------------------------------------------------

让我们来看看命令解析器 ， 它负责理解传入的消息 ： 

.. code-block:: python 

    def parse_message(data):
        """Return a tuple containing the command, the key, and (optionally) the
        value cast to the appropriate type."""
        command, key, value, value_type = data.strip().split(';')
        if value_type:
            if value_type == 'LIST':
                value = value.split(',')
            elif value_type == 'INT':
                value = int(value)
            else:
                value = str(value)
        else:
            value = None
        return command, key, value

在这里我们可以看到发生了类型转换 。 如果该值是一个列表 ， 我们知道我们可以通过对字符\
串调用 ``str.split(',')`` 来创建正确的值 。 对于 ``int`` ， 我们只是利用 \
``int()`` 可以接受字符串的事实 。 字符串和 ``str()`` 也是如此 。 

Command Handlers
------------------------------------------------------------------------------

下面是命令处理程序的代码 。 它们都非常直接 ， 并且 (希望) 看起来像您期望的那样 。 请\
注意 ， 有大量的错误检查 ， 但肯定不是详尽无遗的 。 在您阅读时 ， 尝试找出代码遗漏的\
错误案例并将其发布在 讨论_ 中 。 

.. _讨论: https://web.archive.org/web/20200414132138/http://discourse.jeffknupp.com/

.. code-block:: python 

    def update_stats(command, success):
        """Update the STATS dict with info about if executing
        *command* was a *success*."""
        if success:
            STATS[command]['success'] += 1
        else:
            STATS[command]['error'] += 1


    def handle_put(key, value):
        """Return a tuple containing True and the message
        to send back to the client."""
        DATA[key] = value
        return (True, 'Key [{}] set to [{}]'.format(key, value))


    def handle_get(key):
        """Return a tuple containing True if the key exists and the message
        to send back to the client."""
        if key not in DATA:
            return(False, 'ERROR: Key [{}] not found'.format(key))
        else:
            return(True, DATA[key])


    def handle_putlist(key, value):
        """Return a tuple containing True if the command succeeded and the message
        to send back to the client."""
        return handle_put(key, value)


    def handle_getlist(key):
        """Return a tuple containing True if the key contained a list and
        the message to send back to the client."""
        return_value = exists, value = handle_get(key)
        if not exists:
            return return_value
        elif not isinstance(value, list):
            return (
                False,
                'ERROR: Key [{}] contains non-list value ([{}])'.format(key, value)
            )
        else:
            return return_value


    def handle_increment(key):
        """Return a tuple containing True if the key's value could be incremented
        and the message to send back to the client."""
        return_value = exists, value = handle_get(key)
        if not exists:
            return return_value
        elif not isinstance(value, int):
            return (
                False,
                'ERROR: Key [{}] contains non-int value ([{}])'.format(key, value)
            )
        else:
            DATA[key] = value + 1
            return (True, 'Key [{}] incremented'.format(key))


    def handle_append(key, value):
        """Return a tuple containing True if the key's value could be appended to
        and the message to send back to the client."""
        return_value = exists, list_value = handle_get(key)
        if not exists:
            return return_value
        elif not isinstance(list_value, list):
            return (
                False,
                'ERROR: Key [{}] contains non-list value ([{}])'.format(key, value)
            )
        else:
            DATA[key].append(value)
            return (True, 'Key [{}] had value [{}] appended'.format(key, value))


    def handle_delete(key):
        """Return a tuple containing True if the key could be deleted and
        the message to send back to the client."""
        if key not in DATA:
            return (
                False,
                'ERROR: Key [{}] not found and could not be deleted'.format(key)
            )
        else:
            del DATA[key]


    def handle_stats():
        """Return a tuple containing True and the contents of the STATS dict."""
        return (True, str(STATS))

需要注意的两件事 ： 多重赋值的使用和代码重用 。 许多函数只是对现有函数的简单包装 ， \
具有更多的逻辑 ， 例如 ``handle_get`` 和 ``handle_getlist`` 。 由于我们偶尔只是发\
回现有函数的结果 ， 而其他时候检查该函数返回的内容 ， 因此使用了多重赋值 。 

看看 ``handle_append`` 。 如果我们尝试调用 ``handle_get`` 并且 Key 不存在 ， 我们\
可以简单地返回 ``handle_get`` 返回的内容 。 因此 ， 我们希望能够将 ``handle_get`` \
返回的元组作为单个返回值引用 。 如果 Key 不存在 ， 我们可以简单地说 \
``return return_value`` 。 

如果它确实存在 ， 我们需要检查返回的值 。 因此 ， 我们还想将 ``handle_get`` 的返回\
值称为单独的变量 。 为了同时处理上述情况和我们需要分别处理结果的情况 ， 我们使用多重\
赋值 。 这为我们提供了两全其美的优势 ， 而无需在我们的目的不明确的情况下使用多条线路 \
。 ``return_value = exists, list_value = handle_get(key)`` 明确表示我们将至少以\
两种不同的方式引用 ``handle_get`` 返回的值 。 

这怎么会是一个数据库呢 ？
==============================================================================

上面的程序当然不是 RDBMS ， 但它绝对有资格作为 NoSQL 数据库 。 创建如此容易的原因是\
我们没有与数据进行任何真正的交互 。 我们做最少的类型检查 ， 否则只存储用户发送的任何\
内容 。 如果我们需要存储更多结构化数据 ， 我们可能需要为数据库创建一个模式并在存储和\
检索数据时引用它 。 

因此 ， 如果 NoSQL 数据库更易于编写 、 更易于维护和更易于推理 ， 为什么我们不都运行 \
MongoDB 实例并完成它呢 ？ 当然 ， NoSQL 数据库为我们提供的所有这些数据灵活性都需要\
权衡 ： 可搜索性 。 

Querying Data
------------------------------------------------------------------------------

想象一下 ， 我们使用上面的 NoSQL 数据库来存储之前的汽车数据 。 我们可以使用 VIN 作为\
键并使用值列表作为每个列值来存储它们 ， 即 \
``2134AFGER245267 = ['Lexus', 'RX350', 2013, Black]`` 。 当然 ， 我们已经失去了\
列表中每个索引的含义 。 我们只需要记住某个地方 ， 索引一存储汽车的品牌 ， 索引二存储\
年份 。

更糟糕的是 ， 当我们想要运行之前的一些查询时会发生什么 ？ 找到 1994 年所有汽车的颜色\
变成了一场噩梦 。 我们必须遍历 ``DATA`` 中的每个值 ， 以某种方式确定该值是存储汽车数\
据还是其他内容 ， 查看索引 2 ， 如果索引 2 等于 1994 ， 则取索引 3 的值 。 这比表扫\
描糟糕得多 ， 因为它不仅扫描数据存储中的每一行 ， 而且需要应用一组有点复杂的规则来回\
答查询 。 

NoSQL 数据库的作者当然知道这些问题 ， 并且 (因为查询通常是一个有用的功能) 已经提出\
了许多使查询成为可能的方法 。 一种方法是使用 JSON 等结构化数据 ， 并允许引用其他行来\
表示关系 。 此外 ， 大多数 NoSQL 数据库都有一些命名空间的概念 ， 其中单一类型的数据\
可以存储在它自己的数据库 "部分" 中 ， 允许查询引擎利用它知道数据 "形状" 的事实被查询 。

当然 ， 存在 (并已实现) 更复杂的方法来提高可查询性 ， 但在存储无模式数据和可查询性之\
间总是存在权衡 。 例如 ， 我们的数据库只支持按键查询 。 如果我们需要支持更丰富的查询\
集 ， 事情会变得更加复杂 。 

Summary
==============================================================================

希望现在已经清楚 "NoSQL" 的含义 。 我们学习了一些 SQL 以及 RDBMS 的工作原理 。 我们\
看到了如何从 RDBMS 中检索数据 (使用 SQL 查询) 。 我们构建了一个玩具 NoSQL 数据库来\
检查可查询性和简单性之间的权衡 。 我们还讨论了数据库作者处理这个问题的几种方法 。 

数据库的主题 ， 即使是简单的键值存储 ， 也是非常深入的 。 我们只是触及了表面 。 但是 \
， 希望您对 NoSQL 的含义 、 它的工作原理以及何时使用它有所了解 。 您可以在此站点的\
讨论区 `Chat For Smart People`_ 中继续对话 。 

.. _`Chat For Smart People`: https://web.archive.org/web/20200414132138/http://discourse.jeffknupp.com/

发表于 2014 年 9 月 1 日 ， 作者 ： Jeff Knupp
