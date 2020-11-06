Python 源码阅读系列 2
---------------------------------

前言
===================

承接上文。

在 Python 的实现中，对某些会频繁执行的代码，都会同时提供函数和宏两种版本，比如上午中的 \
PyInt_AS_LONG ， 与之对应的还有一个函数 PyInt_AsLong 。宏版本的 PyInt_AS_LONG 可以省\
去一次函数调用的开销，但是其牺牲了类被安全，因为其参数 op 完全可以不是一个 PyIntObject \
对象， intobject.c 中的函数版 PyInt_AsLong 则会多方检查类型安全性，但是牺牲了执行效率。

从 PyIntObject 对象的加法操作的实现可以清晰地看到 PyIntObject 是一个 immutable 的对\
象， 因为操作完成后，原来参与操作的任何一个对象都没有发生改变，取而代之的是一个全新的 \
PyIntObject 对象诞生。

如果加法结果溢出，其结果就不是一个 PyIntObject 对象，而是一个 PyLongObject 对象。例如：

.. image:: img/2-1.png 

图 2-1 加法溢出的例中

.. image:: img/2-1-0.png 

图 2-1-0 Python 3.7.7 版本实际结果

另一个有趣的元信息是 PyIntObject 对象的文档信息，其维护在 int_doc 域中。文档无缝地集成在语\
言中。可以在 Python 的交互环境下通过 PyIntObject 对象的 __doc__ 属性看到 int_doc 维护的\
文档：

.. image:: img/2-2.png

图 2-2 整数文档信息

.. code-block:: c 

    [Include/Python.h]

    /* Define macros for inline documentation. */
    #define PyDoc_VAR(name) static char name[]
    #define PyDoc_STRVAR(name,str) PyDoc_VAR(name) = PyDoc_STR(str)
    #ifdef WITH_DOC_STRINGS
    #define PyDoc_STR(str) str
    #else
    #define PyDoc_STR(str) ""
    #endif

    [Objects/intobject.c]

    PyDoc_STRVAR(int_doc,
    "int(x[, base]) -> integer\n\
    \n\
    Convert a string or number to an integer, if possible.  A floating point\n\
    argument will be truncated towards zero (this does not include a string\n\
    representation of a floating point number!)  When converting a string, use\n\
    the optional base.  It is an error to supply a base when converting a\n\
    non-string. If the argument is outside the integer range a long object\n\
    will be returned instead.");

2.2 PyIntObject 对象的创建和维护
==================================

2.2.1 对象创建的 3 种途径
++++++++++++++++++++++++++++++++++

在 Python 自身的实现中，几乎都是调用 C API 来创建内建实例对象。而内建对象即便是通过内建类\
型对象中的 tp_new , tp_init 操作创建实例对象，实际上最终还是会调用 Python 为特定对象准备\
的 C API 。

在 intobject.h 中可以看到，为了创建 PyIntObject 对象， Python 提供了 3 条途径，分别从 \
long 值， 从字符串以及 Py_UNICODE 对象生成 PyIntObject 对象。 

.. code-block:: c 

    PyAPI_FUNC(PyObject *) PyInt_FromString(char*, char**, int);
    #ifdef Py_USING_UNICODE
    PyAPI_FUNC(PyObject *) PyInt_FromUnicode(Py_UNICODE*, Py_ssize_t, int);
    #endif
    PyAPI_FUNC(PyObject *) PyInt_FromLong(long);

只考察从 long 值生成 PyIntObject 对象。因为 PyInt_FromString 和 PyInt_FromUnicode \
实际上都是先将字符串或 Py_UNICODE 对象转换成浮点数。然后再调用 PyInt_FromFloat。 它们\
不过利用了 Adaptor Pattern 的思想对整数对象的核心创建函数 PyInt_FromFloat 进行了接口\
转换罢了。

.. code-block:: c 

    PyObject *
    PyInt_FromString(char *s, char **pend, int base)
    {
        char *end;
        long x;
        Py_ssize_t slen;
        PyObject *sobj, *srepr;

        if ((base != 0 && base < 2) || base > 36) {
            PyErr_SetString(PyExc_ValueError,
                    "int() base must be >= 2 and <= 36");
            return NULL;
        }

        while (*s && isspace(Py_CHARMASK(*s)))
            s++;
        errno = 0;

        // 将字符串转换为 long 
        if (base == 0 && s[0] == '0') {
            x = (long) PyOS_strtoul(s, &end, base);
            if (x < 0)
                return PyLong_FromString(s, pend, base);
        }
        else
            x = PyOS_strtol(s, &end, base);
        if (end == s || !isalnum(Py_CHARMASK(end[-1])))
            goto bad;
        while (*end && isspace(Py_CHARMASK(*end)))
            end++;
        if (*end != '\0') {
    bad:
            slen = strlen(s) < 200 ? strlen(s) : 200;
            sobj = PyString_FromStringAndSize(s, slen);
            if (sobj == NULL)
                return NULL;
            srepr = PyObject_Repr(sobj);
            Py_DECREF(sobj);
            if (srepr == NULL)
                return NULL;
            PyErr_Format(PyExc_ValueError,
                    "invalid literal for int() with base %d: %s",
                    base, PyString_AS_STRING(srepr));
            Py_DECREF(srepr);
            return NULL;
        }
        else if (errno != 0)
            return PyLong_FromString(s, pend, base);
        if (pend)
            *pend = end;
        return PyInt_FromLong(x);
    }

