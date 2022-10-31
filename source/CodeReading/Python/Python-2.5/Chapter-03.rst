###############################################################################
Chapter 03 - Python 中的字符串对象
###############################################################################

.. contents::

再对 ``PyIntObject`` 的分析中， Python 中具有不可变长度数据的对象 （定长对象）。 \
在 Python 中， 还大量存在着另一种对象， 即具有可变长度数据的对象 （变长对象）。 与定\
长对象不同， 变长对象维护的数据的长度在对象定义时是不知道的。 

整数对象 ``PyIntObject`` 其维护的数据的长度在对象定义时就已经确定了， 是一个 C 中 \
``long`` 变量的长度； 而可变对象维护的数据的长度只能在对象创建时才能确定， 例如只能\
在创建一个字符串或一个列表时才知道它们所维护的数据的长度， 在此之前， 一无所知。

在变长对象中， 实际上还可以分为可变对象和不可变对象。 可变对象维护的数据在对象被创建\
后还能变化， 比如一个 ``list`` 被创建后， 可以向其中添加元素或删除对象， 这些操作都\
会改变其维护的数据； 而不可变对象所维护的数据在对象创建之后就不能在改变了， 比如 \
Python 中的 ``string`` 和 ``tuple``， 它们都不支持添加或删除操作。 

*******************************************************************************
3.1 ``PyStringObject`` 与 ``PyString_Type``
*******************************************************************************

在 Python 中， ``PyStringObject`` 是对字符串对象的实现。 ``PyStringObject`` 是一\
个拥有可变长度内存的对象。 对于两个不同的 ``PyStringObject`` 对象， 其内部所需的保\
存字符串内容的内存空间显然是不同的。 同时 ``PyStringObject`` 对象是一个不变对象。 \
当创建了一个 ``PyStringObject`` 对象之后， 该对象内部维护的字符串就不能在被改变了\
。 这一特点使得 ``PyStringObject`` 对象可作为 ``dict`` 的键值， 但也使得一些字符串\
操作的效率大大降低， 比如多个字符串的连接。 ``PyStringObject`` 对象定义： 

.. topic:: [Include/stringonject.h]

    .. code-block:: c  

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

在 ``PyStringObject`` 的定义中可以看到， 在 ``PyStringObject`` 的头部实际上是一\
个 ``PyObject_VAR_HEAD``， 其中有一个 ``ob_size`` 变量保存着对象中维护的可变长度内\
存的大小。 虽然在 ``PyStringObject`` 的定义中， ``ob_sval`` 实际上是作为一个字符指\
针指向一段内存的， 这段内存保存着这个字符串对象所维护的实际字符串， 显然这段内存不会\
只是一个字节。 这段内存的实际长度（字节）， 正是有 ``ob_size`` 维护的， 这个机制是 \
Python 中所有变长对象的实现机制。 

同 C 中的字符串一样， ``PyStringObject`` 内部维护的字符串在末尾必须以 '\0' 结尾， \
但是由于字符串的实际长度是由 ``ob_size`` 维护的， 所以 ``PyStringObject`` 表示的字\
符串对象中间是可能出现字符 '\0' 的， 这与 C 语言不同， 因为在 C 中， 只要遇到了字符 \
'\0' 就认为一个字符串结束了， 所以实际上， ``ob_sval`` 指向的是一段长度为 \
``ob_size + 1`` 个字节的内存， 而且必须满足 ``ob_sval[ob_size] == '\0'``。

``PyStringObject`` 中的 ``ob_shash`` 变量的作用是缓存该对象的 hash 值， 这样避免每\
一次都重新计算该字符串对象的 hash 值。 如果一个 ``PyStringObject`` 对象还没有别计算\
过 hash 值， 那么 ``ob_shash`` 的初始值是 ``-1``。 在后面 ``dict`` 中， 这个 \
hash 将会发挥巨大的作用。 计算一个字符串对象的 hash 值时， 采用如下算法： 

.. topic:: [Objects/stringobject.c]

    .. code-block:: c

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

``PyStringObject`` 对象的 ``ob_sstate`` 变量标记了该对象是否已经过 intern 机制的\
处理， 关于 ``PyStringObject`` 的 intern 机制， 在后面会详细介绍， 在 Python 源码\
中的注释显示， 预存字符串的 hash 值和这里的 intern 机制将 Python 虚拟机的执行效率提\
升了 20%。

