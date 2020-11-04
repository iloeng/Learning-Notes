Python 源码阅读系列 2
---------------------------------

前言
===================

承接上文。

1.2.2 对象的行为
======================

对于一种类型，可以完全同时定义三个函数族中的所有操作。即一个对象可以既表现出数值对象的特性\
也可以表现出关联对象的特性。

.. image:: img/1-4.png

图 1-4  数值对象和关联对象的混合体

看上去 a['key'] 操作是一个类似于 dict 这样的对象才会支持的操作。从 int 继承出来的 MyInt \
应该自然就是一个数值对象，但是通过重写 __getitem__ 这个 Python 中的 special method , 可\
以视为指定了 MyInt 在 Python 内部对应的 PyTypeObject 对象的 tp_as_mapping.mp_subscript \
操作。最终 MyInt 的实例对象可以 “表现” 得像一个关联对象。归根结底在于 PyTypeObject 中允许\
一种类型同时指定三种不同对象的行为特性。

1.2.3 类型的类型
=======================

在 PyTypeObject 定义的最开始，可以发现 PyObject_VAR_HEAD ，意味着 Python 中的类型实际上\
也是一个对象。在 Python 中，任何一个东西都是对象，而每个对象都对应一种类型，那么类型对象的\
类型是什么？对于其他对象可以通过与其关联的类型对象确定其类型，可以通过 PyType_Type 来确定\
一个对象是类型对象。

.. code-block:: c

    [Objects/typeobject.c]

    PyTypeObject PyType_Type = {
        PyObject_HEAD_INIT(&PyType_Type)
        0,					/* ob_size */
        "type",					/* tp_name */
        sizeof(PyHeapTypeObject),		/* tp_basicsize */
        sizeof(PyMemberDef),			/* tp_itemsize */
        (destructor)type_dealloc,		/* tp_dealloc */
        0,					/* tp_print */
        0,			 		/* tp_getattr */
        0,					/* tp_setattr */
        type_compare,				/* tp_compare */
        (reprfunc)type_repr,			/* tp_repr */
        0,					/* tp_as_number */
        0,					/* tp_as_sequence */
        0,					/* tp_as_mapping */
        (hashfunc)_Py_HashPointer,		/* tp_hash */
        (ternaryfunc)type_call,			/* tp_call */
        0,					/* tp_str */
        (getattrofunc)type_getattro,		/* tp_getattro */
        (setattrofunc)type_setattro,		/* tp_setattro */
        0,					/* tp_as_buffer */
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
            Py_TPFLAGS_BASETYPE,		/* tp_flags */
        type_doc,				/* tp_doc */
        (traverseproc)type_traverse,		/* tp_traverse */
        (inquiry)type_clear,			/* tp_clear */
        0,					/* tp_richcompare */
        offsetof(PyTypeObject, tp_weaklist),	/* tp_weaklistoffset */
        0,					/* tp_iter */
        0,					/* tp_iternext */
        type_methods,				/* tp_methods */
        type_members,				/* tp_members */
        type_getsets,				/* tp_getset */
        0,					/* tp_base */
        0,					/* tp_dict */
        0,					/* tp_descr_get */
        0,					/* tp_descr_set */
        offsetof(PyTypeObject, tp_dict),	/* tp_dictoffset */
        0,					/* tp_init */
        0,					/* tp_alloc */
        type_new,				/* tp_new */
        PyObject_GC_Del,        		/* tp_free */
        (inquiry)type_is_gc,			/* tp_is_gc */
    };

PyType_Type 在 Python 的类型机制中是一个至关重要的对象，所有用户自定义 class 所对应的 \
PyTypeObject 对象都是通过这个对象创建的。

.. image:: img/1-5.png

图 1-5 PyType_Type 与一般 PyTypeObject 的关系

图 1-5 中一再出现的 <type 'type'> 就是 Python 内部的 PyType_Type , 它是所有 class 的 \
class ,所以在 Python 中被称为 metaclass 。关于 PyType_Type 和 metaclass 后面详细剖析。

接着来看 PyInt_Type 是怎么与 PyType_Type 建立关系的。在 Python 中，每个对象都将自己的\
引用计数、类型信息保存在开始的部分中，为了方便对这部分内存的初始化， Python 提供了有用的\
宏：

