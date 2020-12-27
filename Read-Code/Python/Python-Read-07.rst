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

在确定了插入的位置之后 ， [3] 处开始搬动元素 ， 将插入点之后的所有元素向下 
