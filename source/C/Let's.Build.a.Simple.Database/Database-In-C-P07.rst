*******************************************************************************
Part 07 - B 型树简介
*******************************************************************************

.. contents:: 目录
    :depth: 3
    :backlinks: top

B 树是 SQLite 用来表示表和索引的数据结构， 所以它是一个相当核心的概念。 这篇文章将只\
是介绍这个数据结构， 所以不会有任何代码。 

为什么说树是数据库的一个好的数据结构? 

- 搜索一个特定的值是快速的 (对数时间)。
- 插入 / 删除一个你已经找到的值是快速的 (重新平衡的时间是恒定的)。
- 遍历一个值的范围是快速的 (不像哈希图)。 

B 树不同于二进制树 ("B"可能代表发明者的名字， 但也可能代表 "平衡")。 下面是一个 B 树\
的例子: 

.. figure:: img/B-tree.svg
    :align: center

    example B-Tree (https://en.wikipedia.org/wiki/File:B-tree.svg)

与二叉树不同， B 树中的每个节点可以有 2 个以上的子节点。 每个节点最多可以有 m 个子节\
点， 其中 m 被称为树的 "顺序"。 为了保持树的基本平衡， 我们还说节点必须至少有 m/2 个\
子节点 (四舍五入)。

异常情况： 

- 叶子结点有 0 个孩子
- 根节点可以有少于 m 个子节点， 但必须至少有 2 个子节点
- 如果根节点是一个叶子节点 (唯一的节点)， 它仍然有 0 个子节点

上面的图片是一个 B 树， SQLite 用它来存储索引。 为了存储表， SQLite 使用了一种叫做 \
B+ 树的变体。 

=============================  ================  ===================
Rows                           **B-tree**        **B+tree**  
=============================  ================  ===================
Pronounced                     "Bee Tree"        "Bee Plus Tree"
Used to store                  Indexes           Tables
Internal nodes store keys      Yes               Yes
Internal nodes store values    Yes               No
Number of children per node    Less              More
Internal nodes vs. leaf nodes  Same structure	 Different structure
=============================  ================  ===================

在我们实现索引之前， 我只谈 B+ 树， 但我只把它称为 B 树或 btree。 

有子节点的节点被称为 "内部" 节点。 内部节点和叶子结点的结构是不同的。 

======================  =============================  ===================
For an order-m tree...  Internal Node                  Leaf Node
======================  =============================  ===================
Stores                  keys and pointers to children  keys and values
Number of keys          up to m-1                      as many as will fit
Number of pointers      number of keys + 1             none
Number of values        none                           number of keys
Key purpose             used for routing               paired with value
Stores values?          No                             Yes
======================  =============================  ===================

让我们通过一个例子来看看当你插入元素时， B 树是如何增长的。 为了简单起见， 这棵树将\
是 3 阶的。 这意味着: 

- 每个内部节点最多有 3 个子节点
- 每个内部节点最多两个键
- 每个内部节点至少有 2 个子节点
- 每个内部节点至少有 1 个键

一个空的 B 树只有一个节点： 根节点。 根节点开始时是一个叶子节点， 有零个键 / 值对。 

.. figure:: img/btree1.png
    :align: center

    empty btree

如果我们插入几个键 / 值对， 它们会按排序顺序存储在叶子节点中。 

.. figure:: img/btree2.png
    :align: center

    one-node btree

比方说一个叶子节点的容量是两个键 / 值对。 当我们插入另一个节点时， 我们必须拆分叶子节\
点， 把一半的键值对放在每个节点中。 这两个节点都成为一个新的内部节点的子节点， 这个内\
部节点现在将是根节点。 

.. figure:: img/btree3.png
    :align: center

    two-level btree

内部节点有 1 个键和 2 个指向子节点的指针。 如果我们想查找一个小于或等于 5 的键， 我\
们在左边的子节点中查找。 如果我们想查找一个大于 5 的键， 我们就在右边的子节点中查找。 

现在让我们插入键 "2"。 首先， 我们查找它在哪个叶子节点中， 如果它是存在的， 我们到达\
左边的叶子节点。 这个节点已经满了， 所以我们把叶子节点拆开， 在父节点中创建一个新条目。 

.. figure:: img/btree4.png
    :align: center

    four-node btree

让我们继续添加 Key: 18 和 21。 我们到了必须再次分割的地步， 但在父节点中没有空间容纳\
另一个键 / 指针对。 

.. figure:: img/btree5.png 
    :align: center

    no room in internal node

解决办法是将根节点分成两个内部节点， 然后创建新的根节点作为它们的父节点。 

.. figure:: img/btree6.png 
    :align: center

    three-level btree

只有当我们分割根节点时， 树的深度才会增加。 每个叶子节点都有相同的深度和接近相同数量\
的键 / 值对， 所以树保持平衡和快速搜索。 

在我们实现插入之前， 我将暂不讨论从树上删除键的问题。 

当我们实现这个数据结构时， 每个节点将对应于一个页面。 根节点将存在于第 0 页。 子节点\
的指针将只是包含子节点的页号。 

下一节， 我们开始实现 btree! 
