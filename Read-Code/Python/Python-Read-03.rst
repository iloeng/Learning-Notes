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


