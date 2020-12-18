##############################################################################
Python 源码阅读系列 6
##############################################################################

.. contents::

******************************************************************************
第 3 章  Python 中的字符串对象
******************************************************************************

3.5 PyStringObject 效率相关问题
==============================================================================

承接上文字符串连接 。

通过 "+" 操作符对字符串进行连接时 ， 会调用 string_concat 函数 ：

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

对于任意两个 PyStringObject 对象的连接 ， 就会进行一次内存申请的动作 。 而如果利用 \
PyStringObject 对象的 join 操作 ， 则会进行如下的动作 (假设是对 list 中的 \
PyStringObject 对象进行连接) ：

.. code-block:: c  

    static PyObject *
    string_join(PyStringObject *self, PyObject *orig)
    {
        char *sep = PyString_AS_STRING(self);
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
        for (i = 0; i < seqlen; i++) {
            const size_t old_sz = sz;
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
        res = PyString_FromStringAndSize((char*)NULL, sz);
        if (res == NULL) {
            Py_DECREF(seq);
            return NULL;
        }

        /* Catenate everything. */
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