2.2.2 小整数对象
++++++++++++++++++++++++

在 Python 中，对于小整数对象使用了对象池技术。

.. code-block:: c 

    [Objects/intobject.c]

    #ifndef NSMALLPOSINTS
        #define NSMALLPOSINTS		257
    #endif
    #ifndef NSMALLNEGINTS
        #define NSMALLNEGINTS		5
    #endif
    #if NSMALLNEGINTS + NSMALLPOSINTS > 0
        /* References to small integers are saved in this array so that they
        can be shared.
        The integers that are saved are those in the range
        -NSMALLNEGINTS (inclusive) to NSMALLPOSINTS (not inclusive).
        */
        static PyIntObject *small_ints[NSMALLNEGINTS + NSMALLPOSINTS];
    #endif

这个毫不起眼的 small_ints 就是举足轻重的小整数对象的对象池，准确地说，是 PyIntObject * \
池， 不过一般称其为小整数对象池。在 Python 2.5 中，将小整数集合的范围默认为 [-5, 257)。\
可以通过修改 NSMALLPOSINTS 和 NSMALLNEGINTS 重新编译 Python ，从而将这个范围向两端\
伸展或收缩。

2.2.3 大整数对象
+++++++++++++++++++++++++++

对于小整数，在小整数对象池中完全缓存了 PyIntObject 对象。而对于其他整数， Python 运行\
环境提供了一块内存空间，由大整数轮流使用。在 Python 中， 有一个 PyIntBlock 结构，在这\
基础上，实现了一个单向列表。

.. code-block:: c

    [Objects/intobject.c]

    #define BLOCK_SIZE	1000	/* 1K less typical malloc overhead */
    #define BHEAD_SIZE	8	/* Enough for a 64-bit pointer */
    #define N_INTOBJECTS	((BLOCK_SIZE - BHEAD_SIZE) / sizeof(PyIntObject))

    struct _intblock {
        struct _intblock *next;
        PyIntObject objects[N_INTOBJECTS];
    };

    typedef struct _intblock PyIntBlock;

    static PyIntBlock *block_list = NULL;
    static PyIntObject *free_list = NULL;

PyIntBlock 这个结构里维护了一块内存 (block)，其中保存了一些 PyIntObject 对象。从定义中\
可以看出一个 PyIntBlock 中维护着 N_INTOBJECTS 个对象，计算后是 82 个。这里也可以动态调整。

PyIntBlock 的单向列表通过 block_list 维护，每个 block 中都维护了一个 PyIntObject 数组\
-- objects ， 这就是真正用于存储被缓存的 PyIntObject 对象的内存。 Python 使用一个单向\
链表来管理全部 block 的 objects 中所有的空闲内存，这个自由内存链表的表头就是 free_list 。 
最开始时，两个指针都被设置为空指针。

.. image:: img/2-3.png

2.2.4 添加和删除
++++++++++++++++++++++++

下面通过 PyInt_FromLong 进行细致入微的考察，真实展现一个个 PyIntObject 对象的产生。

.. code-block:: c

    [Objects/intobject.c]

    PyObject *
    PyInt_FromLong(long ival)
    {
        register PyIntObject *v;
    #if NSMALLNEGINTS + NSMALLPOSINTS > 0
    // [1] ：尝试使用小整数对象池
        if (-NSMALLNEGINTS <= ival && ival < NSMALLPOSINTS) {
            v = small_ints[ival + NSMALLNEGINTS];
            Py_INCREF(v);
    #ifdef COUNT_ALLOCS
            if (ival >= 0)
                quick_int_allocs++;
            else
                quick_neg_int_allocs++;
    #endif
            return (PyObject *) v;
        }
    #endif
    // [2]： 为通用整数对象池申请新的内存空间
        if (free_list == NULL) {
            if ((free_list = fill_free_list()) == NULL)
                return NULL;
        }
        /* Inline PyObject_New */
        // [3] ： (inline) 内联 PyObject_New 的行为
        v = free_list;
        free_list = (PyIntObject *)v->ob_type;
        PyObject_INIT(v, &PyInt_Type);
        v->ob_ival = ival;
        return (PyObject *) v;
    }

PyIntObject 对象的创建通过两步完成(上述代码是 Python 2.5 代码，与书中有出入)：

.. code-block::

    PyObject *
    PyInt_FromLong(long ival)
    {
        register PyIntObject *v;
    #if NSMALLNEGINTS + NSMALLPOSINTS > 0
    // [1] ：尝试使用小整数对象池
        if (-NSMALLNEGINTS <= ival && ival < NSMALLPOSINTS) {
            v = small_ints[ival + NSMALLNEGINTS];
            Py_INCREF(v);
            return (PyObject *) v;
        }
    #endif
    // [2]： 为通用整数对象池申请新的内存空间
        if (free_list == NULL) {
            if ((free_list = fill_free_list()) == NULL)
                return NULL;
        }
        /* Inline PyObject_New */
        // [3] ： (inline) 内联 PyObject_New 的行为
        v = free_list;
        free_list = (PyIntObject *)v->ob_type;
        PyObject_INIT(v, &PyInt_Type);
        v->ob_ival = ival;
        return (PyObject *) v;
    }

- 如果小整数对象池机制被激活，则尝试使用小整数对象池；
- 如果不能使用小整数对象池，则使用通用的整数对象池。

2.2.4.1 使用小整数对象池
**********************************

