###############################################################################
Redis Beta 1 源码阅读笔记 - Others: 常量、 枚举等
###############################################################################

.. contents::

*******************************************************************************
Variables
*******************************************************************************

.. _sdsDictType-var:
.. sdsDictType-var

01 sdsDictType 常量
===============================================================================

.. code-block:: c 

    dictType sdsDictType = {
        sdsDictHashFunction,       /* hash function */
        NULL,                      /* key dup */
        NULL,                      /* val dup */
        sdsDictKeyCompare,         /* key compare */
        sdsDictKeyDestructor,      /* key destructor */
        sdsDictValDestructor,      /* val destructor */
    };

``sdsDictType`` 含有 6 个元素， 分别是：

- sdsDictHashFunction_: 哈希函数
- NULL1: key 复制函数， 为空
- NULL2: value 复制函数， 为空
- sdsDictKeyCompare_: key 对比函数
- sdsDictKeyDestructor_: 销毁 key
- sdsDictValDestructor_: 销毁 value 

.. _sdsDictHashFunction: beta-1-functions.rst#sdsDictHashFunction-func
.. _sdsDictKeyCompare: beta-1-functions.rst#sdsDictKeyCompare-func
.. _sdsDictKeyDestructor: beta-1-functions.rst#sdsDictKeyDestructor-func
.. _sdsDictValDestructor: beta-1-functions.rst#sdsDictValDestructor-func

