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

从注释中可以清楚看到 ma_fill 域中维护着从 PyDictObject 对象创建开始直到现在 ， 曾\
经及正处于 Active 态的 entry 个数 ， 而 ma_used 则维护者当前正处于 Active 态的 \
entry 的数量 。 

在 PyDictObject 定义的最后 ， 有一个名为 ma_smalltable 的 PyDictEntry 数组 。 这\
个数组意味着当创建一个 PyDictObject 对象时 ， 至少有 PyDict_MINSIZE 个 entry 被同\
时创建 。 在 dictobject.h 中 ， 这个值被设定为 8 ， 这个值被认为是通过大量的实验得\
出的最佳值 。 既不会太浪费内存空间 ， 又能很好地满足 Python 内部大量使用 \
PyDictObject 的环境需求 ， 不需要在使用的过程中再次调用 malloc 申请内存空间 。

PyDictObject 中的 ma_table 域是关联对象的关键所在 ， 这个类型为 PyDictEntry* 的变\
量指向一片作为 PyDictEntry 集合的内存的开始位置 。 当一个 PyDictObject 对象是一个\
比较小的 dict 时 ， 即 entry 数量少于 8 个 ， ma_table 域将指向 ma_smalltable 这\
个与生俱来的 8 个 entry 的起始地址 。 当 PyDictObject 中 entry 数量大于 8 个时 \
， Python 认为是一个大 dict 将会申请额外的内存空间 ， 并将 ma_table 指向这块空间 \
。 无论何时 ， ma_table 域都不会为 NULL ， 总是有效的 。 

下图分别显示了 Python 中的 "大" ， "小" 两种 dict :

.. image:: img/5-3.png

最后 ， PyDictObject 中的 ma_mask 实际上记录了一个 PyDictObject 对象中所拥有的 \
entry 的数量 。 

5.3 PyDictObject 的创建和维护
==============================================================================

5.3.1 PyDictObject 对象创建
------------------------------------------------------------------------------

Python 内部通过 PyDict_New 来创建一个新的 dict 对象 。 

.. code-block:: c 

    typedef PyDictEntry dictentry;
    typedef PyDictObject dictobject;

    #define INIT_NONZERO_DICT_SLOTS(mp) do {				\
      (mp)->ma_table = (mp)->ma_smalltable;				\
      (mp)->ma_mask = PyDict_MINSIZE - 1;				\
        } while(0)

    #define EMPTY_TO_MINSIZE(mp) do {					\
      memset((mp)->ma_smalltable, 0, sizeof((mp)->ma_smalltable));	\
      (mp)->ma_used = (mp)->ma_fill = 0;				\
      INIT_NONZERO_DICT_SLOTS(mp);					\
        } while(0)

    PyObject *
    PyDict_New(void)
    {
      register dictobject *mp;
      //[1] : 自动创建 dummy 对象
      if (dummy == NULL) { /* Auto-initialize dummy */
        dummy = PyString_FromString("<dummy key>");
        if (dummy == NULL)
          return NULL;
    #ifdef SHOW_CONVERSION_COUNTS
        Py_AtExit(show_counts);
    #endif
      }
      if (num_free_dicts) {
        // [2]: 使用缓冲池
        mp = free_dicts[--num_free_dicts];
        assert (mp != NULL);
        assert (mp->ob_type == &PyDict_Type);
        _Py_NewReference((PyObject *)mp);
        if (mp->ma_fill) {
          EMPTY_TO_MINSIZE(mp);
        }
        assert (mp->ma_used == 0);
        assert (mp->ma_table == mp->ma_smalltable);
        assert (mp->ma_mask == PyDict_MINSIZE - 1);
      } else {
        // [3]: 创建 PyDictObject 对象
        mp = PyObject_GC_New(dictobject, &PyDict_Type);
        if (mp == NULL)
          return NULL;
        EMPTY_TO_MINSIZE(mp);
      }
      mp->ma_lookup = lookdict_string;
    #ifdef SHOW_CONVERSION_COUNTS
      ++created;
    #endif
      _PyObject_GC_TRACK(mp);
      return (PyObject *)mp;
    }

第一次调用 PyDict_New 时 ， 在 [1] 处会创建前文中的 dummy 对象 。 它是一个 \
PyStringObject 对象 ， 实际上用来作为一种指示标志 ， 表明该 entry 曾被使用过 ， 且\
探测序列下一个位置的 entry 有可能是有效的 ， 从而防止探测序列中断 。 

从 num_free_dicts 可以看出 Python 中 dict 的实现同样适用了缓冲池 。 

如果 PyDictObject 对象的缓冲池不可用 ， 那么 Python 将首先从系统堆中为新的 \
PyDictObject 对象申请合适的内存空间 ， 然后通过两个宏完成对新生的 PyDictObject 对\
象的初始化工作 ：

- EMPTY_TO_MINSIZE : 将 ma_smalltable 清零 ， 同时设置 ma_size 和 ma_fill ， 当\
  然在一个 PyDictObject 对象刚被创建的时候 ， 这两个变量都应该是 0 。

- INIT_NONZERO_DICT_SLOTS : 将 ma_table 指向 ma_smalltable ， 并设置 ma_mask \
  为 7 。

ma_mask 的初始化值为 PyDict_MINSIZE - 1 ， 确实与一个 PyDictObject 对象中的 \
entry 的数量有关 。 在创建过程的最后 ， 将 lookdict_string 赋给 ma_lookup 。 正\
是 ma_lookup 指向了 PyDictObject 在 entry 集合中搜索某一特定 entry 时需要进行的动\
作 ， 在 ma_lookup 中包含了散列函数和发生冲突时二次探测函数的具体实现 ， 它是 \
PyDictObject 的搜索策略 。 

5.3.2 PyDictObject 中的元素搜索
------------------------------------------------------------------------------


