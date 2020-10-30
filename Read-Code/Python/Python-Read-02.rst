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


