###############################################################################
Redis Beta 1 源码阅读笔记 - Type Definitions
###############################################################################

.. contents::

*******************************************************************************
Type Definitions
*******************************************************************************

.. _aeFileProc-typedef:
.. aeFileProc-typedef

01 aeFileProc 定义
===============================================================================

.. code-block:: c 

    /* Types and data structures */
    typedef void aeFileProc(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask);

在 c 语言中， 回调是通过函数指针实现的。 通过将回调函数地址传递给被调函数， 从而实现\
回调。 在这里， 通过定义函数指针 aeFileProc， 由调用方实现具体的函数内容， 在实际调\
用函数里， 把 aeFileProc 实现函数的地址传进来。 其实相当于定义一种接口， 由调用方来\
实现该接口。

.. _aeEventFinalizerProc-typedef:
.. aeEventFinalizerProc-typedef

02 aeEventFinalizerProc 定义
===============================================================================

.. code-block:: c 

    /* Types and data structures */
    typedef void aeEventFinalizerProc(struct aeEventLoop *eventLoop, void *clientData);

同 aeFileProc， 也是一个函数函数指针

.. _aeTimeProc-typedef:
.. aeTimeProc-typedef

03 aeTimeProc 定义
===============================================================================

.. code-block:: c 

    /* Types and data structures */
    typedef int aeTimeProc(struct aeEventLoop *eventLoop, long long id, void *clientData);

同 aeFileProc， 也是一个函数函数指针

.. _sds-typedef:
.. sds-typedef

04 sds 定义
===============================================================================

.. code-block:: c 

    typedef char *sds;

sds 就是一个 char 指针， sds 就是一个别名而已。 ``char*`` 表示字符指针类型， 当其指\
向一个字符串的第一个元素时， 它就可以表示这个字符串。

.. _`redisCommandProc-typedef`:
.. redisCommandProc-typedef

05 redisCommandProc 定义
===============================================================================

.. code-block:: c 

    typedef void redisCommandProc(redisClient *c);

定义了 redisCommandProc 类型。 


