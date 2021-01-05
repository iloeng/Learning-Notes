##############################################################################
Python 源码阅读系列 6
##############################################################################

.. contents::

******************************************************************************
第 4 章  Python 中的 List 对象
******************************************************************************

4.2 PyListObject 对象的创建与维护
==============================================================================

4.2.3 插入元素
------------------------------------------------------------------------------

Python 内部通过调用 PyList_Insert 来完成元素的插入动作 ， 而 PyList_Insert 实际上\
调用了 Python 内部的 ins1 。 在 ins1 中为了完成元素的插入工作 ， 必须首先保证一个条\
件得到满足 ， PyListObject 对象必须有足够的内存来容纳插入的元素 。 [1] 处调用了 \
list_resize 函数来保证该条件一定能成立 。 这个函数改变了 PyListObject 所维护的 \
PyObject* 列表的大小 。

.. code-block:: c 

    [Objects/listobject.c]

    static int
    list_resize(PyListObject *self, Py_ssize_t newsize)
    {
        PyObject **items;
        size_t new_allocated;
        Py_ssize_t allocated = self->allocated;

        /* Bypass realloc() when a previous overallocation is large enough
        to accommodate the newsize.  If the newsize falls lower than half
        the allocated size, then proceed with the realloc() to shrink the list.
        */
        // 不需要重新申请内存
        if (allocated >= newsize && newsize >= (allocated >> 1)) {
            assert(self->ob_item != NULL || newsize == 0);
            self->ob_size = newsize;
            return 0;
        }

        /* This over-allocates proportional to the list size, making room
        * for additional growth.  The over-allocation is mild, but is
        * enough to give linear-time amortized behavior over a long
        * sequence of appends() in the presence of a poorly-performing
        * system realloc().
        * The growth pattern is:  0, 4, 8, 16, 25, 35, 46, 58, 72, 88, ...
        */
        // 计算重新申请的内存大小
        new_allocated = (newsize >> 3) + (newsize < 9 ? 3 : 6) + newsize;
        if (newsize == 0)
            new_allocated = 0;
        // 拓展列表
        items = self->ob_item;
        if (new_allocated <= ((~(size_t)0) / sizeof(PyObject *)))
            PyMem_RESIZE(items, PyObject *, new_allocated);
            // 最终调用 C 中的 realloc
        else
            items = NULL;
        if (items == NULL) {
            PyErr_NoMemory();
            return -1;
        }
        self->ob_item = items;
        self->ob_size = newsize;
        self->allocated = new_allocated;
        return 0;
    }

在调整 PyListObject 对象所维护的列表的内存时 ， Python 分两种情况处理 ： 

- newsize < allocated && newsize > allocated/2 : 简单调整 ob_size 值 ； 

- 其他情况 ， 调用 realloc 重新分配空间 。 

在第二中情况下 ， 当 newsize < allocated/2 时 ， Python 会通过 realloc 来收缩列\
表的内存空间 。

将 PyListObject 的空间调整后 ， 函数 ins1 在实际插入元素之前还需在 [2] 处确定元素\
的插入点 。 Python 的 list 操作灵活 ， 支持负值索引 ， 比如一个 n 个元素的 list: \
lst[n] ， 那么 lst[-1] 就是 lst[n-1] 。 

在确定了插入的位置之后 ， [3] 处开始搬动元素 ， 将插入点之后的所有元素向下挪动一个\
位置 ， 这样在插入点就能空出一个位置来 。 一旦搬移元素的工作完成 ， 实际上就大功告成\
了 ， 想插入的元素就又容身之地了 。 

.. image:: img/4-4.png

值得注意的是 ， 通过与 vector 类似的内存管理机制 ， PyListObject 的 allocated 已\
经变成 10 了 ， 而 ob_size 却只有 7 。

在 Python 中 ， list 还有另一个被广泛使用的插入操作 append 。 与上面的插入操作类\
似 ： 

.. code-block:: c 

    [Objects/listobject.c]

    // Python 提供的 C API
    int
    PyList_Append(PyObject *op, PyObject *newitem)
    {
        if (PyList_Check(op) && (newitem != NULL))
            return app1((PyListObject *)op, newitem);
        PyErr_BadInternalCall();
        return -1;
    }

    // 与 append 对对应的 C 函数
    static PyObject *
    listappend(PyListObject *self, PyObject *v)
    {
        if (app1(self, v) == 0)
            Py_RETURN_NONE;
        return NULL;
    }

    static int
    app1(PyListObject *self, PyObject *v)
    {
        Py_ssize_t n = PyList_GET_SIZE(self);

        assert (v != NULL);
        if (n == PY_SSIZE_T_MAX) {
            PyErr_SetString(PyExc_OverflowError,
                "cannot add more objects to list");
            return -1;
        }

        if (list_resize(self, n+1) == -1)
            return -1;

        Py_INCREF(v);
        PyList_SET_ITEM(self, n, v);  // 设置操作
        return 0;
    }

在进行 append 动作的时候 ， 添加的元素是添加在第 ob_size + 1 个位置上的 (即 \
list[ob_size] 处) ， 而不是第 allocated 个位置上 。 

.. image:: img/4-5.png

在 app1 中调用 list_resize 时 ， 由于 newsize(8) 在 5 和 10 之间 ， 所以不需要在\
分配内存空间了 。 直接将 101 放置到第 8 个位置上即可 。 

4.2.3 删除元素
------------------------------------------------------------------------------