下面列出了 ``PyStringObject`` 对应的类型对象 - ``PyString_Type``：

.. topic:: [Objects/stringobject.c]

    .. code-block:: c    

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

在 ``PyStringObject`` 的类型对象中， ``tp_itemsize`` 被设置为 ``sizeof(char)``\
， 即一个字节。 对于 Python 中的任何一种变长对象， ``tp_itemsize`` 这个域是必须设置\
的， ``tp_itemsize`` 指明了由变长对象保存的元素 (item) 的单位长度， 所谓单位长度即\
是指单一一个元素在内存中的长度。 这个 ``tp_itemsize`` 和 ``ob_size`` 共同决定了应\
该额外申请的内存总大小是多少。 ``tp_as_number``、 ``tp_as_sequence``、 \
``tp_as_mapping`` 三个域都被设置了， 表示 ``PyStringObject`` 对数值操作， 序列操作\
和映射操作都支持。 

*******************************************************************************
3.2 创建 ``PyStringObject`` 对象
*******************************************************************************

Python 提供了两条路径， 从 C 中原生的字符串创建 ``PyStringObject`` 对象。 先看一下\
最一般的 ``PyString_FromString``。  

.. topic:: [Objects/stringobject.c]
    
    .. code-block:: C

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

显然传给 ``PyString_FromString`` 的参数必须是一个指向 NUL ('\0') 结尾的字符串指针\
。 在从一个原生字符串创建 ``PyStringObject`` 时， 首先 [1] 处检查该字符数组的长度\
， 如果长度大于了 ``PY_SSIZE_T_MAX``， Python 将不会创建对应的 \
``PyStringObject`` 对象。 ``PY_SSIZE_T_MAX`` 是一个与平台相关的值， 在 Win32 系统\
下， 该值为 ``2 147 483 647``， 即 2GB。 

在 [2] 处， 检查传入的字符串是否是一个空串， 对于空串， Python 并不是每次都会创建相\
应的 ``PyStringObject``。 Python 运行时有一个 ``PyStringObject`` 对象指针 \
``nullstring`` 专门负责处理空的字符数组。 如果第一次在一个空字符串基础上创建 \
``PyStringObject``， 由于 ``nullstring`` 指针被初始化为 ``NULL``， 所以 Python \
会为这个空字符建立一个 ``PyStringObject`` 对象， 将这个 ``PyStringObject`` 对象通\
过 intern 机制进行共享， 然后将 ``nullstring`` 指向这个被共享的对象。 如果在以后 \
Python 检查到需要为一个空字符串创建 ``PyStringObject`` 对象， 这时 \
``nullstring`` 已经存在了， 就直接返回 ``nullstring`` 的引用。

如果不是创建空字符串对象， 接下来的进行的动作就是申请内存， 创建 \
``PyStringObject`` 对象。 [4] 处申请的内存除了 ``PyStringObject`` 的内存， 还有为\
字符数组内的元素申请的额外内存。 然后将 hash 缓存值设为 ``-1``， 将 intern 标志设\
为 ``SSTATE_NOT_INTERNED``。 最后将参数 ``str`` 指向字符数组内的字符拷贝到 \
``PyStringObject`` 所维护的空间中， 在拷贝的过程中， 将字符数组最后的 '\0' 字符也拷\
贝了。 假如对字符数组 "Python" 建立 ``PyStringObject`` 对象， 那么对象建立完成后在\
内存中的状态如图： 

.. figure:: img/3-1.png
    :align: center

在 ``PyString_FromString`` 之外， 还有一条创建 ``PyStringObject`` 对象的途径 - \
``PyString_FromStringAndSize``:

.. topic:: [Objects/stringobject.c]

    .. code-block:: c 

        //[书中的代码]

        PyObject* PyString_FromStringAndSize(const char *str, Py_ssize_t size)
        {
            register PyStringObject *op;
            // 处理 null string
            if (size == 0 && (op = nullstring) != NULL) {
                return (PyObject *)op;
            }
            // 处理字符
            if (size == 1 && str != NULL &&
                (op = characters[*str & UCHAR_MAX]) != NULL)
            {
                return (PyObject *)op;
            }
            // 创建新的 PyStringObject 对象， 并初始化
            /* Inline PyObject_NewVar */
            op = (PyStringObject *)PyObject_MALLOC(sizeof(PyStringObject) + size);
            if (op == NULL)
                return PyErr_NoMemory();
            PyObject_INIT_VAR(op, &PyString_Type, size);
            op->ob_shash = -1;
            op->ob_sstate = SSTATE_NOT_INTERNED;
            if (str != NULL)
                Py_MEMCPY(op->ob_sval, str, size);
            op->ob_sval[size] = '\0';
            /* share short strings */
            if (size == 0) {
                PyObject *t = (PyObject *)op;
                PyString_InternInPlace(&t);
                op = (PyStringObject *)t;
                nullstring = op;
                Py_INCREF(op);
            } else if (size == 1 && str != NULL) {
                PyObject *t = (PyObject *)op;
                PyString_InternInPlace(&t);
                op = (PyStringObject *)t;
                characters[*str & UCHAR_MAX] = op;
                Py_INCREF(op);
            }
            return (PyObject *) op;
        }

        //[代码包中的代码]    

        PyObject *
        PyString_FromStringAndSize(const char *str, Py_ssize_t size)
        {
            register PyStringObject *op;
            assert(size >= 0);
            if (size == 0 && (op = nullstring) != NULL) {
        #ifdef COUNT_ALLOCS
                null_strings++;
        #endif
                Py_INCREF(op);
                return (PyObject *)op;
            }
            if (size == 1 && str != NULL &&
                (op = characters[*str & UCHAR_MAX]) != NULL)
            {
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
            if (str != NULL)
                Py_MEMCPY(op->ob_sval, str, size);
            op->ob_sval[size] = '\0';
            /* share short strings */
            if (size == 0) {
                PyObject *t = (PyObject *)op;
                PyString_InternInPlace(&t);
                op = (PyStringObject *)t;
                nullstring = op;
                Py_INCREF(op);
            } else if (size == 1 && str != NULL) {
                PyObject *t = (PyObject *)op;
                PyString_InternInPlace(&t);
                op = (PyStringObject *)t;
                characters[*str & UCHAR_MAX] = op;
                Py_INCREF(op);
            }
            return (PyObject *) op;
        }

``PyString_FromStringAndSize`` 的操作过程和 ``PyString_FromString`` 一般无二， \
只是有一点， ``PyString_FromString`` 传入的参数必须是以 NUL ('\0') 结尾的字符数组\
的指针， 而 ``PyString_FromStringAndSize`` 没有这样的要求， 因为通过传入的 \
``size`` 参数就可以确定需要拷贝的字符的个数。

*******************************************************************************
3.3 字符串对象的 intern 机制
*******************************************************************************

无论是 ``PyString_FromString`` 还是 ``PyString_FromStringAndSize``， 当字符数组\
的长度为 0 或 1 时， 需要进行一个特别的动作： ``PyString_InternInPlace``。 就是前\
文中提到的 intern 机制。

.. topic:: [Objects/stringobject.c]

    .. code-block:: c 

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
            // intern (共享) 长度较短的 PyStringObject 对象
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

``PyStringObject`` 对象的 intern 机制的目的是： 对于被 intern 之后的字符串， 比如 \
"Ruby"， 在整个 Python 的运行期间， 系统中都只有唯一的一个与字符串 "Ruby" 对应的 \
``PyStringObject`` 对象。 这样当判断两个 ``PyStringObject`` 对象是否相同时， 如果\
他们都被 intern 了， 那么只需要简单地检查它们对应的 ``PyObject*`` 是否相同即可。 这\
个机制既节省了空间， 又简化了对 ``PyStringObject`` 对象的比较。 \
``PyString_InternInPlace`` 负责完成对一个对象进行 intern 操作的函数。

.. topic:: [Objects/stringobject.c]

    .. code-block:: c

        void
        PyString_InternInPlace(PyObject **p)
        {
            register PyStringObject *s = (PyStringObject *)(*p);
            PyObject *t;
            if (s == NULL || !PyString_Check(s))
                Py_FatalError("PyString_InternInPlace: strings only please!");
            /* If it's a string subclass, we don't really know what putting
            it in the interned dict might do. */
            if (!PyString_CheckExact(s))
                return;
            if (PyString_CHECK_INTERNED(s))
                return;
            if (interned == NULL) {
                interned = PyDict_New();
                if (interned == NULL) {
                    PyErr_Clear(); /* Don't leave an exception */
                    return;
                }
            }
            t = PyDict_GetItem(interned, (PyObject *)s);
            if (t) {
                Py_INCREF(t);
                Py_DECREF(*p);
                *p = t;
                return;
            }

            if (PyDict_SetItem(interned, (PyObject *)s, (PyObject *)s) < 0) {
                PyErr_Clear();
                return;
            }
            /* The two references in interned are not counted by refcnt.
            The string deallocator will take care of this */
            s->ob_refcnt -= 2;
            PyString_CHECK_INTERNED(s) = SSTATE_INTERNED_MORTAL;
        }

        //[上述代码是代码包中的代码，下面的是书中的代码]

        void
        PyString_InternInPlace(PyObject **p)
        {
            register PyStringObject *s = (PyStringObject *)(*p);
            PyObject *t;
            // 对 PyStringObject 进行类型和状态检查
            if (!PyString_CheckExact(s))
                return;
            if (PyString_CHECK_INTERNED(s))
                return;
            // 创建记录经 intern 机制处理后的 PyStringObject 的 dict
            if (interned == NULL) {
                interned = PyDict_New();
            }
            // [1] : 检查 PyStringObject 对象 S 是否存在对应的 intern 后的 PyStringObject 对象
            t = PyDict_GetItem(interned, (PyObject *)s);
            if (t) {
                // 注意这里对引用计数的调整
                Py_INCREF(t);
                Py_DECREF(*p);
                *p = t;
                return;
            }

            // [2] : 在 interned 中记录检查 PyStringObject 对象 S 
            PyDict_SetItem(interned, (PyObject *)s, (PyObject *)s);

            /* The two references in interned are not counted by refcnt.
            The string deallocator will take care of this */
            // [3] : 注意这里对引用计数的调整
            s->ob_refcnt -= 2;
            // [4] : 调整 S 中的 intern 状态标志
            PyString_CHECK_INTERNED(s) = SSTATE_INTERNED_MORTAL;
        }

``PyString_InternInPlace`` 首先会进行一系列的检查， 其中包括：

- 检查传入的对象是否是一个 ``PyStringObject`` 对象， intern 机制只能应用在 \
  ``PyStringObject`` 对象上， 甚至对于他的派生类对象系统都不会应用 intern 机制。 

- 检查传入的 ``PyStringObject`` 对象是否已经被 intern 机制处理过了， Python 不会对\
  同一个 ``PyStringObject`` 对象进行一次以上的 intern 操作。 

intern 机制的核心在于 interned， interned 在 *stringobject.c* 中被定义为： \
``static PyObject *interned``。

在代码中 interned 实际指向的是 ``PyDict_New`` 创建的一个对象。 ``PyDict_New`` 实\
际上创建了一个 ``PyDictObject`` 对象， 即 Python 中常用的 ``dict``。 可以看作是 \
C++ 中的 map， 即 ``map<PyObject*, PyObject*>``。 C++ 我不懂， 先记下笔记。 

interned 机制的关键就是在系统中有一个 key value 映射关系的集合， 集合的名称叫做 \
interned。 其中记录着被 intern 机制处理过的 ``PyStringObject`` 对象。 当对一个 \
``PyStringObject`` 对象 a 应用 intern 机制时， 首先会在 interned 这个 dict 中检查\
是否有满足以下条件的对象 b： b 中维护的原生字符串与 a 相同。 如果确实存在对象 b， 那\
么指向 a 的 ``PyObject`` 指针会指向 b， 而 a 的引用计数减 1， 而 a 只是一个被临时创\
建的对象。 如果 interned 中不存在这样的 b， 那么就在 [2] 处将 a 记录到 interned 中。 

下图展示了如果 interned 中存在这样的对象 b， 再对 a 进行 intern 操作时， 原本指向 \
a 的 ``PyObject*`` 指针的变化： 

.. figure:: img/3-2.png
    :align: center

对于被 intern 机制处理的 ``PyStringObject`` 对象， Python 采用了特殊的引用计数机制\
。 在将一个 ``PyStringObject`` 对象 a 的 ``PyObject`` 指针作为 key 和 value 添加\
到 interned 中时 ``PyDictObject`` 对象会通过这两个指针对 a 的引用计数进行两次加 1 \
的操作。 但是 Python 的设计者规定在 interned 中 a 的指针不能被视为对象 a 的有效引用\
， 因为如果是有效引用的话， 那么 a 的引用计数在 Python 结束之前永远不能为 0， 因为 \
interned 中至少有两个指针引用了 a， 那么删除 a 就永远不可能了。

因此 interned 中的指针不能作为 a 的有效引用。 这就是代码中 [3] 处会将引用计数减 2 \
的原因。 在 A 的引用计数在某个时刻减为 0 之后， 系统将会销毁对象 a， 同时会在 \
interned 中删除指向 a 的指针， 在 ``string_dealloc`` 代码中得到验证： 

.. topic:: [Objects/stringobject.c]

    .. code-block:: c 

        static void
        string_dealloc(PyObject *op)
        {
            switch (PyString_CHECK_INTERNED(op)) {
                case SSTATE_NOT_INTERNED:
                    break;

                case SSTATE_INTERNED_MORTAL:
                    /* revive dead object temporarily for DelItem */
                    op->ob_refcnt = 3;
                    if (PyDict_DelItem(interned, op) != 0)
                        Py_FatalError(
                            "deletion of interned string failed");
                    break;

                case SSTATE_INTERNED_IMMORTAL:
                    Py_FatalError("Immortal interned string died.");

                default:
                    Py_FatalError("Inconsistent interned string state.");
            }
            op->ob_type->tp_free(op);
        }

Python 在创建一个字符串的时候， 会首先在 interned 中检查是否已经有改字符串对应的 \
``PyStringObject`` 对象了， 如有则不用创建新的。 这样会节省内存空间， 但是 Python \
并不是在创建 ``PyStringObject`` 时就通过 interned 实现了节省空间的目的。 事实上从 \
``PyString_FromString`` 中可以看到， 无论如何， 一个合法的 ``PyStringObject`` 对\
象是会被创建的， 同样 ``PyString_InternInPlace`` 也只对 ``PyStringObject`` 起作用\
。 Python 始终会为字符串 s 创建 ``PyStringObject`` 对象， 尽管 s 中维护的原生字符\
数组在 interned 中已经有一个与之对应的 ``PyStringObject`` 对象了。 而 intern 机制\
是在 s 被创建后才起作用的， 通常 Python 在运行时创建了一个 ``PyStringObject`` 对\
象 temp 后， 基本上都会调用 ``PyString_InternInPlace`` 对 temp 进行处理， intern \
机制会减少 temp 的引用计数， temp 对象会由于引用计数减为 0 而被销毁。 

Python 提供了一个以 ``char*`` 为参数的 intern 机制相关的函数用来直接对 C 原生字符串\
上做 intern 操作： 

.. code-block:: c 

    PyObject *
    PyString_InternFromString(const char *cp)
    {
        PyObject *s = PyString_FromString(cp);
        if (s == NULL)
            return NULL;
        PyString_InternInPlace(&s);
        return s;
    }

临时对象仍然被创建出来， 实际上在 Python 中， 必须创建一个临时的 \
``PyStringObject`` 对象来完成 interne 操作。 因为 ``PyDictObject`` 必须以 \
``PyObject *`` 指针作为键。 

实际上被 intern 机制处理后的 ``PyStringObject`` 对象分为两类， 一类处于 \
``SSTATE_INTERNED_IMMORTAL`` 状态， 而另一类则处于 ``SSTATE_INTERNED_MORTAL`` 状\
态， 这两种状态的区别在 ``string_dealloc`` 中可以清晰地看到， 显然 \
``SSTATE_INTERNED_IMMORTAL`` 状态的 ``PyStringObject`` 对象是永远不会被销毁的， \
它将与 Python 虚拟机共存， 即同年同月同日死。 

``PyString_InternInPlace`` 只能创建 ``SSTATE_INTERNED_MORTAL`` 状态的 \
``PyStringObject`` 对象， 如果想创建 ``SSTATE_INTERNED_IMMORTAL`` 状态的对象， \
必须通过另一个接口， 在调用 ``PyString_InternInPlace`` 后， 强制改变 \
``PyStringObject`` 的 intern 状态。 

.. code-block:: c 

    void
    PyString_InternImmortal(PyObject **p)
    {
        PyString_InternInPlace(p);
        if (PyString_CHECK_INTERNED(*p) != SSTATE_INTERNED_IMMORTAL) {
            PyString_CHECK_INTERNED(*p) = SSTATE_INTERNED_IMMORTAL;
            Py_INCREF(*p);
        }
    }

*******************************************************************************
3.4 字符缓冲池
*******************************************************************************

Python 为 ``PyStringObject`` 中的一个字节的字符对应的 ``PyStringObject`` 对象也设\
计了一个对象池 ``characters``:

.. topic:: [Objects/stringobject.c]

    .. code-block:: c 

        static PyStringObject *characters[UCHAR_MAX + 1];

``UCHAR_MAX`` 是在系统头文件中定义的常量， 这是一个平台相关的常量， 在 Win32 平台下： 

.. code-block:: c 

    #define UCHAR_MAX    0xff   

这个被定义在 C 语言的 *limits.h* 头文件中。 

在 Python 的整数对象体系中， 小整数的缓冲池是在 Python 初始化的时候被创建的， 而字符\
串对象体系中的字符缓冲池则是以静态变量的形式存在。 在 Python 初始化完成之后， 缓冲池\
中的所有 ``PyStringObject`` 指针都为空。 

创建一个 ``PyStringObject`` 对象时， 无论是通过调用 ``PyString_FromString`` 还是\
通过调用 ``PyString_FromStringAndSize``， 若字符串实际就一个字符， 则会进行如下操作： 

.. code-block:: c 

    PyObject *
    PyString_FromStringAndSize(const char *str, Py_ssize_t size)
    {
        ...
        else if (size == 1 && str != NULL) {
            PyObject *t = (PyObject *)op;
            PyString_InternInPlace(&t);
            op = (PyStringObject *)t;
            characters[*str & UCHAR_MAX] = op;
            Py_INCREF(op);
        }
        return (PyObject *) op;
    }

先对所创建的字符串 (字符) 对象进行 intern 操作， 在将 intern 的结果缓存到字符缓冲\
池 ``characters`` 中。 图 3-3 演示了缓存一个字符到对应的 ``PyStringObject`` 对象\
的过程。

.. figure:: img/3-3.png
    :align: center

3 条带有标号的曲线既代表指针， 有代表进行操作的顺序： 

1. 创建 ``PyStringObject`` 对象 <string p>；

2. 对对象 <string p> 进行 intern 操作；

3. 将对象 <string p> 缓存至字符串缓冲池中。 

在创建 ``PyStringObject`` 时， 会首先检查所要创建的是否是一个字符对象， 然后检查字\
符缓冲池中是否包含这个字符的字符对象的缓冲， 若有直接返回这个缓冲对象即可：

.. topic:: [Objects/stringobject.c]

    .. code-block:: c 

        PyObject *
        PyString_FromStringAndSize(const char *str, Py_ssize_t size)
        {
            register PyStringObject *op;
            ...
            if (size == 1 && str != NULL &&
                (op = characters[*str & UCHAR_MAX]) != NULL)
            {
                return (PyObject *)op;
            }

        ...
        }

*******************************************************************************
3.5 ``PyStringObject`` 效率相关问题
*******************************************************************************

Python 的字符串连接时严重影响 Python 程序执行效率， Python 通过 "+" 进行字符串连接\
的方法极其低下， 根源在于 Python 中的 ``PyStringObject`` 对象是一个不可变对象。 这\
意味着进行字符串连接时， 必须创建一个新的 ``PyStringObject`` 对象。 这样如果要连接 \
N 个 ``PyStringObject`` 对象， 就必须进行 ``N - 1`` 次的内存申请及搬运工作。 

推荐的做法是通过利用 ``PyStringObject`` 对象的 ``join`` 操作来对存储在 ``list`` \
或 ``tuple`` 中的一组 ``PyStringObject`` 对象进行连接操作， 这样只需分配一次内存\
， 执行效率大大提高。

通过 "+" 操作符对字符串进行连接时， 会调用 ``string_concat`` 函数：

.. code-block:: c 

    static PyObject *
    string_concat(register PyStringObject *a, register PyObject *bb)
    {
        register Py_ssize_t size;
        register PyStringObject *op;
        if (!PyString_Check(bb)) {
    #ifdef Py_USING_UNICODE
            if (PyUnicode_Check(bb))
                return PyUnicode_Concat((PyObject *)a, bb);
    #endif
            PyErr_Format(PyExc_TypeError,
                    "cannot concatenate 'str' and '%.200s' objects",
                    bb->ob_type->tp_name);
            return NULL;
        }
    #define b ((PyStringObject *)bb)
        /* Optimize cases with empty left or right operand */
        if ((a->ob_size == 0 || b->ob_size == 0) &&
            PyString_CheckExact(a) && PyString_CheckExact(b)) {
            if (a->ob_size == 0) {
                Py_INCREF(bb);
                return bb;
            }
            Py_INCREF(a);
            return (PyObject *)a;
        }
        // 计算字符串连接后的长度 size 
        size = a->ob_size + b->ob_size;
        if (size < 0) {
            PyErr_SetString(PyExc_OverflowError,
                    "strings are too large to concat");
            return NULL;
        }
        
        /* Inline PyObject_NewVar */
        // 创建新的 PyStringObject 对象 ， 其维护的用于存储字符的内存长度为 size
        op = (PyStringObject *)PyObject_MALLOC(sizeof(PyStringObject) + size);
        if (op == NULL)
            return PyErr_NoMemory();
        PyObject_INIT_VAR(op, &PyString_Type, size);
        op->ob_shash = -1;
        op->ob_sstate = SSTATE_NOT_INTERNED;
        // 将 a 和 b 中的字符拷贝到新建的 PyStringObject 中 
        Py_MEMCPY(op->ob_sval, a->ob_sval, a->ob_size);
        Py_MEMCPY(op->ob_sval + a->ob_size, b->ob_sval, b->ob_size);
        op->ob_sval[size] = '\0';
        return (PyObject *) op;
    #undef b
    }

对于任意两个 ``PyStringObject`` 对象的连接， 就会进行一次内存申请的动作。 而如果利\
用 ``PyStringObject`` 对象的 ``join`` 操作， 则会进行如下的动作 (假设是对 \
``list`` 中的 ``PyStringObject`` 对象进行连接)：

.. code-block:: c  

    static PyObject *
    string_join(PyStringObject *self, PyObject *orig)
    {
        char *sep = PyString_AS_STRING(self);
        // 假设调用 "abc".join(list) ， 那么 self 就是 "abc" 对应的 PyStringObject 
        // 对象 ， 所以 seplen 中存储着 abc 的长度 。 
        const Py_ssize_t seplen = PyString_GET_SIZE(self);
        PyObject *res = NULL;
        char *p;
        Py_ssize_t seqlen = 0;
        size_t sz = 0;
        Py_ssize_t i;
        PyObject *seq, *item;

        seq = PySequence_Fast(orig, "");
        if (seq == NULL) {
            return NULL;
        }
        
        // 获取 list 中 PyStringObject 对象的个数， 保存在 seqlen 中
        seqlen = PySequence_Size(seq);
        if (seqlen == 0) {
            Py_DECREF(seq);
            return PyString_FromString("");
        }
        if (seqlen == 1) {
            item = PySequence_Fast_GET_ITEM(seq, 0);
            if (PyString_CheckExact(item) || PyUnicode_CheckExact(item)) {
                Py_INCREF(item);
                Py_DECREF(seq);
                return item;
            }
        }

        /* There are at least two things to join, or else we have a subclass
        * of the builtin types in the sequence.
        * Do a pre-pass to figure out the total amount of space we'll
        * need (sz), see whether any argument is absurd, and defer to
        * the Unicode join if appropriate.
        */
        // 遍历 list 中每个字符串 ， 累加获得 连接 list 中所有字符串后的长度
        for (i = 0; i < seqlen; i++) {
            const size_t old_sz = sz;
            // seq为python 中的 list 对象 ， 这里获取其中第 i 个字符串 。
            item = PySequence_Fast_GET_ITEM(seq, i);
            if (!PyString_Check(item)){
    #ifdef Py_USING_UNICODE
                if (PyUnicode_Check(item)) {
                    /* Defer to Unicode join.
                    * CAUTION:  There's no gurantee that the
                    * original sequence can be iterated over
                    * again, so we must pass seq here.
                    */
                    PyObject *result;
                    result = PyUnicode_Join((PyObject *)self, seq);
                    Py_DECREF(seq);
                    return result;
                }
    #endif
                PyErr_Format(PyExc_TypeError,
                        "sequence item %zd: expected string,"
                        " %.80s found",
                        i, item->ob_type->tp_name);
                Py_DECREF(seq);
                return NULL;
            }
            sz += PyString_GET_SIZE(item);
            if (i != 0)
                sz += seplen;
            if (sz < old_sz || sz > PY_SSIZE_T_MAX) {
                PyErr_SetString(PyExc_OverflowError,
                    "join() result is too long for a Python string");
                Py_DECREF(seq);
                return NULL;
            }
        }

        /* Allocate result space. */
        // 创建长度为 sz 的 PyStringObject 对象 
        res = PyString_FromStringAndSize((char*)NULL, sz);
        if (res == NULL) {
            Py_DECREF(seq);
            return NULL;
        }

        /* Catenate everything. */
        // 将 list 中的字符串拷贝到新创建的 PyStringObject 对象中 
        p = PyString_AS_STRING(res);
        for (i = 0; i < seqlen; ++i) {
            size_t n;
            item = PySequence_Fast_GET_ITEM(seq, i);
            n = PyString_GET_SIZE(item);
            Py_MEMCPY(p, PyString_AS_STRING(item), n);
            p += n;
            if (i < seqlen - 1) {
                Py_MEMCPY(p, sep, seplen);
                p += seplen;
            }
        }

        Py_DECREF(seq);
        return res;
    }

执行 ``join`` 操作时， 会先统计 ``list`` 中共有多少个 ``PyStringObject`` 对象， \
并统计这些 ``PyStringObject`` 对象所维护的字符串一共的长度， 然后申请内存， 将 \
``list`` 中所有的 ``PyStringObject`` 对象维护的字符串都拷贝到新开辟的内存空间中。 \
这里只进行了一次内存申请就完成了 N 个 ``PyStringObject`` 对象的连接操作。 相比于 \
"+" 提升了效率。

通过在 ``string_concat`` 和 ``string_join`` 中添加输出代码， 可以清晰看到两种字符\
串连接的的区别：

.. figure:: img/3-4.png
    :align: center

*******************************************************************************
3.6 Hack ``PyStringObject``
*******************************************************************************

对 ``PyStringObject`` 对象的运行时的行为进行两项观察。 首先观察 intern 机制， 在 \
Python Interactive 环境中， 创建一个 ``PyStringObject`` 对象后， 会对这个 \
``PyStringObject`` 对象进行 intern 操作， 因此期望内容相同的 ``PyStringObject`` \
对象在 intern 后应该是同一个对象， 观察结果：

.. figure:: img/3-5.png
    :align: center

通过在 ``string_length`` 中添加打印地址和引用计数的代码， 可以在 Python 运行期间获\
得每一个 ``PyStringObject`` 对象的地址及引用计数 (在 address 下一行输出的不是字符\
串的长度信息， 已将其更换为引用计数信息)。 归于一般的字符串及单个字符， intern 机制\
最终会使不同的 ``PyStringObject*`` 指针指向相同的对象。 

观察进行缓冲处理的字符对象， 同样在 ``string_length`` 中添加代码， 打印出缓冲池中\
从 a 到 e 的字符对象的引用计数信息。 为了避免执行 ``len()`` 对引用计数的影响， 不会\
对 a 到 e 的字符对象调用 ``len`` 操作， 而是对另外的 ``PyStringObject`` 对象调用 \
``len`` 操作： 

.. code-block:: c 

    static Py_ssize_t
    string_length(PyStringObject *a)
    {
        return a->ob_size;
    }

上述代码是 ``string_length`` 函数的原始代码， 修改为如下：

.. code-block:: c 

    static void ShowCharacter()
    {
        char chA = 'a';
        PyStringObject** posA = characters + (unsigned short)chA;
        int i;
        char value[5];
        int refcnts[5];
        for (i=0; i<5; ++i)
        {
            PyStringObject* strObj = posA[i];
            value[i] = strObj->ob_sval[0];
            refcnts[i] = strObj->ob_refcnt;
        }
        printf(" value: ");
        for (i=0;i<5;++i)
        {
            printf("%c\t", value[i]);
        }
        printf("\nrefcnt: ");
        for (i=0;i<5;++i)
        {
            printf("%d\t", refcnts[i]);
        }
        printf("\n");
    }

图 3-6 展示了观察的结果， 在创建字符对象时， Python 确实只使用了缓冲池里的对象， 没\
有创建新的对象。 

.. figure:: img/3-6.png
    :align: center