.. code-block:: c 

    [Include/object.h]

    #ifdef Py_TRACE_REFS
    /* Define pointers to support a doubly-linked list of all live heap objects. */
        #define _PyObject_HEAD_EXTRA		\
            struct _object *_ob_next;	\
            struct _object *_ob_prev;

    #define _PyObject_EXTRA_INIT 0, 0,

    #else
    #define _PyObject_HEAD_EXTRA
    #define _PyObject_EXTRA_INIT
    #endif

Python 2.5 的代码是上述内容，书中的代码如下：

.. code-block:: c 

    [Include/object.h]
    #ifdef Py_TRACE_REFS

        #define _PyObject_EXTRA_INIT 0, 0,

    #else
    
        #define _PyObject_EXTRA_INIT
    #endif

    #define PyObject_HEAD_INIT(type)    \
        _PyObject_EXTRA_INIT    \
        1, type,

回顾一下 PyObject 和 PyVarObject 的定义，初始化的动作就一目了然了。实际上，这些宏在各种\
內建类型对象的初始化中被大量地使用着。

以 PyInt_Type 为例，可以更清晰地看到一般的类型对象和这个特立独行的 PyType_Type 对象之间\
的关系：

.. code-block:: c 

    [Objects/intobject.c]

    PyTypeObject PyInt_Type = {
        PyObject_HEAD_INIT(&PyType_Type)
        0,
        "int",
        sizeof(PyIntObject),
        0,
        (destructor)int_dealloc,		/* tp_dealloc */
        (printfunc)int_print,			/* tp_print */
        0,					/* tp_getattr */
        0,					/* tp_setattr */
        (cmpfunc)int_compare,			/* tp_compare */
        (reprfunc)int_repr,			/* tp_repr */
        &int_as_number,				/* tp_as_number */
        0,					/* tp_as_sequence */
        0,					/* tp_as_mapping */
        (hashfunc)int_hash,			/* tp_hash */
            0,					/* tp_call */
            (reprfunc)int_repr,			/* tp_str */
        PyObject_GenericGetAttr,		/* tp_getattro */
        0,					/* tp_setattro */
        0,					/* tp_as_buffer */
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES |
            Py_TPFLAGS_BASETYPE,		/* tp_flags */
        int_doc,				/* tp_doc */
        0,					/* tp_traverse */
        0,					/* tp_clear */
        0,					/* tp_richcompare */
        0,					/* tp_weaklistoffset */
        0,					/* tp_iter */
        0,					/* tp_iternext */
        int_methods,				/* tp_methods */
        0,					/* tp_members */
        0,					/* tp_getset */
        0,					/* tp_base */
        0,					/* tp_dict */
        0,					/* tp_descr_get */
        0,					/* tp_descr_set */
        0,					/* tp_dictoffset */
        0,					/* tp_init */
        0,					/* tp_alloc */
        int_new,				/* tp_new */
        (freefunc)int_free,           		/* tp_free */
    };

可以通过想象看到一个整数对象在运行是的形象表示，如图 1-6 所示：

.. image:: img/1-6.png

图 1-6 运行时整数对象及其类型之间的关系

Python 对象的多态性
===============================

通过 PyObject 和 PyTypeObject , Python 利用 C 语言完成了 C++ 所提供的对象的多态的特性。\
在 Python 中创建一个对象，比如 PyIntObject 对象时，会分配内存，进行初始化。然后 Python \
内部会用一个 PyObject\* 变量，而不是通过一个 PyIntObject\* 变量来保存和维护这个对象。其\
他对象与此类似，所以在 Python 内部各个函数之间传递的都是一种范型指针 -- PyObject\* 。并不\
知道这个指针所指的队形究竟是什么类型的，只能从指针所指对象的 ob_type 域进行动态判断，而正\
是通过这个域， Python 实现了多态机制。

看一下 Print 函数：

.. code-block:: c

    void Print(PyObject* object)
    {
        object->ob_type->tp_print(object);
    }

如果传给 Print 的指针是一个 PyIntObject* , 那么他就会调用 PyIntObject 对象对应的类型对象\
中定义的输出操作，如果指针是一个 PyStringObject* ，那么就会调用 PyStringObject 对象对应\
的类型对象中定义的输出操作。可以看到，这里同一个函数在不同情况下表现出不同的行为，这正是多态\
的核心所在。

