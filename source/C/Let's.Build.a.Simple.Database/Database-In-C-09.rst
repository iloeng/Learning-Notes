##############################################################################
C 语言从头写一个 SQLite 程序
##############################################################################

.. contents::

******************************************************************************
第 13 部分  分割后更新父节点
******************************************************************************

我很满意 。 

我将添加另一个测试 ， 该测试将打印四节点树 。 只是为了让我们测试更多的案例而不是顺序\
的 ID ， 此测试将以伪随机顺序添加记录 。 

.. code-block:: ruby

  it 'allows printing out the structure of a 4-leaf-node btree' do
    script = [
      "insert 18 user18 person18@example.com",
      "insert 7 user7 person7@example.com",
      "insert 10 user10 person10@example.com",
      "insert 29 user29 person29@example.com",
      "insert 23 user23 person23@example.com",
      "insert 4 user4 person4@example.com",
      "insert 14 user14 person14@example.com",
      "insert 30 user30 person30@example.com",
      "insert 15 user15 person15@example.com",
      "insert 26 user26 person26@example.com",
      "insert 22 user22 person22@example.com",
      "insert 19 user19 person19@example.com",
      "insert 2 user2 person2@example.com",
      "insert 1 user1 person1@example.com",
      "insert 21 user21 person21@example.com",
      "insert 11 user11 person11@example.com",
      "insert 6 user6 person6@example.com",
      "insert 20 user20 person20@example.com",
      "insert 5 user5 person5@example.com",
      "insert 8 user8 person8@example.com",
      "insert 9 user9 person9@example.com",
      "insert 3 user3 person3@example.com",
      "insert 12 user12 person12@example.com",
      "insert 27 user27 person27@example.com",
      "insert 17 user17 person17@example.com",
      "insert 16 user16 person16@example.com",
      "insert 13 user13 person13@example.com",
      "insert 24 user24 person24@example.com",
      "insert 25 user25 person25@example.com",
      "insert 28 user28 person28@example.com",
      ".btree",
      ".exit",
    ]
    result = run_script(script)

照原样 ， 它将输出以下内容 ： 

.. code-block:: bash

    - internal (size 3)
    - leaf (size 7)
        - 1
        - 2
        - 3
        - 4
        - 5
        - 6
        - 7
    - key 1
    - leaf (size 8)
        - 8
        - 9
        - 10
        - 11
        - 12
        - 13
        - 14
        - 15
    - key 15
    - leaf (size 7)
        - 16
        - 17
        - 18
        - 19
        - 20
        - 21
        - 22
    - key 22
    - leaf (size 8)
        - 23
        - 24
        - 25
        - 26
        - 27
        - 28
        - 29
        - 30
    db >
 
仔细看 ， 您会发现一个错误 ： 

.. code-block:: bash 

    - 5
    - 6
    - 7
  - key 1

Key 应该是 7 ， 而不是 1 ！

经过一堆调试 ， 我发现这是由于一些错误的指针算法造成的 。 

INTERNAL_NODE_CHILD_SIZE 为 4 。 我的目的是在 ``internal_node_cell()`` 的结果中\
添加 4 个字节 ， 但是由于 ``internal_node_cell()`` 返回的是 ``uint32_t *`` ， 因\
此实际上是在添加 ``4 * sizeof(uint32_t)`` 个字节 。 在进行算术运算之前 ， 我通过将\
其强制转换为 ``void *`` 来对其进行了修复 。 

注意 ！ 空指针的指针算术不是 C 标准的一部分 ， 可能无法与您的编译器一起使用 。 以后\
我可能会写一篇关于可移植性的文章 ， 但现在暂时不做空指针算法 。 

好了 ， 向全面运行 btree 的实现又迈出了一步 。 下一步应该是分割内部节点 。 直到那时 ！

这里_ 是本节所有的代码改动 。 

.. _这里: https://github.com/Deteriorator/SimpleDB/commit/95b97742f6c1883eba0a09b3fdb0dbd2109b5f85