对于一个容器而言 ， 创建 、 设置 、 插入和删除操作是必需的 。 

.. image:: img/4-6.png

图 4-6 删除元素的例子

当 Python 执行 lst.remove(3) 时 ， PyListObject 中的 listremove 操作会被激活 ：

.. code-block:: c 

    [Objects/listobject.c]

    static PyObject *
    listremove(PyListObject *self, PyObject *v)
    {
        Py_ssize_t i;

        for (i = 0; i < self->ob_size; i++) {
            // 比较 list 中的元素与待删除的元素 v
            int cmp = PyObject_RichCompareBool(self->ob_item[i], v, Py_EQ);
            if (cmp > 0) {
                if (list_ass_slice(self, i, i+1,
                        (PyObject *)NULL) == 0)
                    Py_RETURN_NONE;
                return NULL;
            }
            else if (cmp < 0)
                return NULL;
        }
        PyErr_SetString(PyExc_ValueError, "list.remove(x): x not in list");
        return NULL;
    }

在遍历 PyListObject 中所有元素的过程中 ， 将待删除的元素与 PyListObject 中的每个元\
素一一进行比较 ， 比较操作通过 PyObject_RichCompareBool 完成 ， 如果其返回值大于 0 \
， 则表示列表中的某个元素与待删除的元素匹配 。 一旦在列表中发现匹配的元素 ， Python \
会立即调用 list_ass_slice 删除给元素 。 其函数原型如下 ： 

.. code-block:: c 

    [Objects/listobject.c]

    static int
    list_ass_slice(PyListObject *a, Py_ssize_t ilow, Py_ssize_t ihigh, PyObject *v)
    {
        /* Because [X]DECREF can recursively invoke list operations on
        this list, we must postpone all [X]DECREF activity until
        after the list is back in its canonical shape.  Therefore
        we must allocate an additional array, 'recycle', into which
        we temporarily copy the items that are deleted from the
        list. :-( */
        PyObject *recycle_on_stack[8];
        PyObject **recycle = recycle_on_stack; /* will allocate more if needed */
        PyObject **item;
        PyObject **vitem = NULL;
        PyObject *v_as_SF = NULL; /* PySequence_Fast(v) */
        Py_ssize_t n; /* # of elements in replacement list */
        Py_ssize_t norig; /* # of elements in list getting replaced */
        Py_ssize_t d; /* Change in size */
        Py_ssize_t k;
        size_t s;
        int result = -1;	/* guilty until proved innocent */
    #define b ((PyListObject *)v)
        if (v == NULL)
            n = 0;
        else {
            if (a == b) {
                /* Special case "a[i:j] = a" -- copy b first */
                v = list_slice(b, 0, b->ob_size);
                if (v == NULL)
                    return result;
                result = list_ass_slice(a, ilow, ihigh, v);
                Py_DECREF(v);
                return result;
            }
            v_as_SF = PySequence_Fast(v, "can only assign an iterable");
            if(v_as_SF == NULL)
                goto Error;
            n = PySequence_Fast_GET_SIZE(v_as_SF);
            vitem = PySequence_Fast_ITEMS(v_as_SF);
        }
        if (ilow < 0)
            ilow = 0;
        else if (ilow > a->ob_size)
            ilow = a->ob_size;

        if (ihigh < ilow)
            ihigh = ilow;
        else if (ihigh > a->ob_size)
            ihigh = a->ob_size;

        norig = ihigh - ilow;
        assert(norig >= 0);
        d = n - norig;
        if (a->ob_size + d == 0) {
            Py_XDECREF(v_as_SF);
            return list_clear(a);
        }
        item = a->ob_item;
        /* recycle the items that we are about to remove */
        s = norig * sizeof(PyObject *);
        if (s > sizeof(recycle_on_stack)) {
            recycle = (PyObject **)PyMem_MALLOC(s);
            if (recycle == NULL) {
                PyErr_NoMemory();
                goto Error;
            }
        }
        memcpy(recycle, &item[ilow], s);

        if (d < 0) { /* Delete -d items */
            memmove(&item[ihigh+d], &item[ihigh],
                (a->ob_size - ihigh)*sizeof(PyObject *));
            list_resize(a, a->ob_size + d);
            item = a->ob_item;
        }
        else if (d > 0) { /* Insert d items */
            k = a->ob_size;
            if (list_resize(a, k+d) < 0)
                goto Error;
            item = a->ob_item;
            memmove(&item[ihigh+d], &item[ihigh],
                (k - ihigh)*sizeof(PyObject *));
        }
        for (k = 0; k < n; k++, ilow++) {
            PyObject *w = vitem[k];
            Py_XINCREF(w);
            item[ilow] = w;
        }
        for (k = norig - 1; k >= 0; --k)
            Py_XDECREF(recycle[k]);
        result = 0;
    Error:
        if (recycle != recycle_on_stack)
            PyMem_FREE(recycle);
        Py_XDECREF(v_as_SF);
        return result;
    #undef b
    }

list_ass_slice 实际上并不是一个专用于删除操作的函数 ， 它的完整功能如下 ：

- a[ilow:ihigh] = v if v != NULL.

- del a[ilow:ihigh] if v == NULL.

它实际上有着 replace 和 remove 两种语义 ， 决定使用哪种语义的是最后一个参数 v 决定 。

.. image:: img/4-7.png

图 4-7 list_ass_slice 的不同语义

