##############################################################################
Python 源码阅读系列 8
##############################################################################

.. contents::

******************************************************************************
第 5 章  Python 中的 Dict 对象
******************************************************************************

5.2 PyDictObject
==============================================================================

5.2.1 关联容器的 entry
------------------------------------------------------------------------------

在 PyDictEntry 中 ， me_hash 域存储的是 me_key 的散列值 ， 利用一个域来记录这个散\
列值可以避免每次查询的时候都要重新计算一遍散列值 。

在 Python 中一个 PyDictObject 对象生存变化的过程中 ， 其中的 entry 会在不同的状态\
间转换 。 PyDictObject 中 entry 可以在 3 种状态间转换 ： Unused 态 、 Active 态\
和 Dummy 态 。

- 当一个 entry 的 me_key 和 me_value 都是 NULL 时 ， entry 处于 Unused 态 。 \
  Unused 态表明目前该 entry 中并没有存储 (key ， value) 对 ， 而且在此之前 ， 也\
  没有存储过它们 。 每个 entry 在初始化的时候都会处于这种状态 ， 而且只有在 Unused \
  态下 ， entry 的 me_key 域才会为 NULL 。

- 当 entry 中存储一个 (key ， value) 对时， entry 便转换到了 Active 态 。 在 \
  Active 态 。 在 Active 态下 ， me_key 和 me_value 都不能为 NULL 。 更进一步地\
  说 ， me_key 不能是 dummy 对象 。 

- 当 entry 中存储的 (key ， value) 对被删除后 ， entry 的状态不能直接从 Active 态\
  转为 Unused 态 ， 否则会导致冲突探测链的中断 。 相反 ， entry 中的 me_key 将指\
  向 dummy 对象 ， entry 进入 Dummy 态 ， 这就是 "伪删除" 技术 。 当 Python 沿着\
  某条冲突链搜索时 ， 如果发现一个 entry 处于 Dummy 态 ， 说明目前该 entry 虽然是\
  无效的 ， 但是其后的 entry 可能是有效的 ， 是应该搜索的 。 这样就保证了冲突探测链\
  的连续性 。

.. image:: img/5-2.png

5.2.2 关联容器的实现
------------------------------------------------------------------------------

在 Python 中 ， 关联容器是通过 PyDictObject 对象来实现的 。 而一个 PyDictObject \
对象实际上是一大堆 entry 的集合 ， 总控这些集合的结构如下 ： 

.. code-block:: c 

    [Include/dictobject.h]

    #define PyDict_MINSIZE 8
    typedef struct _dictobject PyDictObject;
    struct _dictobject {
        PyObject_HEAD
        Py_ssize_t ma_fill;  /* # Active + # Dummy */
        Py_ssize_t ma_used;  /* # Active */

        /* The table contains ma_mask + 1 slots, and that's a power of 2.
        * We store the mask instead of the size because the mask is more
        * frequently needed.
        */
        Py_ssize_t ma_mask;

        /* ma_table points to ma_smalltable for small tables, else to
        * additional malloc'ed memory.  ma_table is never NULL!  This rule
        * saves repeated runtime null-tests in the workhorse getitem and
        * setitem calls.
        */
        PyDictEntry *ma_table;
        PyDictEntry *(*ma_lookup)(PyDictObject *mp, PyObject *key, long hash);
        PyDictEntry ma_smalltable[PyDict_MINSIZE];
    };

