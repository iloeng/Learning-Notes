###############################################################################
Redis Beta 1 源码阅读笔记 - Structures
###############################################################################

.. contents::

*******************************************************************************
Structures
*******************************************************************************

.. _redisServer-struct:
.. redisServer-struct

01 redisServer 结构体
===============================================================================

redisServer 结构体一共由 17 个子元素构成

.. code-block:: c

    /* Global server state structure */
    struct redisServer {
        int port;                     // 端口
        int fd;                       // 文件描述符
        dict **dict;                  // 哈希表
        long long dirty;              /* changes to DB from the last save */
        list *clients;                // 客户端列表 
        char neterr[ANET_ERR_LEN];    // 
        aeEventLoop *el;              // 事件循环
        int verbosity;                // 
        int cronloops;                // 
        int maxidletime;              // 超时时间
        int dbnum;                    // 数据库数量
        list *objfreelist;            /* A list of freed objects to avoid malloc() */
        int bgsaveinprogress;         //
        time_t lastsave;              // 最新保存时间
        struct saveparam *saveparams; //
        int saveparamslen;            //
        char *logfile;                // 日志文件
    };

.. _dict-struct:
.. dict-struct

02 dict 结构体
===============================================================================

.. code-block:: c 

    typedef struct dict {
        dictEntry **table;     // 哈希表结点指针数组
        dictType *type;        // 类型
        unsigned int size;     // 指针数组的大小
        unsigned int sizemask; // 长度掩码， 当使用下标访问数据时，确保下标不越界
        unsigned int used;     // 哈希表现有的节点数量
        void *privdata;        // 字典的私有数据
    } dict;

dict 类型是一个 dict 结构体。

sizemask 字段的作用是当使用下标访问数据时， 确保下标不越界。

例如当前 size 为 8 时， sizemask 为 7 (0x111)。 当给定一个下标N时， 将 N 与 \
sizemask 进行与操作后得出下标才是最终使用的下标， 这是一个绝对不会越界的下标。 

.. _dictEntry-struct:
.. dictEntry-struct

03 dictEntry 结构体
===============================================================================

.. code-block:: c 

    typedef struct dictEntry {
        void *key;              // 键
        void *val;              // 值
        struct dictEntry *next; // 下一个结点
    } dictEntry;

dictEntry 类型是一个 dictEntry 结构体。

dictEntry 就是 Dict (Hash Table) 的节点或条目， 每个条目都有 key， value 和下一个\
条目的地址

.. _dictType-struct:
.. dictType-struct

04 dictType 结构体
===============================================================================

.. code-block:: c

    typedef struct dictType {
        unsigned int (*hashFunction)(const void *key);
        void *(*keyDup)(void *privdata, const void *key);
        void *(*valDup)(void *privdata, const void *obj);
        int (*keyCompare)(void *privdata, const void *key1, const void *key2);
        void (*keyDestructor)(void *privdata, void *key);
        void (*valDestructor)(void *privdata, void *obj);
    } dictType;

dictType 结构包含若干函数指针， 用于 dict 的调用者对涉及 key 和 value 的各种操作进\
行自定义。 这些操作包含：

- hashFunction， 对 key 进行哈希值计算的哈希算法。
- keyDup 和 valDup， 分别定义 key 和 value 的拷贝函数， 用于在需要的时候对 key 和 \
  value 进行深拷贝， 而不仅仅是传递对象指针。
- keyCompare， 定义两个 key 的比较操作， 在根据 key 进行查找时会用到。
- keyDestructor 和 valDestructor， 分别定义对 key 和 value 的销毁函数。 私有数据\
  指针 （privdata） 就是在 dictType 的某些操作被调用时会传回给调用者。

.. _list-struct:
.. list-struct

05 list 结构体
===============================================================================

.. code-block:: c 

    typedef struct list {
        listNode *head; // 头节点
        listNode *tail; // 尾节点
        void *(*dup)(void *ptr);
        void (*free)(void *ptr);
        int (*match)(void *ptr, void *key);
        int len;
    } list;

list 是一个双向链表， 含有头节点和尾节点及链表的长度， 另外还有 3 个函数指针， 分别是 \
dup 、 free 和 match ：

- dup: 节点拷贝函数， 用于在需要的时候对节点进行深拷贝
- free: 节点释放函数
- match: 节点匹配函数

.. _listNode-struct:
.. listNode-struct

06 listNode 结构体
===============================================================================

.. code-block:: c 

    typedef struct listNode {
        struct listNode *prev; // 上一个节点地址
        struct listNode *next; // 下一个节点地址
        void *value;           // 当前结点的值的地址
    } listNode;

双向链表的节点， 含有 3 个元素， 分别是上一个节点地址， 下一个节点地址以及当前结点的\
值。 

.. _aeEventLoop-struct:
.. aeEventLoop-struct

07 aeEventLoop 结构体
===============================================================================

.. code-block:: c 

    /* State of an event based program */
    typedef struct aeEventLoop {
        long long timeEventNextId;
        aeFileEvent *fileEventHead;
        aeTimeEvent *timeEventHead;
        int stop;
    } aeEventLoop;

事件循环结构体

- ``timeEventNextId``: 用于生成时间事件的唯一标识 id
- ``fileEventHead``:  注册的事件链表头指针
- ``timeEventHead``: 注册的时间事件链表头指针
- ``stop``: 停止标志， 1 表示停止

.. _aeFileEvent-struct:
.. aeFileEvent-struct

08 aeFileEvent 结构体
===============================================================================