前文提到的 AOL 的 C API 正是建立在这种 “多态” 机制上的。

.. code-block:: c 

    long
    PyObject_Hash(PyObject *v)
    {
        PyTypeObject *tp = v->ob_type;
        if (tp->tp_hash != NULL)
            return (*tp->tp_hash)(v);
        if (tp->tp_compare == NULL && RICHCOMPARE(tp) == NULL) {
            return _Py_HashPointer(v); /* Use address as hash value */
        }
        /* If there's a cmp but no hash defined, the object can't be hashed */
        PyErr_Format(PyExc_TypeError, "unhashable type: '%.200s'",
                v->ob_type->tp_name);
        return -1;
    }

引用计数
================

在 C 或 C++ 中， 程序员被赋予了极大的自由，可以任意申请内存。但是权力的另一面则对应着责任，\
程序员必须负责将申请的内存释放，并释放无效指针。

现代的开发语言中一般都选择有语言本身负责内存的管理个维护，即采用了垃圾回收机制，比如 Java \
和 C# 。垃圾回收机制使开发人员从维护内存分配和清理的繁重工作中解放出来，但同时也剥夺了程序\
员与内存亲密接触的机会，并付出了一定的运行效率作为代价。 Python 同样内建了垃圾回收机制，代\
替程序员进行繁重的内存管理工作，而引用计数正是 Python 垃圾回收集中的一部分。

Python 通过对一个对象的引用计数的管理来维护对象在内存中存在与否。Python 中每个东西都是一\
个对象，都有一个 ob_refcnt 变量。这个变量维护着该对象的引用计数，从而也决定着该对象的创建\
与消亡。

在 Python 中，主要是通过 Py_INCREF(op) 和 PyDECREF(op) 两个宏来增加和减少一个对象的引用\
计数。当一个对象的引用计数减少到 0 后， PyDECREF 将调用该对象的析构函数来释放该对象所占用\
的内存和系统资源。这里的 ‘析构函数’ 借用了 C++ 的词汇，实际上这个析构动作是通过在对象对应\
的类型对象中顶一个的一个函数指针来指定的，就是 tp_dealloc 。

在 ob_refcnt 减为 0 后，将触发对象销毁的事件。在 Python 的对象体系中来看，各个对象提供了\
不同的事件处理函数，而事件的注册动作正是在各个对象对应的类型对象中静态完成的。

PyObject 中的 ob_refcnt 是一个 32 位的整型变量，实际蕴含着 Python 所做的一个假设，即对\
一个对象的引用不会超过一个整型变量的最大值。一般情况下，如果不是恶意代码，这个假设是成立的。

需要注意的是，在 Python 的各个对象中，类型对象是超越引用计数规则的。类型对象永远不会被析\
构。每个对象中指向类型对象的指针被视为类型对象的引用。

在每个对象创建的时候，Python 提供了一个 _Py_NewReference(op) 宏来将对象的引用计数初始化为\
1 。

在 Python 的源代码中可以看到，在不同的编译选项下 (Py_REF_DEBUG, Py_TRACE_REFS), 引用计数\
的宏还要做许多额外的工作。 以下是 Python 最终发行时这些宏对应的实际代码

.. code-block:: c 

    [Include/object.h]

    #define _Py_NewReference(op) ((op)->ob_refcnt = 1)

    #define _Py_ForgetReference(op) _Py_INC_TPFREES(op)

    #define _Py_Dealloc(op) ((*(op)->ob_type->tp_dealloc)((PyObject *)(op)))

    #define Py_INCREF(op) ((op)->ob_refcnt++)

    #define Py_DECREF(op)					\
        if (--(op)->ob_refcnt != 0)			\
            ;			\
        else						\
            _Py_Dealloc((PyObject *)(op))

    #define Py_XINCREF(op) if ((op) == NULL) ; else Py_INCREF(op)
    #define Py_XDECREF(op) if ((op) == NULL) ; else Py_DECREF(op)

在每个对象的引用计数减为 0 时，与该对象对应的析构函数就会被调用，但是要特别注意的是，调用\
析构函数并不意味着最终一定会调用 free 释放内存空间，频繁地申请和释放内存空间会使 Python \
的执行效率大打折扣。一般来说， Python 中大量采用了内存对象池的技术，使用这种技术可以避免\
频繁申请和释放内存。因此在析构时，通常都是将对象占用的空间归还到内存池中。这一点在 Python \
内建对象的实现中可以看得一清二楚。

