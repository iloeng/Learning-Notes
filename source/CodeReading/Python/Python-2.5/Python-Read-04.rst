##############################################################################
Python 源码阅读系列 4
##############################################################################

.. contents::

******************************************************************************
第 3 章  Python 中的字符串对象
******************************************************************************

再对 PyIntObject 的分析中 ， Python 中具有不可变长度数据的对象 （定长对象） 。 在 \
Python 中 ， 还大量存在着另一种对象 ， 即具有可变长度数据的对象 （变长对象） 。 与定\
长对象不同 ， 变长对象维护的数据的长度在对象定义时是不知道的 。 

整数对象 PyIntObject 其维护的数据的长度在对象定义时就已经确定了 ， 是一个 C 中 long \
变量的长度 ； 而可变对象维护的数据的长度只能在对象创建时才能确定 ， 例如只能在创建一\
个字符串或一个列表时才知道它们所维护的数据的长度 ， 在此之前 ， 一无所知 。

在变长对象中 ， 实际上还可以分为可变对象和不可变对象 。 可变对象维护的数据在对象被创\
建后还能变化 ， 比如一个 list 被创建后 ， 可以向其中添加元素或删除对象 ， 这些操作都\
会改变其维护的数据 ； 而不可变对象所维护的数据在对象创建之后就不能在改变了 ， 比如 \
Python 中的 string 和 tuple ， 它们都不支持添加或删除操作 。 

3.1 PyStringObject 与 PyString_Type
==============================================================================

在 Python 中 ， PyStringObject 是对字符串对象的实现 。 PyStringObject 是一个拥有\
可变长度内存的对象 。 对于两个不同的 PyStringObject 对象 ， 其内部所需的保存字符串\
内容的内存空间显然是不同的 。 同时 ， PyStringObject 对象是一个不变对象 。 当创建了\
一个 PyStringObject 对象之后 ， 该对象内部维护的字符串就不能在被改变了 。 这一特点\
使得 PyStringObject 对象可作为 dict 的键值 ， 但也使得一些字符串操作的效率大大降低 \
， 比如多个字符串的连接 。 PyStringObject 对象定义 ： 

.. code-block:: c  

    [Include/stringonject.h]

    typedef struct {
        PyObject_VAR_HEAD
        long ob_shash;
        int ob_sstate;
        char ob_sval[1];

        /* Invariants:
        *     ob_sval contains space for 'ob_size+1' elements.
        *     ob_sval[ob_size] == 0.
        *     ob_shash is the hash of the string or -1 if not computed yet.
        *     ob_sstate != 0 iff the string object is in stringobject.c's
        *       'interned' dictionary; in this case the two references
        *       from 'interned' to this object are *not counted* in ob_refcnt.
        */
    } PyStringObject;

在 PyStringObject 的定义中可以看到 ， 在 PyStringObject 的头部实际上是一个 \
PyObject_VAR_HEAD ， 其中有一个 ob_size 变量保存着对象中维护的可变长度内存的大小 \
。 虽然在 PyStringObject 的定义中 ， ob_sval 实际上是作为一个字符指针指向一段内存\
的 ， 这段内存保存着这个字符串对象所维护的实际字符串 ， 显然这段内存不会只是一个字节 \
。 这段内存的实际长度（字节） ， 正是有 ob_size 维护的 ， 这个机制是 Python 中所有\
变长对象的实现机制 。 

同 C 中的字符串一样 ， PyStringObject 内部维护的字符串在末尾必须以 '\0' 结尾 ， 但\
是由于字符串的实际长度是由 ob_size 维护的 ， 所以 PyStringObject 表示的字符串对象\
中间是可能出现字符 '\0' 的 ， 这与 C 语言不同 ， 因为在 C 中 ， 只要遇到了字符 \
'\0' 就认为一个字符串结束了 ， 所以实际上 ， ob_sval 指向的是一段长度为 ob_size \
+ 1 个字节的内存 ， 而且必须满足 ob_sval[ob_size] == '\0' 。

PyStringObject 中的 ob_shash 变量的作用是缓存该对象的 hash 值 ， 这样避免每一次都\
重新计算该字符串对象的 hash 值 。 如果一个 PyStringObject 对象还没有别计算过 hash \
值 ， 那么 ob_shash 的初始值是 -1 。 在后面 dict 中 ， 这个 hash 将会发挥巨大的作\
用 。 计算一个字符串对象的 hash 值时 ， 采用如下算法 ： 

.. code-block:: c

    [Objects/stringobject.c]

    static long
    string_hash(PyStringObject *a)
    {
        register Py_ssize_t len;
        register unsigned char *p;
        register long x;

        if (a->ob_shash != -1)
            return a->ob_shash;
        len = a->ob_size;
        p = (unsigned char *) a->ob_sval;
        x = *p << 7;
        while (--len >= 0)
            x = (1000003*x) ^ *p++;
        x ^= a->ob_size;
        if (x == -1)
            x = -2;
        a->ob_shash = x;
        return x;
    }

PyStringObject 对象的 ob_sstate 变量标记了该对象是否已经过 intern 机制的处理 ， \
关于 PyStringObject 的 intern 机制 ， 在后面会详细介绍 ， 在 Python 源码中的注释\
显示 ， 预存字符串的 hash 值和这里的 intern 机制将 Python 虚拟机的执行效率提升了 \
20% 。

下面列出了 PyStringObject 对应的类型对象 - PyString_Type ：