.. code-block:: c 

    /* File event structure */
    typedef struct aeFileEvent {
        int fd;
        int mask; /* one of AE_(READABLE|WRITABLE|EXCEPTION) */
        aeFileProc *fileProc;
        aeEventFinalizerProc *finalizerProc;
        void *clientData;
        struct aeFileEvent *next;
    } aeFileEvent;

aeFileEvent 文件事件结构体， 实际上是一个链表

- ``fd``: 文件描述符
- ``mask``: 标识这是一个读事件或写事件还是一个异常
- ``fileProc``: 事件处理函数
- ``finalizerProc``: 事件最后一次处理程序， 若设置则删除时间事件时调用
- ``clientData``: 传递给事件处理函数的数据
- ``next``: 下一个事件的地址

.. _aeTimeEvent-struct:
.. aeTimeEvent-struct

09 aeTimeEvent 结构体
===============================================================================

.. code-block:: c 

    /* Time event structure */
    typedef struct aeTimeEvent {
        long long id; /* time event identifier. */
        long when_sec; /* seconds */
        long when_ms; /* milliseconds */
        aeTimeProc *timeProc;
        aeEventFinalizerProc *finalizerProc;
        void *clientData;
        struct aeTimeEvent *next;
    } aeTimeEvent;

aeTimeEvent 时间事件结构体， 实际上也是一个链表

- ``id``: 时间事件标识 ID， 而且用于删除时间事件
- ``when_sec``: 秒
- ``when_ms``: 毫秒
- ``timeProc``: 时间事件处理函数
- ``finalizerProc``: 时间事件最后一次处理程序， 若设置则删除时间事件时调用
- ``clientData``: 传递给事件处理函数的数据
- ``next``: 下一个时间事件的地址

.. _saveparam-struct:
.. saveparam-struct

10 saveparam 结构体
===============================================================================

.. code-block:: c 

    struct saveparam {
        time_t seconds;  // 时间段
        int changes;     // 改变数量
    };

.. _sharedObjectsStruct-struct:
.. sharedObjectsStruct-struct

11 sharedObjectsStruct 结构体
===============================================================================

.. code-block:: c 

    struct sharedObjectsStruct {
        robj *crlf, *ok, *err, *zerobulk, *nil, *zero, *one, *pong;
    } shared;

为了操作方便， 同时为了节省内存， redis 定义了一组全局的共享对象 "shared"， 其中的 \
crlf 代表一个 "\r\n" 字符串对象， ok 代表一个 "ok" 字符串对象等。 

其中 robj 类型是 redisObject_ 结构体。

.. _redisObject: #redisObject-struct

.. _redisObject-struct:
.. redisObject-struct

12 redisObject 结构体
===============================================================================

.. code-block:: c 

    typedef struct redisObject {
        int type;
        void *ptr;
        int refcount;
    } robj;

redis 对象结构体， 包含了 3 个元素

- type: 对象类型
- ptr: 对象指针
- refcount: 对象引用计数

.. _sdshdr-struct:
.. sdshdr-struct

13 sdshdr 结构体
===============================================================================

.. code-block:: c 

    struct sdshdr {
        long len;
        long free;
        char buf[0];
    };

``sdshdr`` 全称是 Simple Dynamic Strings Header， 包含了 3 个元素：

- len: 记录 buf 数组中已使用字节的数量， 等于 sds 保存字符串的长度
- free: 记录 buf 数组中未使用字节的数量
- buf: 字节数组， 用于保存字符串

.. _`redisClient-struct`:
.. `redisClient-struct`

14 redisClient 结构体
===============================================================================

.. code-block:: c 

    /* With multiplexing we need to take per-clinet state.
    * Clients are taken in a liked list. */
    typedef struct redisClient {
        int fd;
        dict *dict;
        sds querybuf;
        sds argv[REDIS_MAX_ARGS];
        int argc;
        int bulklen;    /* bulk read len. -1 if not in bulk read mode */
        list *reply;
        int sentlen;
        time_t lastinteraction; /* time of the last interaction, used for timeout */
    } redisClient;

redisClient 结构体拥有 9 个字段：

- fd: 代表 client 的文件描述符
- dict: 字典指针
- querybuf: 查询缓存
- argv: 参数数组
- argc: 参数个数
- bulklen: 批量读取长度
- reply: 回应
- sentlen: 发送数据长度
- lastinteraction: 最近的交互时间， 用于 timeout

.. _`listIter-struct`:
.. `listIter-struct`

15 listIter 结构体
===============================================================================

.. code-block:: c 

    typedef struct listIter {
        listNode *next;
        listNode *prev;
        int direction;
    } listIter;

listIter 结构体拥有 3 个字段：

- next: 上一个链表节点
- prev: 下一个链表节点
- direction: 查询方向

.. _`dictIterator-struct`:
.. `dictIterator-struct`

16 dictIterator 结构体
===============================================================================

.. code-block:: c 

    typedef struct dictIterator {
        dict *ht;
        int index;
        dictEntry *entry, *nextEntry;
    } dictIterator;

dictIterator 结构体包含了 4 个字段：

- ht: 当前哈希表
- index: 索引值
- entry: 此次迭代的哈希表 Entry 
- nextEntry: 下一次迭代的哈希表 Entry 

.. _`redisCommand-struct`:
.. `redisCommand-struct`

17 redisCommand 结构体
===============================================================================

.. code-block:: c 

    struct redisCommand {
        char *name;
        redisCommandProc *proc;
        int arity;
        int type;
    };

redis 命令结构体， 包含了 4 个属性：

- name: redis 命令名称
- proc: 函数指针， 执行该命令时应该执行的函数
- arity: 
- type: 类型