Python 对象的分类
===============================

将 Python 的对象从概念上大致分为 5 类：

- Fundamental 对象： 类型对象
- Numeric 对象： 数值对象
- Sequence 对象： 容纳其他对象的序列集合对象
- Mapping 对象： 类似于 C++ 中 map 的关联对象
- Internal 对象： Python 虚拟机在运行使内部使用的对象

.. image:: img/1-7.png

图 1-7 Python 中对象的分类

Python 中的整数对象
=======================

初识 PyIntObject 对象
+++++++++++++++++++++++++++

除了 “定长对象” 和 “变长对象” 这种对象的二分法，根据对象维护数据的可变性可将对象分为 \
"可变对象 (mutable)" 和 "不可变对象 (immutable)" 。 PyIntObject 对象就是一个不可\
变对象，也就是创建一个 PyIntObject 对象之后，就无法更改该对象的值了。字符串对象也是。

整数对象池是整数对象的缓冲池机制。在此基础上，运行时的整数对象并非一个个对立的对象，而\
是如同自然界的蚂蚁一般，已经是通过一定的结构联结在一起的庞大的整数对象系统了。静态的整\
数对象的定义 -- PyIntObject：

.. code-block:: c 

    typedef struct {
        PyObject_HEAD
        long ob_ival;
    } PyIntObject;

PyIntObject 实际上就是对 C 中原生类型 long 的一个简单包装。 Python 对象中与对象相\
关的元信息实际上都是保存在与对象对应的类型对象中的，对于 PyIntObject 的类型对象是 \
PyInt_Type：

.. code-block:: c

    [Objects/intobject.c]

    PyTypeObject PyInt_Type = {
        PyObject_HEAD_INIT(&PyType_Type)
        0,
        "int",
        sizeof(PyIntObject),
        0,
        (destructor)int_dealloc,		/* tp_dealloc */
        (printfunc)int_print,			/* tp_print */
        0,					/* tp_getattr */
        0,					/* tp_setattr */
        (cmpfunc)int_compare,			/* tp_compare */
        (reprfunc)int_repr,			/* tp_repr */
        &int_as_number,				/* tp_as_number */
        0,					/* tp_as_sequence */
        0,					/* tp_as_mapping */
        (hashfunc)int_hash,			/* tp_hash */
            0,					/* tp_call */
            (reprfunc)int_repr,			/* tp_str */
        PyObject_GenericGetAttr,		/* tp_getattro */
        0,					/* tp_setattro */
        0,					/* tp_as_buffer */
        Py_TPFLAGS_DEFAULT | Py_TPFLAGS_CHECKTYPES |
            Py_TPFLAGS_BASETYPE,		/* tp_flags */
        int_doc,				/* tp_doc */
        0,					/* tp_traverse */
        0,					/* tp_clear */
        0,					/* tp_richcompare */
        0,					/* tp_weaklistoffset */
        0,					/* tp_iter */
        0,					/* tp_iternext */
        int_methods,				/* tp_methods */
        0,					/* tp_members */
        0,					/* tp_getset */
        0,					/* tp_base */
        0,					/* tp_dict */
        0,					/* tp_descr_get */
        0,					/* tp_descr_set */
        0,					/* tp_dictoffset */
        0,					/* tp_init */
        0,					/* tp_alloc */
        int_new,				/* tp_new */
        (freefunc)int_free,           		/* tp_free */
    };

PyIntObject 支持的操作：

==============   ============================
操作              描述
==============   ============================
int_dealloc       PyIntObject 对象的析构操作
int_free          PyIntObject 对象的释放操作
int_repr          转化成 PyStringObject 对象
int_hash          获得 HASH 值
int_print         打印 PyIntObject 对象
int_compare       比较操作
int_as_number     数值操作集合
int_methods       成员函数集合
==============   ============================

下面这个例子看一下如何比较两个整数对象的大小。

.. code-block:: c 

    [Objects/intobject.c]

    static int
    int_compare(PyIntObject *v, PyIntObject *w)
    {
        register long i = v->ob_ival;
        register long j = w->ob_ival;
        return (i < j) ? -1 : (i > j) ? 1 : 0;
    }