.. code-block:: c

    [Objects/stringobject.c]

    PyTypeObject PyString_Type = {
        PyObject_HEAD_INIT(&PyType_Type)
        0,
        "str",
        sizeof(PyStringObject),
        sizeof(char),
        string_dealloc, 			/* tp_dealloc */
        (printfunc)string_print, 		/* tp_print */
        0,					/* tp_getattr */
        0,					/* tp_setattr */
        0,					/* tp_compare */
        string_repr, 				/* tp_repr */
        &string_as_number,			/* tp_as_number */
        &string_as_sequence,			/* tp_as_sequence */
        &string_as_mapping,			/* tp_as_mapping */
        (hashfunc)string_hash, 			/* tp_hash */
        0,					/* tp_call */
        string_str,				/* tp_str */
        PyObject_GenericGetAttr,		/* tp_getattro */
        0,					/* tp_setattro */
        &string_as_buffer,			/* tp_as_buffer */
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES |
            Py_TPFLAGS_BASETYPE,		/* tp_flags */
        string_doc,				/* tp_doc */
        0,					/* tp_traverse */
        0,					/* tp_clear */
        (richcmpfunc)string_richcompare,	/* tp_richcompare */
        0,					/* tp_weaklistoffset */
        0,					/* tp_iter */
        0,					/* tp_iternext */
        string_methods,				/* tp_methods */
        0,					/* tp_members */
        0,					/* tp_getset */
        &PyBaseString_Type,			/* tp_base */
        0,					/* tp_dict */
        0,					/* tp_descr_get */
        0,					/* tp_descr_set */
        0,					/* tp_dictoffset */
        0,					/* tp_init */
        0,					/* tp_alloc */
        string_new,				/* tp_new */
        PyObject_Del,	                	/* tp_free */
    };

在 PyStringObject 的类型对象中 ， tp_itemsize 被设置为 sizeof(char) ， 即一个字\
节 。 对于 Python 中的任何一种变长对象 ， tp_itemsize 这个域是必须设置的 ， \
tp_itemsize 指明了由变长对象保存的元素 (item) 的单位长度 ， 所谓单位长度即是指单一\
一个元素在内存中的长度 。 这个 tp_itemsize 和 ob_size 共同决定了应该额外申请的内存\
总大小是多少 。 tp_as_number 、 tp_as_sequence 、 tp_as_mapping 三个域都被设置了 \
， 表示 PyStringObject 对数值操作 ， 序列操作和映射操作都支持 。 

3.2 创建 PyStringObject 对象
==============================================================================

Python 提供了两条路径 ， 从 C 中原生的字符串创建 PyStringObject 对象 。 先看一下最\
一般的 PyString_FromString 。  

.. code-block:: C

    // [Objects/stringobject.c]

    PyObject *
    PyString_FromString(const char *str)
    {
        register size_t size;
        register PyStringObject *op;

        assert(str != NULL);
        size = strlen(str);
        if (size > PY_SSIZE_T_MAX) {
            PyErr_SetString(PyExc_OverflowError,
                "string is too long for a Python string");
            return NULL;
        }
        if (size == 0 && (op = nullstring) != NULL) {
    #ifdef COUNT_ALLOCS
            null_strings++;
    #endif
            Py_INCREF(op);
            return (PyObject *)op;
        }
        if (size == 1 && (op = characters[*str & UCHAR_MAX]) != NULL) {
    #ifdef COUNT_ALLOCS
            one_strings++;
    #endif
            Py_INCREF(op);
            return (PyObject *)op;
        }

        /* Inline PyObject_NewVar */
        op = (PyStringObject *)PyObject_MALLOC(sizeof(PyStringObject) + size);
        if (op == NULL)
            return PyErr_NoMemory();
        PyObject_INIT_VAR(op, &PyString_Type, size);
        op->ob_shash = -1;
        op->ob_sstate = SSTATE_NOT_INTERNED;
        Py_MEMCPY(op->ob_sval, str, size+1);
        /* share short strings */
        if (size == 0) {
            PyObject *t = (PyObject *)op;
            PyString_InternInPlace(&t);
            op = (PyStringObject *)t;
            nullstring = op;
            Py_INCREF(op);
        } else if (size == 1) {
            PyObject *t = (PyObject *)op;
            PyString_InternInPlace(&t);
            op = (PyStringObject *)t;
            characters[*str & UCHAR_MAX] = op;
            Py_INCREF(op);
        }
        return (PyObject *) op;
    }

    // 上述代码是 Python 2.5 源码，以下是书中的代码

    PyObject *
    PyString_FromString(const char *str)
    {
        register size_t size;
        register PyStringObject *op;

        // [1]: 判断字符串长度
        size = strlen(str);
        if (size > PY_SSIZE_T_MAX) {
            return NULL;
        }

        // [2]: 处理 NULL string
        if (size == 0 && (op = nullstring) != NULL) {
            return (PyObject *)op;
        }

        // [3]: 处理字符
        if (size == 1 && (op = characters[*str & UCHAR_MAX]) != NULL) {
            return (PyObject *)op;
        }

        /* Inline PyObject_NewVar */
        // [4]: 创建新的 PyStringObject 对象， 并初始化
        op = (PyStringObject *)PyObject_MALLOC(sizeof(PyStringObject) + size);
        PyObject_INIT_VAR(op, &PyString_Type, size);
        op->ob_shash = -1;
        op->ob_sstate = SSTATE_NOT_INTERNED;
        Py_MEMCPY(op->ob_sval, str, size+1);
        /* share short strings */
        if (size == 0) {
            PyObject *t = (PyObject *)op;
            PyString_InternInPlace(&t);
            op = (PyStringObject *)t;
            nullstring = op;
            Py_INCREF(op);
        } else if (size == 1) {
            PyObject *t = (PyObject *)op;
            PyString_InternInPlace(&t);
            op = (PyStringObject *)t;
            characters[*str & UCHAR_MAX] = op;
            Py_INCREF(op);
        }
        return (PyObject *) op;
    }

未完待续...