Python 源码阅读系列 1
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