显然 PyIntObject 对象的比较操作实际上就是简单地将他所维护的 long 值进行比较。需要特别注\
意 int_as_number

.. code-block:: c 

    [Objects/intobject.c]

    static PyNumberMethods int_as_number = {
        (binaryfunc)int_add,	/*nb_add*/
        (binaryfunc)int_sub,	/*nb_subtract*/
        (binaryfunc)int_mul,	/*nb_multiply*/
        (binaryfunc)int_classic_div, /*nb_divide*/
        (binaryfunc)int_mod,	/*nb_remainder*/
        (binaryfunc)int_divmod,	/*nb_divmod*/
        (ternaryfunc)int_pow,	/*nb_power*/
        (unaryfunc)int_neg,	/*nb_negative*/
        (unaryfunc)int_pos,	/*nb_positive*/
        (unaryfunc)int_abs,	/*nb_absolute*/
        (inquiry)int_nonzero,	/*nb_nonzero*/
        (unaryfunc)int_invert,	/*nb_invert*/
        (binaryfunc)int_lshift,	/*nb_lshift*/
        (binaryfunc)int_rshift,	/*nb_rshift*/
        (binaryfunc)int_and,	/*nb_and*/
        (binaryfunc)int_xor,	/*nb_xor*/
        (binaryfunc)int_or,	/*nb_or*/
        int_coerce,		/*nb_coerce*/
        (unaryfunc)int_int,	/*nb_int*/
        (unaryfunc)int_long,	/*nb_long*/
        (unaryfunc)int_float,	/*nb_float*/
        (unaryfunc)int_oct,	/*nb_oct*/
        (unaryfunc)int_hex, 	/*nb_hex*/
        0,			/*nb_inplace_add*/
        0,			/*nb_inplace_subtract*/
        0,			/*nb_inplace_multiply*/
        0,			/*nb_inplace_divide*/
        0,			/*nb_inplace_remainder*/
        0,			/*nb_inplace_power*/
        0,			/*nb_inplace_lshift*/
        0,			/*nb_inplace_rshift*/
        0,			/*nb_inplace_and*/
        0,			/*nb_inplace_xor*/
        0,			/*nb_inplace_or*/
        (binaryfunc)int_div,	/* nb_floor_divide */
        int_true_divide,	/* nb_true_divide */
        0,			/* nb_inplace_floor_divide */
        0,			/* nb_inplace_true_divide */
        (unaryfunc)int_int,	/* nb_index */
    };

这个 PyNumberMethods 中定义了一个对象作为数值对象时所有可选的操作信息。再 Python 2.5 中\
PyNumberMethods 中一共有 39 个函数指针，即其中定义了 39 种可选的操作，包括加法，减法，乘\
法, 模运算等。

再 int_as_number 中，确定了对于一个整数对象，这些数值操作应该如何进行。当然，并非所有的操\
作都要求一定要被实现。下面看一下加法操作的实现：

.. code-block:: c 

    [Include/intobject.h]

    #define PyInt_AS_LONG(op) (((PyIntObject *)(op))->ob_ival)

    [Objects/intobject.c]

    #define CONVERT_TO_LONG(obj, lng)		\
        if (PyInt_Check(obj)) {			\
            lng = PyInt_AS_LONG(obj);	\
        }					\
        else {					\
            Py_INCREF(Py_NotImplemented);	\
            return Py_NotImplemented;	\
        }

    static PyObject *
    int_add(PyIntObject *v, PyIntObject *w)
    {
        register long a, b, x;
        CONVERT_TO_LONG(v, a);
        CONVERT_TO_LONG(w, b);
        x = a + b;
        // [1]: 检查加法结果是否溢出
        if ((x^a) >= 0 || (x^b) >= 0)
            return PyInt_FromLong(x);
        return PyLong_Type.tp_as_number->nb_add((PyObject *)v, (PyObject *)w);
    }

PyIntObject 对象所实现的加法操作是直接在其维护的 long 值上进行的，在完成加法操作后，代码\
中进行了溢出检查，如果没有溢出就返回一个新的 PyIntObject ，这个 PyIntObject 所拥有的值正\
好是加法操作的结果。

未完待续...
