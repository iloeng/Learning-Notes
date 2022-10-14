###############################################################################
Redis Beta 1 源码阅读笔记 - Functions
###############################################################################

.. contents::

*******************************************************************************
Functions
*******************************************************************************

.. _ResetServerSaveParams-func:
.. ResetServerSaveParams-func

01 ResetServerSaveParams 函数
===============================================================================

.. code-block:: c

    static void ResetServerSaveParams() {
        free(server.saveparams);
        server.saveparams = NULL;
        server.saveparamslen = 0;
    }

static 关键字表示该函数只能在本文件中使用。 ``ResetServerSaveParams`` 函数的功能是\
清空 server 全局变量中的 ``saveparams`` 字段和 ``saveparamslen`` 字段。 

首先释放掉 ``server.saveparams`` 字段的内存， 然后将该字段置为 NULL， 同时将 \
``saveparamslen`` 置为 0， ``saveparamslen`` 顾名思义就是 ``server.saveparams`` \
的长度。

.. _appendServerSaveParams-func:
.. appendServerSaveParams-func

02 appendServerSaveParams 函数
===============================================================================

.. code-block:: c

    static void appendServerSaveParams(time_t seconds, int changes) {
        server.saveparams = realloc(server.saveparams,sizeof(struct saveparam)*(server.saveparamslen+1));
        if (server.saveparams == NULL) oom("appendServerSaveParams");
        server.saveparams[server.saveparamslen].seconds = seconds;
        server.saveparams[server.saveparamslen].changes = changes;
        server.saveparamslen++;
    }

该函数用于 redis 的持久化功能。 \

``server.saveparamslen`` 初始为 0， initServerConfig_ 函数中连续执行了 3 次 \
``appendServerSaveParams`` 函数， 注册了 3 次 redis 持久化检查任务， 分别是一小时内\
有 1 次改变、 5 分钟内有 100 次改变和 1 分钟内 10000 次改变， 满足这三个中的任意一个\
都将进行 redis 的持久化操作。 

.. _initServerConfig: beta-1-main-flow.rst#initServerConfig-func

``appendServerSaveParams`` 函数每次执行， 都会先分配内存， 内存大小是 saveparam_ \
结构体的大小乘以 saveparamslen 加一的结果， 然后将 saveparams 字段填充， 例如 \
``appendServerSaveParams(60*60,1);`` 步骤会将 3600 添加到 \
server.saveparams[0].seconds， 将 1 填到 server.saveparams[0].changes， 同时将 \
``server.saveparamslen`` 字段进行自增。

.. _`saveparam`: beta-1-structures.rst#saveparam-struct

这个函数会为后来的数据文件保存做铺垫。

.. _listCreate-func:
.. listCreate-func

03 listCreate 函数
===============================================================================

.. code-block:: c

    list *listCreate(void)
    {
        struct list *list;

        if ((list = malloc(sizeof(*list))) == NULL)
            return NULL;
        list->head = list->tail = NULL;
        list->len = 0;
        list->dup = NULL;
        list->free = NULL;
        list->match = NULL;
        return list;
    }

该函数用于新建一个空的双端链表， 分配好内存后， 将值置为 NULL， 长度置为 0， 最终返\
回这个新建的链表。

.. _createSharedObjects-func:
.. createSharedObjects-func

04 createSharedObjects 函数
===============================================================================

.. code-block:: c

    #define REDIS_STRING 0

    static void createSharedObjects(void) {
        shared.crlf = createObject(REDIS_STRING,sdsnew("\r\n"));
        shared.ok = createObject(REDIS_STRING,sdsnew("+OK\r\n"));
        shared.err = createObject(REDIS_STRING,sdsnew("-ERR\r\n"));
        shared.zerobulk = createObject(REDIS_STRING,sdsnew("0\r\n\r\n"));
        shared.nil = createObject(REDIS_STRING,sdsnew("nil\r\n"));
        shared.zero = createObject(REDIS_STRING,sdsnew("0\r\n"));
        shared.one = createObject(REDIS_STRING,sdsnew("1\r\n"));
        shared.pong = createObject(REDIS_STRING,sdsnew("+PONG\r\n"));
    }

这个函数主要是创建一些共享的全局对象， 我们平时在跟 redis 服务交互的时候， 如果有遇到\
错误， 会收到一些固定的错误信息或者字符串比如： -ERR syntax error， -ERR no such \
key。 这些字符串对象都是在这个函数里面进行初始化的。 

shared 全局变量是一个 sharedObjectsStruct_ 结构体。 

.. _sharedObjectsStruct: beta-1-structures.rst#sharedObjectsStruct-struct

``REDIS_STRING`` 常量被设置为 0， sdsnew_ 函数是字符串对象创建函数， 最终会返回字\
符串的地址

.. _sdsnew: #sdsnew-func

.. _createObject-func:
.. createObject-func

05 createObject 函数
===============================================================================

.. code-block:: c

    static robj *createObject(int type, void *ptr) {
        robj *o;

        if (listLength(server.objfreelist)) {
            listNode *head = listFirst(server.objfreelist);
            o = listNodeValue(head);
            listDelNode(server.objfreelist,head);
        } else {
            o = malloc(sizeof(*o));
        }
        if (!o) oom("createObject");
        o->type = type;
        o->ptr = ptr;
        o->refcount = 1;
        return o;
    }

在 createSharedObjects_ 函数中有使用到 createObject_ 函数， createObject_ 函数用\
于创建 redis 对象， 其参数有两个： ``type`` 为 redis 对象的类型； ``ptr`` 为 redis \
对象的地址指针。

.. _createSharedObjects: #createSharedObjects-func
.. _createObject: #createObject-func

listLength_ 宏定义的作用是返回 list_ 的 len 的值， 即链表的长度。

.. _listLength: beta-1-macros.rst#listLength-macro
.. _list: beta-1-structures.rst#list-struct

listFirst_ 宏定义的作用是返回 list_ 的 head 的值， 即链表的头节点的指针。

.. _listFirst: beta-1-macros.rst#listFirst-macro

listNodeValue_ 宏定义的作用是返回 listNode_ 的 value 的值， 即链表节点的值指针。

.. _listNode: beta-1-structures.rst#listNode-struct
.. _listNodeValue: beta-1-macros.rst#listNodeValue-macro

listDelNode_ 函数用于删除链表中指定的节点。 在此处就是删除链表的头节点， 因为释放的\
是头节点。

.. _listDelNode: #listDelNode-func

当 ``server`` 的 ``objfreelist`` 字段不为 0 时， 说明当前的 server 中有可以释放的 \
redis 对象， 那么直接从 ``objfreelist`` 链表中拿第一个对象作为新建的 redis 对象， \
否则就需要重新分配内存来新建 redis 对象。 此举是为了节省内存。 这就是第一个 if 语句的\
作用。 

最终将创建的 redis 对象地址返回。 

.. _listDelNode-func:
.. listDelNode-func

06 listDelNode 函数
===============================================================================

.. code-block:: c

    void listDelNode(list *list, listNode *node)
    {
        if (node->prev)
            node->prev->next = node->next;
        else
            list->head = node->next;
        if (node->next)
            node->next->prev = node->prev;
        else
            list->tail = node->prev;
        if (list->free) list->free(node->value);
        free(node);
        list->len--;
    }

删除节点函数有两个参数： ``list`` 是需要删除节点的链表； ``node`` 是被删的节点。

当当前节点 node 有前节点时， 说明不是链表的头节点， 删除节点时需要将前节点的 next 节\
点指向 node 的 next 节点， 略过自己； 否则的话说明 node 是头节点， 只需将头节点指向 \
node 的 next 节点。

当当前节点 node 有 next 节点时， 说明不是链表的尾节点， 删除节点时需要将 next 节点的 \
prev 节点指向当前节点 node 的 prev 节点， 也是要略过自己， 毕竟当前节点 node 是要删\
除的； 否则的话说明 node 是尾节点， 只需要将尾节点指向当前节点的 prev 节点。

如果 list 的 free 设置了某个函数， 将会对这个 node 执行该函数。

然后释放 node 的内存， 同时将 list 的 len 长度进行减 1。

.. _sdsnew-func:
.. sdsnew-func

07 sdsnew 函数
===============================================================================

.. code-block:: C 

    sds sdsnew(const char *init) {
        size_t initlen = (init == NULL) ? 0 : strlen(init);
        return sdsnewlen(init, initlen);
    }

sds_ 类型实际上是字符指针类型， redis 中实现了 sds_， 实际上可以看做 simple \
dynamic strings 简单动态字符串的缩写

.. _sds: beta-1-typedefs.rst#sds-typedef

当字符指针 (也可以看做是字符串) ``init`` 为 NULL 时， initlen 取 0， 否则取字符串 \
``init`` 的长度； 然后执行 sdsnewlen_ 函数创建一个给定长度的字符串。

.. _sdsnewlen: #sdsnewlen-func

.. _sdsnewlen-func:
.. sdsnewlen-func

08 sdsnewlen 函数
===============================================================================

.. code-block:: C 

    sds sdsnewlen(const void *init, size_t initlen) {
        struct sdshdr *sh;

        sh = malloc(sizeof(struct sdshdr)+initlen+1);
    #ifdef SDS_ABORT_ON_OOM
        if (sh == NULL) sdsOomAbort();
    #else
        if (sh == NULL) return NULL;
    #endif
        sh->len = initlen;
        sh->free = 0;
        if (initlen) {
            if (init) memcpy(sh->buf, init, initlen);
            else memset(sh->buf,0,initlen);
        }
        sh->buf[initlen] = '\0';
        return (char*)sh->buf;
    }

在这个函数中首先遇到了 sdshdr_ 结构体， 它的全称是 Simple Dynamic Strings Header。 \
这个结构体包含了字符串的长度、 剩余空间和字符串本身。

.. _sdshdr: beta-1-structures.rst#sdshar-struct

然后根据指定的字符串长度 ``initlen`` 分配内存大小， 首先是字符串头部大小 sdshdr 大\
小加上指定的长度 ``initlen``， 用于存放字符串， 而最后的 1 则表示字符串结束符 ``\0`` \
。 

如果定义了 ``SDS_ABORT_ON_OOM``， 当 ``sh`` 为 NULL 时， 执行 sdsOomAbort_ 函数， \
打印内存不足信息并中止程序执行， 直接从调用的地方跳出。 如果没有定义， 则直接返回 \
NULL。 

.. _sdsOomAbort: #sdsOomAbort-func

然后将字符串头部的 len 置为要创建的字符串的长度 initlen， 将 free 置为 0； 当 \
initlen 不为 0 时， 且字符串 init 不为空时， 将字符串 init 复制到 sh->buf 指向的地\
址中， 长度为 initlen， 如果字符串 init 为空， 则将字符 0 复制到 sh->buf 指向的地址\
中， 长度也是 initlen。 最后在向字符串结尾添加结束符 ``\0``。 

最终返回创建的字符串的地址。

.. _sdsOomAbort-func:
.. sdsOomAbort-func

09 sdsOomAbort 函数
===============================================================================

.. code-block:: C 

    static void sdsOomAbort(void) {
        fprintf(stderr,"SDS: Out Of Memory (SDS_ABORT_ON_OOM defined)\n");
        abort();
    }

执行这个函数的原因是内存不足了， 将错误信息向标准错误 stderr 传输， 同时中止程序执行。 

.. _aeCreateEventLoop-func:
.. aeCreateEventLoop-func

10 aeCreateEventLoop 函数
===============================================================================

.. code-block:: C 

    aeEventLoop *aeCreateEventLoop(void) {
        aeEventLoop *eventLoop;

        eventLoop = malloc(sizeof(*eventLoop));
        if (!eventLoop) return NULL;
        eventLoop->fileEventHead = NULL;
        eventLoop->timeEventHead = NULL;
        eventLoop->timeEventNextId = 0;
        eventLoop->stop = 0;
        return eventLoop;
    }

aeEventLoop_ 类型之前已经解析过了。

.. _aeEventLoop: beta-1-structures.rst#aeEventLoop-struct

先分配内存， 当 eventLoop 不为 NULL 时， 初始化 eventLoop 各个字段的值， 最终返回 \
eventLoop。 

.. _oom-func:
.. oom-func

11 oom 函数
===============================================================================

.. code-block:: C 

    static void oom(const char *msg) {
        fprintf(stderr, "%s: Out of memory\n",msg);
        fflush(stderr);
        sleep(1);
        abort();
    }

与之前的 sdsOomAbort_ 函数类似， 将内存不足的信息传输到 stderr 打印之后， 清除 \
stderr 缓存， 休息 1 秒钟后中止程序执行

.. _sdsOomAbort: #sdsOomAbort-func

.. _anetTcpServer-func:
.. anetTcpServer-func

12 anetTcpServer 函数
===============================================================================

.. code-block:: C 

    int anetTcpServer(char *err, int port, char *bindaddr)
    {
        int s, on = 1;
        struct sockaddr_in sa;
        
        // 1
        if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            anetSetError(err, "socket: %s\n", strerror(errno));
            return ANET_ERR;
        }

        // 2
        if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
            anetSetError(err, "setsockopt SO_REUSEADDR: %s\n", strerror(errno));
            close(s);
            return ANET_ERR;
        }
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
        
        // 3
        if (bindaddr) inet_aton(bindaddr, &sa.sin_addr);

        // 4
        if (bind(s, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
            anetSetError(err, "bind: %s\n", strerror(errno));
            close(s);
            return ANET_ERR;
        }

        // 5
        if (listen(s, 5) == -1) {
            anetSetError(err, "listen: %s\n", strerror(errno));
            close(s);
            return ANET_ERR;
        }
        return s;
    }

此函数的核心代码就是调用系统 socket 库的 ``listen`` 函数建立起了一个 TCP Server。 

此函数可以拆分成 5 个主要步骤：

#. ``socket`` 函数用于创建一个新的通信端 (socket)， 如果创建成功将返回一个新的文件\
   描述符， 否则返回 -1， 同时将错误代码写入 errno。 如果等于 -1， 说明创建失败， 然\
   后执行 anetSetError_ 函数并返回错误信息

#. ``setsockopt`` 函数用于操作文件描述符引用的 socket， 如果操作成功返回 0， 否则返\
   回 -1， 同时设置相应的 errno； 然后执行 anetSetError_ 函数， 关闭 socket 并返回\
   错误信息； 然后设置 socket 的相关信息， ``htons`` 用于将无符号的 short 整型主机\
   字节序转换为网络字节序； ``htonl`` 则用于将无符号的整型主机字节序转换为网络字节序。

#. 当指定了地址 ``bindaddr``， ``inet_aton`` 函数则会将 ``bindaddr`` 从数字与点构\
   成的 IPv4 转换为网络字节序的二进制数据， 并存储到 ``&sa.sin_addr``， 如果地址是\
   有效的则返回非零， 否则返回 0

#. 使用 ``bind`` 函数将 IP 地址与 socket 进行绑定； ``socket`` 函数创建套接字的时\
   候， 这个套接字就存在地址簇中了， 但是没有 IP 地址分配给它， ``bind`` 函数将指定\
   的地址分配给套接字， 如果执行成功返回 0， 否则返回 -1 并设置相应的 errno。

#. 这一步是核心步骤， ``listen`` 函数将文件描述符代表的套接字标记为一个被动的套接字， \
   可以使用 ``accept`` 函数接收进入的网络请求； 而那个 5 表示的是队列的长度为 5。 \
   执行成功返回 0， 失败返回 -1 同时设置相应的 errno。

#. 如果以上步骤都没有问题， 将返回这个可以正常接收数据的套接字文件描述符。

.. _anetSetError: #anetSetError-func

.. _anetSetError-func:
.. anetSetError-func

13 anetSetError 函数
===============================================================================

.. code-block:: C 

    #define ANET_ERR_LEN 256

    static void anetSetError(char *err, const char *fmt, ...)
    {
        va_list ap;

        if (!err) return;
        va_start(ap, fmt);
        vsnprintf(err, ANET_ERR_LEN, fmt, ap);
        va_end(ap);
    }

该函数使用了可变参数， ``void va_start(va_list ap, last);`` 从该函数的的声明可以看\
出: 最后一个确定参数是 last， 可变参数是从 last 开始的， 一直到最后， 一旦 va_end \
函数执行， ap 将变成 undefined 状态；  

.. code-block:: C 

    int vsnprintf(char *str, size_t size, const char *format, va_list ap);

格式化字符串， 最多写入 size 字节 (包含字符串结束符 "\\0") 到 str 中。

此函数中的 size 被设定为 ``ANET_ERR_LEN`` 也就是 256。

.. _redisLog-func:
.. redisLog-func

14 redisLog 函数
===============================================================================

.. code-block:: C 

    void redisLog(int level, const char *fmt, ...)
    {
        va_list ap;
        FILE *fp;

        fp = (server.logfile == NULL) ? stdout : fopen(server.logfile,"a");
        if (!fp) return;

        va_start(ap, fmt);
        if (level >= server.verbosity) {
            char *c = ".-*";
            fprintf(fp,"%c ",c[level]);
            vfprintf(fp, fmt, ap);
            fprintf(fp,"\n");
            fflush(fp);
        }
        va_end(ap);

        if (server.logfile) fclose(fp);
    }

redis 日志记录函数， 参数是可变参数， 有两个固定参数： 

#. level： 表示的是日志等级
#. fmt： 日志格式
#. 其他： 为可变参数

可变参数是从 fmt 开始的， 之后都是可变参数。 

首先判断 server.logfile 是否为 NULL， 若是将 fp 置为 stdout， 否则以追加的形式打\
开文件流， 然后判断文件流是否正常， 不正常直接返回空

当 level 大于或等于 ``server.verbosity``， 即 server 的信息复杂度， 也就是日志级\
别了， 在 initServerConfig_ 函数中被定义为 ``REDIS_DEBUG``

.. code-block:: c

    ...
    server.verbosity = REDIS_DEBUG;
    ...

    /* Log levels */
    #define REDIS_DEBUG 0
    #define REDIS_NOTICE 1
    #define REDIS_WARNING 2

因此函数中的 ``c[level]`` 为 ``.``

然后将可变参数以 fmt 格式写入到 fp 中， 最后换行。 函数的结尾判断是否有日志文件， 如\
果有， 还需要关闭 fp 文件流。

.. _dictCreate-func:
.. dictCreate-func

15 dictCreate 函数
===============================================================================

.. code-block:: C 

    /* Create a new hash table */
    dict *dictCreate(dictType *type, void *privDataPtr)
    {
        dict *ht = _dictAlloc(sizeof(*ht));

        _dictInit(ht,type,privDataPtr);
        return ht;
    }

该函数用于创建一个新的 dict 哈希表， type 是类型指针， privDataPtr 是私有数据指针。

首先先分配内存空间， 即执行 `_dictAlloc`_ 函数， 大小就是 dict_ 结构体的大小， 然后对\
这个对象进行初始化， 执行 `_dictInit`_ 函数。 

.. _dict: beta-1-structures.rst#dict-struct
.. _`_dictAlloc`: #_dictAlloc-func
.. _`_dictInit`: #_dictInit-func

最后返回这个新建的哈希表。 函数中的 ht 就是 hash table 的首字母缩写。

.. _`_dictAlloc-func`:
.. `_dictAlloc-func`

16 _dictAlloc 函数
===============================================================================

.. code-block:: C 

    static void *_dictAlloc(int size)
    {
        void *p = malloc(size);
        if (p == NULL)
            _dictPanic("Out of memory");
        return p;
    }

首先用 ``malloc`` 函数分配内存空间， 如果 p 为空， 则说明内存分配失败了， 因此会执行 \
`_dictPanic`_ 函数打印错误信息。 

.. _`_dictPanic`: #_dictPanic-func

如果内存分配成功， 直接返回分配的内存的地址。

.. _`_dictPanic-func`:
.. `_dictPanic-func`

17 _dictPanic 函数
===============================================================================

.. code-block:: C 

    static void _dictPanic(const char *fmt, ...)
    {
        va_list ap;

        va_start(ap, fmt);
        fprintf(stderr, "\nDICT LIBRARY PANIC: ");
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n\n");
        va_end(ap);
    }

该函数是一个可变参数函数， 有一个固定参数 fmt， 表示的是格式； 然后将 \
"\nDICT LIBRARY PANIC: " 字符串传输到标准错误输出 stderr， 然后对可变参数列表进行格\
式化输出， 最后换行。 总而言之就是用来打印 dict 模块错误信息的函数。

.. _`_dictInit-func`:
.. `_dictInit-func`

18 _dictInit 函数
===============================================================================

.. code-block:: C 

    #define DICT_OK 0

    /* Initialize the hash table */
    int _dictInit(dict *ht, dictType *type, void *privDataPtr)
    {
        _dictReset(ht);
        ht->type = type;
        ht->privdata = privDataPtr;
        return DICT_OK;
    }

初始化 dict 哈希表的函数拥有 3 个参数， 分别是需要初始化的哈希表 ht， 初始化的类型 \
type 以及私有数据 privDataPtr。 

首先会执行 `_dictReset`_ 函数将哈希表重置， 然后将重置后的哈希表 ht 的 type 字段设置\
为参数 type， privdata 字段设置为 privDataPtr 参数。 一切 OK 的话， 返回 DICT_OK， \
也就是 0。

.. _`_dictReset`: #_dictReset-func

.. _`_dictReset-func`:
.. `_dictReset-func`

19 _dictReset 函数
===============================================================================

.. code-block:: C 

    /* Reset an hashtable already initialized with ht_init().
    * NOTE: This function should only called by ht_destroy(). */
    static void _dictReset(dict *ht)
    {
        ht->table = NULL;
        ht->size = 0;
        ht->sizemask = 0;
        ht->used = 0;
    }

顾名思义， 重置哈希表， 但是根据代码注释， 这个方法只能被 ``ht_destroy`` 调用。

将 table 字段置为 NULL， 其他字段被置为 0。

.. _`aeCreateTimeEvent-func`:
.. `aeCreateTimeEvent-func`

20 aeCreateTimeEvent 函数
===============================================================================

.. code-block:: C 

    #define AE_ERR -1

    long long aeCreateTimeEvent(aeEventLoop *eventLoop, long long milliseconds,
            aeTimeProc *proc, void *clientData,
            aeEventFinalizerProc *finalizerProc)
    {
        long long id = eventLoop->timeEventNextId++;
        aeTimeEvent *te;

        te = malloc(sizeof(*te));
        if (te == NULL) return AE_ERR;
        te->id = id;
        aeAddMillisecondsToNow(milliseconds,&te->when_sec,&te->when_ms);
        te->timeProc = proc;
        te->finalizerProc = finalizerProc;
        te->clientData = clientData;
        te->next = eventLoop->timeEventHead;
        eventLoop->timeEventHead = te;
        return id;
    }

该函数用于创建定时器， 首先将当前事件循环的下一个定时器的 ID 自增加一存到 id 里面， \
te 是一个指向定时器 aeTimeEvent_ 的指针。

.. _aeTimeEvent: beta-1-structures.rst#aeTimeEvent-struct

然后对定时器分配内存， 并将内存地址赋值给 te， 如果 te 为 NULL， 说明内存分配失败了， \
直接返回 ``AE_ERR`` 即 -1。 

然后将 id 赋值个定时的 id 字段； 然后对当前定时器的时间进行操作， 实际上就是修改定时\
器的 when_sec 字段和 when_ms 字段， 这个过程执行的是 aeAddMillisecondsToNow_ 函数。 

.. _aeAddMillisecondsToNow: #aeAddMillisecondsToNow-func

然后设置定时器的处理函数， timeProc 字段被设置为参数 proc； finalizerProc 字段被设\
置为参数 finalizerProc； clientData 字段被设置为参数 clientData。

再然后这个新建的定时器的下一个定时器被设置为当前事件循环的定时器链表的头指针， 同时当\
前事件循环的定时器头指针被设置为这个新建的定时器。 实际上就是创建完就作为第一个监听的\
定时器。

最终将定时器的 id 返回。

.. _`aeAddMillisecondsToNow-func`:
.. `aeAddMillisecondsToNow-func`

21 aeAddMillisecondsToNow 函数
===============================================================================

.. code-block:: C 

    static void aeAddMillisecondsToNow(long long milliseconds, long *sec, long *ms) {
        long cur_sec, cur_ms, when_sec, when_ms;

        aeGetTime(&cur_sec, &cur_ms);
        when_sec = cur_sec + milliseconds/1000;
        when_ms = cur_ms + milliseconds%1000;
        if (when_ms >= 1000) {
            when_sec ++;
            when_ms -= 1000;
        }
        *sec = when_sec;
        *ms = when_ms;
    }

这个函数的功能很简单， 对时间进行换算， 当前的时间加上需要间隔的毫秒数， 最终返回超时\
时间， 也就是时间到了那个点， 就会执行一些操作。

aeGetTime_ 函数用于获取当前的秒和毫秒。

.. _aeGetTime: #aeGetTime-func

``milliseconds/1000`` 用于获取 milliseconds 包含有多少秒， 如果 milliseconds 大于\
或等于 1000， 则取整， 否则为 0。 然后用当前的毫秒加上上一步剩余的毫秒， 如果 when_ms \
大于等于 1000， 可以对秒进行加一， 同时将毫秒减去 1000， 最终将计算后的秒和毫秒赋值给\
参数 sec 和参数 ms。

.. _`aeGetTime-func`:
.. `aeGetTime-func`

22 aeGetTime 函数
===============================================================================

.. code-block:: C 

    static void aeGetTime(long *seconds, long *milliseconds)
    {
        struct timeval tv;

        gettimeofday(&tv, NULL);
        *seconds = tv.tv_sec;
        *milliseconds = tv.tv_usec/1000;
    }

该函数调用 gettimeofday 函数获取当前的时间， tv_sec 表示的是秒， tv_usec 表示的是微\
秒， 因此将其除以 1000 转换为毫秒。

.. _`serverCron-func`:
.. `serverCron-func`

23 serverCron 函数
===============================================================================

.. code-block:: C 

    #define REDIS_DEBUG 0
    #define REDIS_NOTICE 1
    #define REDIS_WARNING 2

    /* Hash table parameters */
    #define REDIS_HT_MINFILL        10      /* Minimal hash table fill 10% */
    #define REDIS_HT_MINSLOTS       16384   /* Never resize the HT under this */

    int serverCron(struct aeEventLoop *eventLoop, long long id, void *clientData) {
        // 1
        int j, size, used, loops = server.cronloops++;
        REDIS_NOTUSED(eventLoop);
        REDIS_NOTUSED(id);
        REDIS_NOTUSED(clientData);

        // 2
        /* If the percentage of used slots in the HT reaches REDIS_HT_MINFILL
        * we resize the hash table to save memory */
        for (j = 0; j < server.dbnum; j++) {
            size = dictGetHashTableSize(server.dict[j]);
            used = dictGetHashTableUsed(server.dict[j]);
            if (!(loops % 5) && used > 0) {
                redisLog(REDIS_DEBUG,"DB %d: %d keys in %d slots HT.",j,used,size);
                // dictPrintStats(server.dict);
            }
            if (size && used && size > REDIS_HT_MINSLOTS &&
                (used*100/size < REDIS_HT_MINFILL)) {
                redisLog(REDIS_NOTICE,"The hash table %d is too sparse, resize it...",j);
                dictResize(server.dict[j]);
                redisLog(REDIS_NOTICE,"Hash table %d resized.",j);
            }
        }

        // 3
        /* Show information about connected clients */
        if (!(loops % 5)) redisLog(REDIS_DEBUG,"%d clients connected",listLength(server.clients));

        // 4
        /* Close connections of timedout clients */
        if (!(loops % 10))
            closeTimedoutClients();

        // 5
        /* Check if a background saving in progress terminated */
        if (server.bgsaveinprogress) {
            int statloc;
            if (wait4(-1,&statloc,WNOHANG,NULL)) {
                int exitcode = WEXITSTATUS(statloc);
                if (exitcode == 0) {
                    redisLog(REDIS_NOTICE,
                        "Background saving terminated with success");
                    server.dirty = 0;
                    server.lastsave = time(NULL);
                } else {
                    redisLog(REDIS_WARNING,
                        "Background saving error");
                }
                server.bgsaveinprogress = 0;
            }
        } else {
            /* If there is not a background saving in progress check if
            * we have to save now */
            time_t now = time(NULL);
            for (j = 0; j < server.saveparamslen; j++) {
                struct saveparam *sp = server.saveparams+j;

                if (server.dirty >= sp->changes &&
                    now-server.lastsave > sp->seconds) {
                    redisLog(REDIS_NOTICE,"%d changes in %d seconds. Saving...",
                        sp->changes, sp->seconds);
                    saveDbBackground("dump.rdb");
                    break;
                }
            }
        }
        return 1000;
    }

server 的 cronloops 字端根据我目前的理解应该是自动检测循环的次数， 初始的时候为 0。 \
将这个大函数根据注释分成 6 部分。

#. 新建局部变量 j， size， used 和 loops， 其中 loops 被初始化为 server.cronloops \
   + 1； 同时将三个参数 eventLoop， id 和 clientData 的类型强制转换为 void， 因为\
   在这个函数中， 这三个参数并没有使用。
#. 当哈希表中已经使用的空间达到 redis 哈希表最小填充， 即 REDIS_HT_MINFILL， 重新设\
   置哈希表的尺寸以达到节省内存的目的。 首先会用 dictGetHashTableSize_ 宏和 \
   dictGetHashTableUsed_ 宏来获取哈希表的大小以及以及使用的大小； 然后每 5 次定时检\
   测记录一次日志， 因为 ``loops % 5`` 只有在 loops 为 5 的整数倍的时候， 这个表达式\
   才能为 0， 才会执行第一个 if 语句中的 redisLog_ 函数； 然后当 ``size``， \
   ``used``， ``size > REDIS_HT_MINSLOTS`` 和 \
   ``(used*100/size < REDIS_HT_MINFILL)`` 都为真值的时候， 也就是当哈希表的大小大\
   于 16384， 且已使用的比率小于 10% 时， 就需要执行 if 内部的缩小哈希表大小的操作， \
   因为哈希表的大小比较大， 但是使用率低， 因此缩小以节省内存， 重置哈希表大小的函数是 \
   dictResize_
#. 每 5 次定时检测记录一次有多少个 client 在连接着 server， 这个数量是通过 \
   listLength_ 宏定义获取 server.clients 的长度拿到的。
#. 每 10 次检测， 断开连接超时的 clients， 执行的函数是 closeTimedoutClients_
#. 然后检测 redis 是否有后台进程用于持久化数据， 也就是保存数据。 当 \
   server.bgsaveinprogress 为真值非 0 时会执行 if 语句的内容， 否则执行 else 的内\
   容。 当为真值时， 说明有后台进程在进行数据的保存， 因此会执行 wait4 函数等待说有的\
   子进程， wait4 函数的第一个参数 -1 表示等待的是所有的子进程； 第二个参数 &statloc \
   表示的是存储的等待结果， 第 3 个参数 WNOHANG 表示非阻塞， 如果没有子进程退出就立刻\
   返回结果。 然后宏 WEXITSTATUS(statloc) 将等待的结果转换为 exitcode， 当 \
   exitcode 为 0 时记录 REDIS_NOTICE 级别的日志， 同时将 server.dirty 置为 0， \
   server.lastsave 置为当前时间； 否则的话记录 REDIS_WARNING 级别日志， 信息是后台\
   保存错误最终将 server.bgsaveinprogress 置为 0。 当没有后台保存进程的时候， 就需要\
   检测是否需要保存， 先获取当前时间， 然后判断修改的数量是否大于等于设定的数量， 同时\
   上次保存成功的时间与当前时间的间隔是否大于或等于设定的时间间隔， 如果是就记录日志， \
   同时执行 saveDbBackground_ 函数生成备份数据， 文件名为 dump.rdb
#. 如果一切 OK， 则该函数返回 1000。

..

  wait3 等待所有的子进程； wait4 可以像 waitpid 一样指定要等待的子进程： pid>0 表示\
  子进程ID； pid=0 表示当前进程组中的子进程； pid=-1 表示等待所有子进程； pid<-1 表\
  示进程组ID为pid绝对值的子进程。

.. _dictGetHashTableSize: beta-1-macros.rst#dictGetHashTableSize-macro
.. _dictGetHashTableUsed: beta-1-macros.rst#dictGetHashTableUsed-macro
.. _redisLog: beta-1-functions.rst#redisLog-func
.. _dictResize: beta-1-functions.rst#dictResize-func
.. _closeTimedoutClients: beta-1-functions.rst#closeTimedoutClients-func
.. _saveDbBackground: beta-1-functions.rst#saveDbBackground-func

.. _`dictResize-func`:
.. `dictResize-func`

24 dictResize 函数
===============================================================================

.. code-block:: C 

    /* This is the initial size of every hash table */
    #define DICT_HT_INITIAL_SIZE     16
    
    int dictResize(dict *ht)
    {
        int minimal = ht->used;

        if (minimal < DICT_HT_INITIAL_SIZE)
            minimal = DICT_HT_INITIAL_SIZE;
        return dictExpand(ht, minimal);
    }

重置字典哈希表的最小 size， 使其最小能容纳所有的节点， 且满足不等式 used/buckets 接\
近 <= 1。 

``DICT_HT_INITIAL_SIZE`` 为默认的哈希表大小， 其值为 16， 当已经使用的大小小于 16 \
的时候， 将 minimal 最小值设为 16， 否则就是哈希表已经使用的大小， 然后使用 \
dictExpand_ 函数进行字典大小的修改。

.. _dictExpand: #dictExpand-func

.. _`dictExpand-func`:
.. `dictExpand-func`

25 dictExpand 函数
===============================================================================

.. code-block:: C 

    /* Expand or create the hashtable */
    int dictExpand(dict *ht, unsigned int size)
    {
        // 1
        dict n; /* the new hashtable */
        unsigned int realsize = _dictNextPower(size), i;

        /* the size is invalid if it is smaller than the number of
        * elements already inside the hashtable */
        if (ht->used > size)
            return DICT_ERR;

        // 2
        _dictInit(&n, ht->type, ht->privdata);
        n.size = realsize;
        n.sizemask = realsize-1;
        n.table = _dictAlloc(realsize*sizeof(dictEntry*));

        // 3
        /* Initialize all the pointers to NULL */
        memset(n.table, 0, realsize*sizeof(dictEntry*));

        // 4
        /* Copy all the elements from the old to the new table:
        * note that if the old hash table is empty ht->size is zero,
        * so dictExpand just creates an hash table. */
        n.used = ht->used;
        for (i = 0; i < ht->size && ht->used > 0; i++) {
            dictEntry *he, *nextHe;

            if (ht->table[i] == NULL) continue;
            
            /* For each hash entry on this slot... */
            he = ht->table[i];
            while(he) {
                unsigned int h;

                nextHe = he->next;
                /* Get the new element index */
                h = dictHashKey(ht, he->key) & n.sizemask;
                he->next = n.table[h];
                n.table[h] = he;
                ht->used--;
                /* Pass to the next element */
                he = nextHe;
            }
        }

        // 5
        assert(ht->used == 0);
        _dictFree(ht->table);

        // 6
        /* Remap the new hashtable in the old */
        *ht = n;
        return DICT_OK;
    }

该函数用于扩展或创建哈希表。 按照代码注释， 大致分成 6 部分解析。

#. realsize 是 `_dictNextPower`_ 函数结果， 用于判断当前的 size 是否是在 2 的某一\
   次方内， 如果不在就将乘以 2； 然后判断哈希表已使用的大小是否大于哈希表的大小， 若是\
   返回 ``DICT_ERR`` 即 1
#. 对哈希表 n 进行初始化， 然后将哈希表的 size 置为 realsize， 同时 sizemask 置为 \
   realsize-1， table 置为哈希表分配 dictEntry 内存的地址
#. 将指向 n.table 的内存全部写成 0
#. 当旧的哈希表的大小不为 0 且有使用的大小时， 循环迭代复制每一个元素到新的哈希表中， \
   需要注意的是， 之前在 initServer_ 函数中使用的 sdsDictType_ 进行的初始化 dict 操\
   作， 因此在 dictHashKey_ 宏中使用的是 hash 函数是 sdsDictHashFunction_， 在此处\
   使用 ``dictHashKey(ht, he->key) & n.sizemask`` 是为了防止数组越界， 因为 \
   sizemask 一直比 size 小 1。 复制完成后将旧的 hash 表已使用大小减 1。 
#. 判断就的 hash 表已使用大小是否为 0， 为 0 说明复制完毕， 因为在复制的时候复制一个\
   就减 1。 然后在将旧的 hash 表使用 `_dictFree`_ 函数释放
#. 然后将旧的 hash 表的指针指向新的拓展后的 hash 表。 之前步骤一切 OK 后， 返回 \
   DICT_OK 即 0

.. _`_dictNextPower`: #_dictNextPower-func
.. _`initServer`: beta-1-main-flow.rst#initServer-func
.. _`sdsDictType`: beta-1-others.rst#sdsDictType-var
.. _`dictHashKey`: beta-1-macros.rst#dictHashKey-macro
.. _`sdsDictHashFunction`: #sdsDictHashFunction-func

.. _`_dictNextPower-func`:
.. `_dictNextPower-func`

26 _dictNextPower 函数
===============================================================================

.. code-block:: C 

    /* Our hash table capability is a power of two */
    static unsigned int _dictNextPower(unsigned int size)
    {
        unsigned int i = DICT_HT_INITIAL_SIZE;

        if (size >= 2147483648U)
            return 2147483648U;
        while(1) {
            if (i >= size)
                return i;
            i *= 2;
        }
    }

redis 中的哈希表的容量都是 2 的整数次幂， 同时初始化的容量是 DICT_HT_INITIAL_SIZE \
即 16。

该函数用于判断一个 hash 表的大小是否应该放大乘以 2。 

- 当传入的参数大小大于等于 2147483648U， 直接返回 2147483648U
- 当哈希表的大小小于或等于初始容量， 返回初始容量表明无须扩大， 否则将 i 乘以 2 继续\
  判断。 直到 i 的值大于等于 hash 表的值， 并返回这个值

.. _`sdsDictHashFunction-func`:
.. `sdsDictHashFunction-func`

27 sdsDictHashFunction 函数
===============================================================================

.. code-block:: C 

    static unsigned int sdsDictHashFunction(const void *key) {
        return dictGenHashFunction(key, sdslen((sds)key));
    }

sdsDictType 类型的 hash 函数就是该函数

在该函数中执行 dictGenHashFunction_ 函数对 key 进行 hash 运算， 最终返回函数值

.. _dictGenHashFunction: #dictGenHashFunction-func

.. _`dictGenHashFunction-func`:
.. `dictGenHashFunction-func`

28 dictGenHashFunction 函数
===============================================================================

.. code-block:: C 

    /* Generic hash function (a popular one from Bernstein).
    * I tested a few and this was the best. */
    unsigned int dictGenHashFunction(const unsigned char *buf, int len) {
        unsigned int hash = 5381;

        while (len--)
            hash = ((hash << 5) + hash) + (*buf++); /* hash * 33 + c */
        return hash;
    }

传入的参数 len 有多少就执行多少次 hash 运算， 最终将运算结果返回。

.. _`closeTimedoutClients-func`:
.. `closeTimedoutClients-func`

29 closeTimedoutClients 函数
===============================================================================

.. code-block:: C 

    /* Directions for iterators */
    #define AL_START_HEAD 0
    #define AL_START_TAIL 1

    void closeTimedoutClients(void) {
        redisClient *c;
        listIter *li;
        listNode *ln;
        time_t now = time(NULL);

        li = listGetIterator(server.clients,AL_START_HEAD);
        if (!li) return;
        while ((ln = listNextElement(li)) != NULL) {
            c = listNodeValue(ln);
            if (now - c->lastinteraction > server.maxidletime) {
                redisLog(REDIS_DEBUG,"Closing idle client");
                freeClient(c);
            }
        }
        listReleaseIterator(li);
    }

此处需要先了解一下 redisClient_ 结构体和 listIter_ 结构体。

.. _redisClient: beta-1-structures.rst#redisClient-struct
.. _listIter: beta-1-structures.rst#listIter-struct

先获取当前的时间， 然后使用 listGetIterator_ 函数生成一个访问 List 的迭代器， 其中包\
含了访问方向。 代码中使用的是 AL_START_HEAD 即 0， 表示的是从 List 头节点开始访问。

.. _listGetIterator: #listGetIterator-func

当访问迭代器为空时， 直接返回。 正常时继续向下执行， 然后使用 listNextElement_ 获取下\
一个节点， 节点不为空时， 执行 listNodeValue_ 宏获取结点值。 当现在的时候与上次交互的\
时间间隔大于 server.maxidletime 时， 即大于超时时间， 就记录关闭 client 连接的日志， \
同时使用 freeClient_ 函数释放 client 连接。 

.. _listNextElement: #listNextElement-func
.. _listNodeValue: beta-1-macros.rst#listNodeValue-macro
.. _freeClient: #freeClient-func

最终使用 listReleaseIterator_ 函数释放 List 访问迭代器。

.. _listReleaseIterator: #listReleaseIterator-func

.. _`listGetIterator-func`:
.. `listGetIterator-func`

30 listGetIterator 函数
===============================================================================

.. code-block:: C 

    listIter *listGetIterator(list *list, int direction)
    {
        listIter *iter;
        
        if ((iter = malloc(sizeof(*iter))) == NULL) return NULL;
        if (direction == AL_START_HEAD)
            iter->next = list->head;
        else
            iter->next = list->tail;
        iter->direction = direction;
        return iter;
    }

从给定的 List 和 direction 生成一个 List 访问迭代器。 

如果分配迭代器内存失败， 直接返回 NULL。 当 direction 为 AL_START_HEAD 时， 表明是\
从头节点开始访问， 那么将迭代器 next 字段置为当前 List 的头节点； 否则就是从尾节点开\
始访问， 将 next 字段置为 List 的尾节点； 然后将其方向 direction 字段置为给定的 \
direction， 最终返回这个迭代器。

.. _`listNextElement-func`:
.. `listNextElement-func`

31 listNextElement 函数
===============================================================================

.. code-block:: C 

    listNode *listNextElement(listIter *iter)
    {
        listNode *current = iter->next;

        if (current != NULL) {
            if (iter->direction == AL_START_HEAD)
                iter->next = current->next;
            else
                iter->next = current->prev;
        }
        return current;
    }
    
声明 current 为当前节点， 其值为 List 访问迭代器的 next 指针， 如果 current 非空， \
当 iter 方向为从头节点开始时， 那么 iter->next 就是当前节点的 next 节点， 即 iter->\
next->next， 相当于 iter 向后移动了一个单位。 否则就向前移动。

最终返回 current 节点。 

.. _`freeClient-func`:
.. `freeClient-func`

32 freeClient 函数
===============================================================================

.. code-block:: C 

    #define AE_READABLE 1
    #define AE_WRITABLE 2
    #define AE_EXCEPTION 4

    static void freeClient(redisClient *c) {
        listNode *ln;

        aeDeleteFileEvent(server.el,c->fd,AE_READABLE);
        aeDeleteFileEvent(server.el,c->fd,AE_WRITABLE);
        sdsfree(c->querybuf);
        listRelease(c->reply);
        freeClientArgv(c);
        close(c->fd);
        ln = listSearchKey(server.clients,c);
        assert(ln != NULL);
        listDelNode(server.clients,ln);
        free(c);
    }

释放 client 连接， 需要进行一系列的操作：

#. aeDeleteFileEvent(server.el,c->fd,AE_READABLE); aeDeleteFileEvent_ 函数删除 \
   IO 读
#. aeDeleteFileEvent(server.el,c->fd,AE_WRITABLE); aeDeleteFileEvent_ 函数删除 \
   IO 写
#. sdsfree_ 函数释放 client 查询缓冲区 
#. listRelease_ 函数释放 client reply 
#. freeClientArgv_ 函数释放 client 参数
#. close 关闭 client 连接
#. listSearchKey_ 从 server.clients 中搜索要释放的 client
#. 断言搜索结果是否为空， 为空说明 clients 列表中没有要释放的 client 
#. 正常情况下 ln 是不为空的， 使用 listDelNode_ 从 server.clients 将 client 删除
#. 最后释放 client 占用的内存

.. _`aeDeleteFileEvent`: #aeDeleteFileEvent-func
.. _`sdsfree`: #sdsfree-func
.. _`listRelease`: #listRelease-func
.. _`freeClientArgv`: #freeClientArgv-func
.. _`listSearchKey`: #listSearchKey-func
.. _`listDelNode`: #listDelNode-func

.. _`aeDeleteFileEvent-func`:
.. `aeDeleteFileEvent-func`

33 aeDeleteFileEvent 函数
===============================================================================

.. code-block:: C 

    void aeDeleteFileEvent(aeEventLoop *eventLoop, int fd, int mask)
    {
        aeFileEvent *fe, *prev = NULL;

        fe = eventLoop->fileEventHead;
        while(fe) {
            if (fe->fd == fd && fe->mask == mask) {
                if (prev == NULL)
                    eventLoop->fileEventHead = fe->next;
                else
                    prev->next = fe->next;
                if (fe->finalizerProc)
                    fe->finalizerProc(eventLoop, fe->clientData);
                free(fe);
                return;
            }
            prev = fe;
            fe = fe->next;
        }
    }

局部变量 fe 指的是当前 FileEvent， prev 指的是上一个 FileEvent。 

然后从第一个 FileEvent， 即 ``fe = eventLoop->fileEventHead`` 开始循环判断， 当当\
前 FileEvent 的 fd 与传递的 fd 相等且当前的 mask 与传递的 mask 相等时， 开始执行删除\
操作：

- 当 prev 为空， 说明是第一个 FileEvent， 那么直接将 fileEventHead 指向当前 \
  FileEvent 的 next； 否则就不是第一个 FileEvent， 直接将当前 FileEvent 的前一个的\
  next 指向当前 FileEvent 的 next， 直接略过当前 FileEvent， 表明删除
- 当当前 FileEvent 的 finalizerProc 指针有值时， 那么执行这个函数。 finalizerProc \
  是一个指向函数的指针。
- 删除后将当前 FileEvent 占用的内存释放， 并返回

如果不满足 if 条件， 则开始进行下一轮判断， 直到 fe 为空。

.. _`sdsfree-func`:
.. `sdsfree-func`

34 sdsfree 函数
===============================================================================

.. code-block:: C 

    void sdsfree(sds s) {
        if (s == NULL) return;
        free(s-sizeof(struct sdshdr));
    }

释放字符串对象内存。 当字符串 s 为空时直接返回； 否则将 sds 的对象释放掉。

``s-sizeof(struct sdshdr)`` 此处的意思是字符串和 sdshdr 整体。

.. code-block::

    |5|0|redis|
    ^   ^
    sh  sh->buf

sizeof(struct sdshdr) 实际上只是 len 和 free 字段的长度， buf 字段是不确定长度， 因\
此在 sizeof 计算时并没有包含在内。 那么 s 就是 buf 所在的指针， 因此此处 free 的时候\
就是连同 sdshdr 一起释放。

.. _`listRelease-func`:
.. `listRelease-func`

35 listRelease 函数
===============================================================================

.. code-block:: C 

    void listRelease(list *list)
    {
        int len;
        listNode *current, *next;

        current = list->head;
        len = list->len;
        while(len--) {
            next = current->next;
            if (list->free) list->free(current->value);
            free(current);
            current = next;
        }
        free(list);
    }

该函数用于释放整个 List， 会从第一个节点开始释放内存， 直到整个 list 完全释放。

current 从头节点开始， 如果指定了 ``list->free``， 那么就执行该函数释放当前结点的值。 \
否则直接释放当前结点， 同时将当前结点指向下一个节点。

最终释放 list 的内存。

.. _`freeClientArgv-func`:
.. `freeClientArgv-func`

36 freeClientArgv 函数
===============================================================================

.. code-block:: C 

    static void freeClientArgv(redisClient *c) {
        int j;

        for (j = 0; j < c->argc; j++)
            sdsfree(c->argv[j]);
        c->argc = 0;
    }

在 redisClient_ 结构体中， argv 字段是字符串数组， 因此在该函数中通过 for 循环的方式\
使用 sdsfree_ 函数逐个释放掉每个 argv ， argc 就是 argv 的数量， 因此释放完毕后， \
argc 被置为 0。

.. _redisClient: beta-1-structures.rst#redisClient-struct
.. _sdsfree: #sdsfree-func

.. _`listSearchKey-func`:
.. `listSearchKey-func`

37 listSearchKey 函数
===============================================================================

.. code-block:: C 

    // todo
    listNode *listSearchKey(list *list, void *key)
    {
        listIter *iter;
        listNode *node;

        iter = listGetIterator(list, AL_START_HEAD);
        while((node = listNextElement(iter)) != NULL) {
            if (list->match) {
                if (list->match(node->value, key)) {
                    listReleaseIterator(iter);
                    return node;
                }
            } else {
                if (key == node->value) {
                    listReleaseIterator(iter);
                    return node;
                }
            }
        }
        listReleaseIterator(iter);
        return NULL;
    }

该函数用于在 list 中搜索 key， 如果搜索到返回这个节点， 否则返回 NULL。

iter 是 list 访问迭代器， 它是从 list 的头节点开始的； node 就是 list 节点。

当 ``list->match`` 指针有值时， 如果 ``list->match(node->value, key)`` 直接使用 \
listReleaseIterator_ 释放 iter 同时返回节点 node； 否则当 ``key == node->value`` \
时释放 iter 同时返回 node。

.. _listReleaseIterator: #listReleaseIterator-func

如果 ``listNextElement(iter)`` 为 NULL， 直接使用 listReleaseIterator 释放 iter \
并返回 NULL。

.. _`listReleaseIterator-func`:
.. `listReleaseIterator-func`

38 listReleaseIterator 函数
===============================================================================

.. code-block:: C 

    void listReleaseIterator(listIter *iter) {
        free(iter);
    }

该函数直接调用 free 函数释放 listIter 结构体的内存。

.. _`saveDbBackground-func`:
.. `saveDbBackground-func`

39 saveDbBackground 函数
===============================================================================

.. code-block:: C 

    /* Error codes */
    #define REDIS_OK                0
    #define REDIS_ERR               -1

    static int saveDbBackground(char *filename) {
        pid_t childpid;

        if (server.bgsaveinprogress) return REDIS_ERR;
        if ((childpid = fork()) == 0) {
            /* Child */
            close(server.fd);
            if (saveDb(filename) == REDIS_OK) {
                exit(0);
            } else {
                exit(1);
            }
        } else {
            /* Parent */
            redisLog(REDIS_NOTICE,"Background saving started by pid %d",childpid);
            server.bgsaveinprogress = 1;
            return REDIS_OK;
        }
        return REDIS_OK; /* unreached */
    }

后台备份 redis 数据， childpid 就是子进程。 server.bgsaveinprogress 表示的是是否有\
进程在进行数据备份。 在 serverCron_ 函数中已经将 server.bgsaveinprogress 置为 0 了。

.. _serverCron: #serverCron-func

childpid 被用于存放 fork 函数值。 当成功执行 fork 函数的时候， 在子进程中返回的是 0， \
父进程中返回的是进程 ID， 因此在在子进程中进行 saveDb_ 操作， 成功保存后使用 exit(0) \
退出进程， 否则使用 exit(1) 退出进程； 与此同时父进程中打印日志， 将 \
server.bgsaveinprogress 置为 1 并返回 REDIS_OK 即 0。 

.. _saveDb: #saveDb-func

最后的返回 0 是不会执行到这一步的。

.. _`saveDb-func`:
.. `saveDb-func`

40 saveDb 函数
===============================================================================

.. code-block:: C 

    #define REDIS_SELECTDB 254
    #define REDIS_STRING 0
    #define REDIS_LIST 1
    #define REDIS_EOF 255

    static int saveDb(char *filename) {
        dictIterator *di = NULL;
        dictEntry *de;
        uint32_t len;
        uint8_t type;
        FILE *fp;
        char tmpfile[256];
        int j;

        // 1
        snprintf(tmpfile,256,"temp-%d.%ld.rdb",(int)time(NULL),(long int)random());
        
        // 2
        fp = fopen(tmpfile,"w");
        if (!fp) {
            redisLog(REDIS_WARNING, "Failed saving the DB: %s", strerror(errno));
            return REDIS_ERR;
        }

        // 3
        if (fwrite("REDIS0000",9,1,fp) == 0) goto werr;
        
        // 4
        for (j = 0; j < server.dbnum; j++) {
            // 1
            dict *dict = server.dict[j];
            if (dictGetHashTableUsed(dict) == 0) continue;
            di = dictGetIterator(dict);
            if (!di) {
                fclose(fp);
                return REDIS_ERR;
            }

            // 2
            /* Write the SELECT DB opcode */
            type = REDIS_SELECTDB;
            len = htonl(j);
            if (fwrite(&type,1,1,fp) == 0) goto werr;
            if (fwrite(&len,4,1,fp) == 0) goto werr;

            // 3
            /* Iterate this DB writing every entry */
            while((de = dictNext(di)) != NULL) {
                // 4
                sds key = dictGetEntryKey(de);
                robj *o = dictGetEntryVal(de);

                // 5
                type = o->type;
                len = htonl(sdslen(key));
                if (fwrite(&type,1,1,fp) == 0) goto werr;
                if (fwrite(&len,4,1,fp) == 0) goto werr;
                if (fwrite(key,sdslen(key),1,fp) == 0) goto werr;

                // 6
                if (type == REDIS_STRING) {
                    /* Save a string value */
                    sds sval = o->ptr;
                    len = htonl(sdslen(sval));
                    if (fwrite(&len,4,1,fp) == 0) goto werr;
                    if (fwrite(sval,sdslen(sval),1,fp) == 0) goto werr;
                } else if (type == REDIS_LIST) {
                    /* Save a list value */
                    list *list = o->ptr;
                    listNode *ln = list->head;

                    len = htonl(listLength(list));
                    if (fwrite(&len,4,1,fp) == 0) goto werr;
                    while(ln) {
                        robj *eleobj = listNodeValue(ln);
                        len = htonl(sdslen(eleobj->ptr));
                        if (fwrite(&len,4,1,fp) == 0) goto werr;
                        if (fwrite(eleobj->ptr,sdslen(eleobj->ptr),1,fp) == 0)
                            goto werr;
                        ln = ln->next;
                    }
                } else {
                    assert(0 != 0);
                }
            }
            // 7
            dictReleaseIterator(di);
        }

        // 5
        /* EOF opcode */
        type = REDIS_EOF;
        if (fwrite(&type,1,1,fp) == 0) goto werr;
        fclose(fp);
        
        // 6
        /* Use RENAME to make sure the DB file is changed atomically only
        * if the generate DB file is ok. */
        if (rename(tmpfile,filename) == -1) {
            redisLog(REDIS_WARNING,"Error moving temp DB file on the final destionation: %s", strerror(errno));
            unlink(tmpfile);
            return REDIS_ERR;
        }

        // 7
        redisLog(REDIS_NOTICE,"DB saved on disk");
        server.dirty = 0;
        server.lastsave = time(NULL);
        return REDIS_OK;

        // 8
    werr:
        fclose(fp);
        redisLog(REDIS_WARNING,"Error saving DB on disk: %s", strerror(errno));
        if (di) dictReleaseIterator(di);
        return REDIS_ERR;
    }

保存 redis 数据到 rdb 数据库文件中， 函数太长就分解了一下：

- STEP-1: 临时数据库的名称， 包含了保存数据库时的时间和随机字符
- STEP-2: 使用临时数据库名称打开一个文件流， 如果文件流打开错误， 记录日志并返回 \
  REDIS_ERR
- STEP-3: 将 REDIS0000 字符串写入到文件流， 如果写入错误， 直接执行 werr 代码段， 代\
  码段的操是关闭文件流， 记录日志， 如果已经生成 di 了就释放了， 最终返回 REDIS_ERR \
  即 -1。
- STEP-4: 从这一步开始迭代写入每个 db。
    - STEP-1: 局部变量 dict 用于存放每轮循环中的哈希表， 然后 dictGetHashTableUsed_ \
      宏用于查看哈希表已经使用的数量， 如为 0 说明哈希表为空则执行 Continue 跳过此次\
      循环， 否则 dictGetIterator_ 函数生成哈希表迭代器 di， 如果 di 为假， 则关闭\
      文件流并返回 -1
    - STEP-2: 将 type 置为 REDIS_SELECTDB 即 254， 将 len db 序号从主机字节序转换\
      为网络字节序， 然后将 type 和 len 写入到文件流中， 如果写入出错执行 werr 代码\
      段
    - STEP-3: 从此处开始将哈希表的每个条目写入到文件中。 当 dictNext_ 函数值即 de 不\
      为空时开始循环。 dictNext_ 函数用于获取哈希表中的下一个条目。
    - STEP-4: 哈希表条目的 key 由 dictGetEntryKey_ 宏获取， 是一个 sds 字符串； \
      val 由 dictGetEntryVal_ 宏获取， 是一个 robj 对象
    - STEP-5: 分别将 dict 的 type、 len 和 key 写入到文件流中， 如果写入出错直接执\
      行 werr 代码段
    - STEP-6: 当 dict 的 type 为 REDIS_STRING 即 0 时， dict 的 val 就是 sds 字符\
      串， 然后将 val 的长度和值写入到文件流中， 写入出错就执行 werr， val 的长度使\
      用 sdslen_ 函数获取； 当 dict 的 type 为 REDIS_LIST 即 1 时， dict 的 val \
      就是 list 对象， 先将 list 的长度写入到文件流中， 然后从 list 头节点开始循环写\
      入每个节点的长度和值。 else 中的语句极大概率不会执行， 因此早期 redis 的数据中\
      只有字符串和 list 类型， 其他类型并没有进行处理
    - STEP-7: 哈希表处理完毕后， 通过 dictReleaseIterator_ 函数来释放掉迭代器 

- STEP-5: 将 REDIS_EOF 即 255 Redis 结束符写入到文件流中， 写入出错执行 werr 代码并\
  关闭文件流
- STEP-6: 使用 rename 函数将写好的临时数据库文件移动到目标地址， 执行成功返回 0， 失\
  败返回 -1； 如果 rename 失败， 将记录 redis 日志， 并使用 unlink 函数删除指定的临\
  时文件 tmpfile， 并最终返回 REDIS_ERR 即 -1。
- STEP-7: rename 成功也会记录 redis 日志， 并将 server 的 dirty 置为 0， lastsave \
  置为当前时间， 最后返回 REDIS_OK 即 0
- STEP-8: 在保存数据的任意一个过程失败都将会执行该代码段。

.. _dictGetHashTableUsed: beta-1-macros.rst#dictGetHashTableUsed-macro
.. _dictGetIterator: #dictGetIterator-func
.. _dictNext: #dictNext-func
.. _dictGetEntryKey: beta-1-macros.rst#dictGetEntryKey-macro
.. _dictGetEntryVal: beta-1-macros.rst#dictGetEntryKey-macro
.. _sdslen: #sdslen-func
.. _dictReleaseIterator: #dictReleaseIterator-func

.. _`dictGetIterator-func`:
.. `dictGetIterator-func`

41 dictGetIterator 函数
===============================================================================

.. code-block:: C 

    dictIterator *dictGetIterator(dict *ht)
    {
        dictIterator *iter = _dictAlloc(sizeof(*iter));

        iter->ht = ht;
        iter->index = -1;
        iter->entry = NULL;
        iter->nextEntry = NULL;
        return iter;
    }

生成一个哈希表迭代器， 结构体是 dictIterator_。

.. _dictIterator: beta-1-structures.rst#dictIterator-struct

首先分配这个迭代器的内存， 然后初始化迭代器内部各个字段的值， index 为 -1 说明还没开始\
迭代， 而且当前 entry 和 nextEntry 都是 NULL。 最终返回这个迭代器

.. _`dictNext-func`:
.. `dictNext-func`

42 dictNext 函数
===============================================================================

.. code-block:: C 

    dictEntry *dictNext(dictIterator *iter)
    {
        while (1) {
            if (iter->entry == NULL) {
                iter->index++;
                if (iter->index >=
                        (signed)iter->ht->size) break;
                iter->entry = iter->ht->table[iter->index];
            } else {
                iter->entry = iter->nextEntry;
            }
            if (iter->entry) {
                /* We need to save the 'next' here, the iterator user
                * may delete the entry we are returning. */
                iter->nextEntry = iter->entry->next;
                return iter->entry;
            }
        }
        return NULL;
    }

开始循环判断哈希表迭代器， 获取下一个 entry。

首先判断当前 entry 是否为 NULL：

- 若是， 说明这个迭代器是进行的初次迭代， 将 index 自增加 1； 如果 index 大于等于哈希\
  表的大小 size， 直接 break 循环， 并返回 NULL； 正常情况下将 entry 置为哈希表的 \
  index 索引代表的 entry； 若 entry 不是 NULL， 说明不是初次迭代， 直接将 entry 置\
  为 nextEntry。
- 当 entry 为真时， 将 nextEntry 置为 iter->entry->next， 即 next next， 并返回修\
  改后的 iter->entry。 

.. _`sdslen-func`:
.. `sdslen-func`

43 sdslen 函数
===============================================================================

.. code-block:: C 

    size_t sdslen(const sds s) {
        struct sdshdr *sh = (void*) (s-(sizeof(struct sdshdr)));
        return sh->len;
    }

之前分析过， redis 中的字符串时字符串头 (sdshdr) 和字符串拼接使用的， 在 sdshdr 中包\
含了字符串的长度， 但是在使用 sds 的时候， 字符串的指针指向的是 sdshdr 的 buf， 并不\
是 sdshdr， 因此需要减去 sdshdr 的大小， 从而使其指向 sdshdr， 最终返回 sdshdr 的 \
len 字段。 

.. _`dictReleaseIterator-func`:
.. `dictReleaseIterator-func`

44 dictReleaseIterator 函数
===============================================================================

.. code-block:: C 

    void dictReleaseIterator(dictIterator *iter)
    {
        _dictFree(iter);
    }

直接使用 `_dictFree`_ 函数释放掉哈希表迭代器占用的内存。

.. _`_dictFree`: #_dictFree-func

.. _`_dictFree-func`:
.. `_dictFree-func`

45 _dictFree 函数
===============================================================================

.. code-block:: C 

    static void _dictFree(void *ptr) {
        free(ptr);
    }

直接使用 free 函数释放掉给定的指针。

.. _`sdstrim-func`:
.. `sdstrim-func`

46 sdstrim 函数
===============================================================================

.. code-block:: C 

    sds sdstrim(sds s, const char *cset) {
        struct sdshdr *sh = (void*) (s-(sizeof(struct sdshdr)));
        char *start, *end, *sp, *ep;
        size_t len;

        sp = start = s;
        ep = end = s+sdslen(s)-1;
        while(sp <= end && strchr(cset, *sp)) sp++;
        while(ep > start && strchr(cset, *ep)) ep--;
        len = (sp > ep) ? 0 : ((ep-sp)+1);
        if (sh->buf != sp) memmove(sh->buf, sp, len);
        sh->buf[len] = '\0';
        sh->free = sh->free+(sh->len-len);
        sh->len = len;
        return s;
    }

从 sds 字符串首尾去除特定字符的函数。

sp 指的是字符串开始位置， 可以看做是 start point， ep 是字符串结束位置， 可以看做 \
end point， 然后循环判断 sp 指向的字符在 cset 中第一次出现的指针， strchr 函数就是这\
个意思， 执行成功返回指针， 失败返回 NULL； 直到 sp > end 或者 strchr 为 NULL。 下面\
的一个步骤反着进行， 从最后一个字符开始判断。 一旦首字符或尾字符不是 cset 中的， \
strchr 函数就返回 NULL， 从而推出 while 循环。

然后重新设置字符串长度， 当 sp > ep 时， 说明字符串都需要去除， len 就为 0 否则为 \
((ep-sp)+1)， 这是去除特定字符后的长度。 

当 sh->buf 即字符串与 sp 不相等时， 使用 memmove 将 sp 复制到 sh->buf， 复制 len 个\
字节， 就是将去除首尾特定字符后的字符串设置为 sds 字符串， 然后重新设置 sdshdr 中的值\
， 最终返回去除字符后的字符串。

.. _`sdssplitlen-func`:
.. `sdssplitlen-func`

47 sdssplitlen 函数
===============================================================================

.. code-block:: C 

    sds *sdssplitlen(char *s, int len, char *sep, int seplen, int *count) {
        // 1
        int elements = 0, slots = 5, start = 0, j;

        sds *tokens = malloc(sizeof(sds)*slots);
    #ifdef SDS_ABORT_ON_OOM
        if (tokens == NULL) sdsOomAbort();
    #endif
        if (seplen < 1 || len < 0 || tokens == NULL) return NULL;
        for (j = 0; j < (len-(seplen-1)); j++) {
            /* make sure there is room for the next element and the final one */
            // 2
            if (slots < elements+2) {
                slots *= 2;
                sds *newtokens = realloc(tokens,sizeof(sds)*slots);
                if (newtokens == NULL) {
    #ifdef SDS_ABORT_ON_OOM
                    sdsOomAbort();
    #else
                    goto cleanup;
    #endif
                }
                tokens = newtokens;
            }
            // 3
            /* search the separator */
            if ((seplen == 1 && *(s+j) == sep[0]) || (memcmp(s+j,sep,seplen) == 0)) {
                tokens[elements] = sdsnewlen(s+start,j-start);
                if (tokens[elements] == NULL) {
    #ifdef SDS_ABORT_ON_OOM
                    sdsOomAbort();
    #else
                    goto cleanup;
    #endif
                }
                elements++;
                start = j+seplen;
                j = j+seplen-1; /* skip the separator */
            }
        }

        // 4
        /* Add the final element. We are sure there is room in the tokens array. */
        tokens[elements] = sdsnewlen(s+start,len-start);
        if (tokens[elements] == NULL) {
    #ifdef SDS_ABORT_ON_OOM
                    sdsOomAbort();
    #else
                    goto cleanup;
    #endif
        }
        elements++;
        *count = elements;
        return tokens;

    // 5
    #ifndef SDS_ABORT_ON_OOM
    cleanup:
        {
            int i;
            for (i = 0; i < elements; i++) sdsfree(tokens[i]);
            free(tokens);
            return NULL;
        }
    #endif
    }

该函数用于拆分字符串， 分割符可以是一个字符， 也可以是多个字符。

- STEP-1: 初始化局部变量 elements 为 0； slots 为 5， slots 应该是用于存放拆分后的\
  字符串； start 为 0； 以及 j。 然后分配 slots 占用内存， 分配失败就执行 \
  sdsOomAbort_ 函数； 然后判断分割符的长度， 被分割字符串的长度以及 slots 内存释放分\
  配成功， 如果有任意一个为真， 都将返回 NULL。
- STEP-2: 在被分割字符串减去分割符长度范围内进行循环； 当 ``slots < elements+2`` 时\
  说明存储分割后的字符串的空间不足， slots 需要进行扩展， 在代码中直接扩大一倍， 然后\
  使用 realloc 函数重新分配内存。 如果内存分配失败， 将会执行 cleanup 代码段。
- STEP-3: 搜索条件有两个， 一是分割符只有一个， 且第 j 次循环时的字符等于分割符； 二\
  是 memcmp 函数的值为 0 即第 j 次循环后开始的字符串， 前 seplen 字符与 sep 相等。 \
  这两个条件任意满足一个， 都会执行 if 内部语句， 搜索到就进行字符串分割操作， 然后将\
  其存放到 tokens 内存中， 随后忽略分割符。
- STEP-4: 保存了之前分割的字符串， 但是分割后的最后一部分并没有保存， 因此在最后进行一\
  次保存， 保存完成后返回 tokens 即分割后的字符串。
- STEP-5: 此处是分割过程出现问题后， 需要的清理工作， 防止出现内存泄露等问题， 释放掉\
  之前创建对象占用的内存。

.. _sdsOomAbort: #sdsOomAbort-func

.. _`sdsempty-func`:
.. `sdsempty-func`

48 sdsempty 函数
===============================================================================

.. code-block:: C 

    sds sdsempty(void) {
        return sdsnewlen("",0);
    }

该函数使用 sdsnewlen_ 函数新建了一个长度为 0 的空字符串。

.. _sdsnewlen: #sdsnewlen-func

.. _`sdscatprintf-func`:
.. `sdscatprintf-func`

49 sdscatprintf 函数
===============================================================================

.. code-block:: C 

    sds sdscatprintf(sds s, const char *fmt, ...) {
        va_list ap;
        char *buf, *t;
        size_t buflen = 32;

        va_start(ap, fmt);
        while(1) {
            buf = malloc(buflen);
    #ifdef SDS_ABORT_ON_OOM
            if (buf == NULL) sdsOomAbort();
    #else
            if (buf == NULL) return NULL;
    #endif
            buf[buflen-2] = '\0';
            vsnprintf(buf, buflen, fmt, ap);
            if (buf[buflen-2] != '\0') {
                free(buf);
                buflen *= 2;
                continue;
            }
            break;
        }
        va_end(ap);
        t = sdscat(s, buf);
        free(buf);
        return t;
    }

将字符串格式化后再与字符串 s 进行拼接， 最后返回拼接后的字符串。 拼接函数使用的是 \
sdscat_ 

.. _sdscat: #sdscat-func

.. _`sdscat-func`:
.. `sdscat-func`

50 sdscat 函数
===============================================================================

.. code-block:: C 

    sds sdscat(sds s, char *t) {
        return sdscatlen(s, t, strlen(t));
    }

该函数通过调用 sdscatlen_ 函数进行字符串连接操作。 需要连接的长度是字符串 t 的长度。

.. _sdscatlen: #sdscatlen-func

.. _`sdscatlen-func`:
.. `sdscatlen-func`

51 sdscatlen 函数
===============================================================================

.. code-block:: C 

    sds sdscatlen(sds s, void *t, size_t len) {
        struct sdshdr *sh;
        size_t curlen = sdslen(s);

        s = sdsMakeRoomFor(s,len);
        if (s == NULL) return NULL;
        sh = (void*) (s-(sizeof(struct sdshdr)));
        memcpy(s+curlen, t, len);
        sh->len = curlen+len;
        sh->free = sh->free-len;
        s[curlen+len] = '\0';
        return s;
    }

当前字符串的长度使用 sdslen_ 函数进行获取， 之后使用 sdsMakeRoomFor_ 函数进行字符串 \
s 的拓展， 如果拓展失败返回 NULL。

然后将字符串 t 追加到字符串 s 的尾部， 同时进行 sdshdr 字段的相关变更， 最终返回拼接\
后的字符串 s

.. _sdsMakeRoomFor: #sdsMakeRoomFor-func

.. _`sdsMakeRoomFor-func`:
.. `sdsMakeRoomFor-func`

52 sdsMakeRoomFor 函数
===============================================================================

.. code-block:: C 

    static sds sdsMakeRoomFor(sds s, size_t addlen) {
        struct sdshdr *sh, *newsh;
        size_t free = sdsavail(s);
        size_t len, newlen;

        if (free >= addlen) return s;
        len = sdslen(s);
        sh = (void*) (s-(sizeof(struct sdshdr)));
        newlen = (len+addlen)*2;
        newsh = realloc(sh, sizeof(struct sdshdr)+newlen+1);
    #ifdef SDS_ABORT_ON_OOM
        if (newsh == NULL) sdsOomAbort();
    #else
        if (newsh == NULL) return NULL;
    #endif

        newsh->free = newlen - len;
        return newsh->buf;
    }

该函数用于拓展字符串 s 的内存空间。

首先创建两个 sdshdr， 字符串 s 的可用空间使用 sdsavail_ 函数进行获取。 当可用空间大于\
或等于需要增加的长度时， 直接返回字符串 s 不做任何操作。

.. _sdsavail: #sdsavail-func

否则将当前的长度加上需要增加的长度的和乘以 2 作为新的字符串的长度， 之后重新分配 sh 代\
表的内存， 随后修改新的可用空间， 最后返回拓展后的字符串。

.. _`sdsavail-func`:
.. `sdsavail-func`

53 sdsavail 函数
===============================================================================

.. code-block:: C 

    size_t sdsavail(sds s) {
        struct sdshdr *sh = (void*) (s-(sizeof(struct sdshdr)));
        return sh->free;
    }

该函数用于获取当前字符串 s 可用的空间。

.. _`createListObject-func`:
.. `createListObject-func`

54 createListObject 函数
===============================================================================

.. code-block:: C 

    static robj *createListObject(void) {
        list *l = listCreate();

        if (!l) oom("createListObject");
        listSetFreeMethod(l,decrRefCount);
        return createObject(REDIS_LIST,l);
    }

该函数用于创建 List 对象。 

首先使用 listCreate_ 函数创建一个空的 List， 然后使用 listSetFreeMethod_ 宏设置 \
List 的 Free 方法为 decrRefCount_ 函数。 

.. _`listCreate`: #listCreate-func
.. _`listSetFreeMethod`: beta-1-macros.rst#listSetFreeMethod-macro
.. _`decrRefCount`: #decrRefCount-func

最终返回使用 createObject_ 函数创建的 REDIS_LIST 对象。 

.. _`decrRefCount-func`:
.. `decrRefCount-func`

55 decrRefCount 函数
===============================================================================

.. code-block:: C 

    static void decrRefCount(void *obj) {
        robj *o = obj;
        if (--(o->refcount) == 0) {
            switch(o->type) {
            case REDIS_STRING: freeStringObject(o); break;
            case REDIS_LIST: freeListObject(o); break;
            case REDIS_SET: freeSetObject(o); break;
            default: assert(0 != 0); break;
            }
            if (!listAddNodeHead(server.objfreelist,o))
                free(o);
        }
    }

函数名称是减少引用计数， 参数是一个 void 类型的 obj 对象， 实际上就是不确定类型的对象， \
然后将 obj 赋值给 robj 对象即 redis 对象。 

首先判断将 robj 对象的引用计数减一后是否为 0， 若是， 看 robj 的 type 属性进行相关的\
操作: 1. 如果是 REDIS_STRING 类型， 则使用 freeStringObject_ 函数进行对象释放； 2. \
如果是 REDIS_LIST 类型， 则使用 freeListObject_ 函数进行对象释放； 3. 如果是 \
REDIS_SET 类型， 则使用 freeSetObject_ 函数进行对象释放； 其他情况执行 assert 语句， \
这是一处逻辑错误， 一旦执行到此处说明有逻辑问题。

.. _`freeStringObject`: #freeStringObject-func
.. _`freeListObject`: #freeListObject-func
.. _`freeSetObject`: #freeSetObject-func

然后将 o 对象使用 listAddNodeHead_ 函数添加到 objfreelist 的头部， 添加失败就执行 \
free 函数直接释放掉。

.. _`listAddNodeHead`: #listAddNodeHead-func

.. _`freeStringObject-func`:
.. `freeStringObject-func`

56 freeStringObject 函数
===============================================================================

.. code-block:: C 

    static void freeStringObject(robj *o) {
        sdsfree(o->ptr);
    }

该函数调用了 sdsfree_ 函数进行 String 对象的释放。

.. _`freeListObject-func`:
.. `freeListObject-func`

57 freeListObject 函数
===============================================================================

.. code-block:: C 

    static void freeListObject(robj *o) {
        listRelease((list*) o->ptr);
    }

该函数调用了 listRelease_ 函数进行 List 对象的释放。

.. _`freeSetObject-func`:
.. `freeSetObject-func`

58 freeSetObject 函数
===============================================================================

.. code-block:: C 

    static void freeSetObject(robj *o) {
        /* TODO */
        o = o;
    }

集合类型 set 在 beta-1 版本中还未实现。

.. _`listAddNodeHead-func`:
.. `listAddNodeHead-func`

59 listAddNodeHead 函数
===============================================================================

.. code-block:: C 

    list *listAddNodeHead(list *list, void *value)
    {
        listNode *node;

        if ((node = malloc(sizeof(*node))) == NULL)
            return NULL;
        node->value = value;
        if (list->len == 0) {
            list->head = list->tail = node;
            node->prev = node->next = NULL;
        } else {
            node->prev = NULL;
            node->next = list->head;
            list->head->prev = node;
            list->head = node;
        }
        list->len++;
        return list;
    }

该函数用于将一个值添加到 List 的头节点。

新建一个 listNode， 将需要添加的值设置为这个 listNode 的 value， 然后判断需要添加的 \
list 的长度是否为 0， 如果为 0 则头节点和尾节点都将是添加的这个值； 否则将需要添加的这\
个节点的前驱节点置为 NULL， 后继节点置为当前 list 的头节点， 然后将 list 头节点的前驱\
节点置为该 node， 头节点置为当前 node。

将 list 的 len 自增加一， 最后返回该 list。

.. _`listAddNodeTail-func`:
.. `listAddNodeTail-func`

60 listAddNodeTail 函数
===============================================================================

.. code-block:: C 

    list *listAddNodeTail(list *list, void *value)
    {
        listNode *node;

        if ((node = malloc(sizeof(*node))) == NULL)
            return NULL;
        node->value = value;
        if (list->len == 0) {
            list->head = list->tail = node;
            node->prev = node->next = NULL;
        } else {
            node->prev = list->tail;
            node->next = NULL;
            list->tail->next = node;
            list->tail = node;
        }
        list->len++;
        return list;
    }

和上一个函数差不多， 只不过这个是追加到 List 的尾节点。 

.. _`dictAdd-func`:
.. `dictAdd-func`

61 dictAdd 函数
===============================================================================

.. code-block:: C 

    int dictAdd(dict *ht, void *key, void *val)
    {
        int index;
        dictEntry *entry;

        /* Get the index of the new element, or -1 if
        * the element already exists. */
        if ((index = _dictKeyIndex(ht, key)) == -1)
            return DICT_ERR;

        /* Allocates the memory and stores key */
        entry = _dictAlloc(sizeof(*entry));
        entry->next = ht->table[index];
        ht->table[index] = entry;

        /* Set the hash entry fields. */
        dictSetHashKey(ht, entry, key);
        dictSetHashVal(ht, entry, val);
        ht->used++;
        return DICT_OK;
    }

将给定的 key 和 val 添加到哈希表中。

添加之前使用 `_dictKeyIndex`_ 函数从哈希表中获取 index 索引， 如果获取到 -1 就说明已\
经存在， 直接返回 DICT_ERR， 否则正常执行。

使用 `_dictAlloc`_ 函数分配一个 dictEntry 内存空间用于添加给定的 key 和 val， 将 \
entry 的 next 设置为 ht->table[index]， 同时 ht->table[index] 被置为 nextEntry。

然后使用 dictSetHashKey_ 宏和 dictSetHashVal_ 宏设置哈希表中的 Key 和 Val 字段。 \
设置完成后， 将哈希表的 used 自增加一， 并返回 DICT_OK 即 0。 实际上这两个宏直接就将\
给定的 key 和 val 设置到哈希表中了， 因为在 sdsDictType_ 类型中并没有设置 keyDup 和 \
valDup 值。

.. _`_dictKeyIndex`: #_dictKeyIndex-func
.. _`_dictAlloc`: #_dictAlloc-func
.. _`dictSetHashKey`: beta-1-macros.rst#dictSetHashKey-macro
.. _`dictSetHashVal`: beta-1-macros.rst#dictSetHashVal-macro

.. _`_dictKeyIndex-func`:
.. `_dictKeyIndex-func`

62 _dictKeyIndex 函数
===============================================================================

.. code-block:: C 

    static int _dictKeyIndex(dict *ht, const void *key)
    {
        unsigned int h;
        dictEntry *he;

        /* Expand the hashtable if needed */
        if (_dictExpandIfNeeded(ht) == DICT_ERR)
            return -1;
        /* Compute the key hash value */
        h = dictHashKey(ht, key) & ht->sizemask;
        /* Search if this slot does not already contain the given key */
        he = ht->table[h];
        while(he) {
            if (dictCompareHashKeys(ht, key, he->key))
                return -1;
            he = he->next;
        }
        return h;
    }

首先使用 `_dictExpandIfNeeded`_ 函数进行预判断， 如有需要将对哈希表扩展。

然后使用 dictHashKey_ 宏获取 key 在 ht 哈希表中的值， 实际上使用的是 \
sdsDictHashFunction_ 函数进行计算的 hash 值， 然后哈希值和 ht->sizemask 进行与运算\
获取 index 索引值。 然后循环使用 dictCompareHashKeys_ 宏对比哈希表中是否已经存在， \
如果已经存在则返回 -1， 若不存在则返回获取的 index 索引值 h。 dictCompareHashKeys_ \
宏实际执行的是 sdsDictKeyCompare_ 函数， 因为在 initServer_ 函数中 dictCreate_ 函\
数创建的是 sdsDictType_ 类型。 

.. _`_dictExpandIfNeeded`: #_dictExpandIfNeeded-func
.. _`dictHashKey`: beta-1-macros.rst#dictHashKey-macro
.. _`dictCompareHashKeys`: beta-1-macros.rst#dictCompareHashKeys-macro
.. _`dictCreate`: #dictCreate-func
.. _`sdsDictKeyCompare`: #sdsDictKeyCompare-func


.. _`_dictExpandIfNeeded-func`:
.. `_dictExpandIfNeeded-func`

63 _dictExpandIfNeeded 函数
===============================================================================

.. code-block:: C 

    static int _dictExpandIfNeeded(dict *ht)
    {
        /* If the hash table is empty expand it to the intial size,
        * if the table is "full" dobule its size. */
        if (ht->size == 0)
            return dictExpand(ht, DICT_HT_INITIAL_SIZE);
        if (ht->used == ht->size)
            return dictExpand(ht, ht->size*2);
        return DICT_OK;
    }

当哈希表 ht 的大小为 0 时说明是空哈希表， 使用 dictExpand_ 函数创建一个 \
DICT_HT_INITIAL_SIZE 即 16 个元素的哈希表； 如果已使用空间 used 等于哈希表的大小， \
说明已经全部使用完毕， 就将哈希表扩展至原来的 2 倍。 如果不需要拓展， 直接返回 \
DICT_OK 即 0， 其他情况下拓展失败， 则返回的是 DICT_ERR 即 1

.. _`dictExpand`: #dictExpand-func

.. _`sdsDictKeyCompare-func`:
.. `sdsDictKeyCompare-func`

64 sdsDictKeyCompare 函数
===============================================================================

.. code-block:: C 

    static int sdsDictKeyCompare(void *privdata, const void *key1,
            const void *key2)
    {
        int l1,l2;
        DICT_NOTUSED(privdata);

        l1 = sdslen((sds)key1);
        l2 = sdslen((sds)key2);
        if (l1 != l2) return 0;
        return memcmp(key1, key2, l1) == 0;
    }

比较两个 key， 首先将 privdata 使用 DICT_NOTUSED_ 宏处理一下， 因为在该函数内部并没\
有使用到， 但是函数指针声明的时候包含的有这个参数。

.. _`DICT_NOTUSED`: beta-1-macros.rst#DICT_NOTUSED-macro

之后使用 sdslen_ 函数获取两个 key 的长度， 长度不同肯定不相同， 不相等就返回 0 即假值\
。长度相同的话， 就比较内存中的值是否相等， memcmp 在两个值相等的时候返回 0， 那么该函\
数就会在相等的时候返回真值。

.. _`acceptHandler-func`:
.. `acceptHandler-func`

65 acceptHandler 函数
===============================================================================

.. code-block:: C 

    static void acceptHandler(aeEventLoop *el, int fd, void *privdata, int mask) {
        int cport, cfd;
        char cip[128];
        REDIS_NOTUSED(el);
        REDIS_NOTUSED(mask);
        REDIS_NOTUSED(privdata);

        cfd = anetAccept(server.neterr, fd, cip, &cport);
        if (cfd == AE_ERR) {
            redisLog(REDIS_DEBUG,"Accepting client connection: %s", server.neterr);
            return;
        }
        redisLog(REDIS_DEBUG,"Accepted %s:%d", cip, cport);
        if (createClient(cfd) == REDIS_ERR) {
            redisLog(REDIS_WARNING,"Error allocating resoures for the client");
            close(cfd); /* May be already closed, just ingore errors */
            return;
        }
    }

使用 REDIS_NOTUSED_ 宏将 el， mask 和 privdata 转换为 void 类型， 避免警告。

然后使用 anetAccept_ 函数进行连接， 正常执行会返回套接字文件描述符， 否则返回 \
ANET_ERR 即 -1。 当 cfd 为 AE_ERR 即 -1 时， 记录日志并无值返回。

正常连接之后， 使用 createClient_ 函数创建 Client 对象， 如果创建失败， 则记录日志， \
关闭连接并无值返回。

.. _`REDIS_NOTUSED`: beta-1-macros.rst#REDIS_NOTUSED-macro
.. _`anetAccept`: #anetAccept-func
.. _`createClient`: #createClient-func

.. _`anetAccept-func`:
.. `anetAccept-func`

66 anetAccept 函数
===============================================================================

.. code-block:: C 

    int anetAccept(char *err, int serversock, char *ip, int *port)
    {
        int fd;
        struct sockaddr_in sa;
        unsigned int saLen;

        while(1) {
            saLen = sizeof(sa);
            fd = accept(serversock, (struct sockaddr*)&sa, &saLen);
            if (fd == -1) {
                if (errno == EINTR)
                    continue;
                else {
                    anetSetError(err, "accept: %s\n", strerror(errno));
                    return ANET_ERR;
                }
            }
            break;
        }
        if (ip) strcpy(ip,inet_ntoa(sa.sin_addr));
        if (port) *port = ntohs(sa.sin_port);
        return fd;
    }

该函数用于创建套接字连接， 创建成功将返回文件描述符， 失败则 1. 如果 errno = EINTR， \
则继续执行； 2. 使用 anetSetError_ 函数打印错误日志并返回 ANET_ERR 即 -1

.. _`createClient-func`:
.. `createClient-func`

67 createClient 函数
===============================================================================

.. code-block:: C 

    static int createClient(int fd) {
        redisClient *c = malloc(sizeof(*c));

        anetNonBlock(NULL,fd);
        anetTcpNoDelay(NULL,fd);
        if (!c) return REDIS_ERR;
        selectDb(c,0);
        c->fd = fd;
        c->querybuf = sdsempty();
        c->argc = 0;
        c->bulklen = -1;
        c->sentlen = 0;
        c->lastinteraction = time(NULL);
        if ((c->reply = listCreate()) == NULL) oom("listCreate");
        listSetFreeMethod(c->reply,decrRefCount);
        if (aeCreateFileEvent(server.el, c->fd, AE_READABLE,
            readQueryFromClient, c, NULL) == AE_ERR) {
            freeClient(c);
            return REDIS_ERR;
        }
        if (!listAddNodeTail(server.clients,c)) oom("listAddNodeTail");
        return REDIS_OK;
    }

该函数用于创建一个 Client 对象。 

首先分配 Client 的内存空间； 然后使用 anetNonBlock_ 函数设置给定的 socket 连接为无阻\
塞， 然后使用 anetTcpNoDelay_ 函数禁用 TCP 无延迟连接。 

然后使用 selectDb_ 函数设置当前 Client 的 dict 为服务器中的第一个。 然后初始化 \
Client 的其他值， 其中 querybuf 属性被 sdsempty_ 函数清空。 reply 属性是一个空的 \
List， 使用的是 listCreate_ 函数创建的， 并将其 Free 方法使用 listSetFreeMethod_ \
宏设置为 decrRefCount_ 函数。 

然后使用 aeCreateFileEvent_ 函数创建一个 IO 事件， 但处理函数是 \
readQueryFromClient_ 函数， 对应的 client 就是创建的 Client。 如果创建 IO 事件失败\
则使用 freeClient_ 函数释放掉 Client 占用的内存， 并返回 REDIS_ERR 即 -1 

创建 IO 事件成功后， 将创建的 Client 使用 listAddNodeTail_ 函数追加到 \
server.clients 尾部。

最终返回 REDIS_OK 即 0.

.. _`anetNonBlock`: #anetNonBlock-func
.. _`anetTcpNoDelay`: #anetTcpNoDelay-func
.. _`selectDb`: #selectDb-func
.. _`sdsempty`: #sdsempty-func
.. _`readQueryFromClient`: #readQueryFromClient-func
.. _`listAddNodeTail`: #listAddNodeTail-func
.. _`aeCreateFileEvent`: beta-1-main-flow.rst#aeCreateFileEvent-func

.. _`anetNonBlock-func`:
.. `anetNonBlock-func`

68 anetNonBlock 函数
===============================================================================

.. code-block:: C 

    int anetNonBlock(char *err, int fd)
    {
        int flags;

        /* Set the socket nonblocking.
        * Note that fcntl(2) for F_GETFL and F_SETFL can't be
        * interrupted by a signal. */
        if ((flags = fcntl(fd, F_GETFL)) == -1) {
            anetSetError(err, "fcntl(F_GETFL): %s\n", strerror(errno));
            return ANET_ERR;
        }
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            anetSetError(err, "fcntl(F_SETFL,O_NONBLOCK): %s\n", strerror(errno));
            return ANET_ERR;
        }
        return ANET_OK;
    }

首先使用 fcntl 函数获取给定的文件描述符 fd 的 access mode 和状态标志， 如果为 -1 则\
使用 anetSetError_ 函数打印错误信息并返回 ANET_ERR 即 -1

然后在使用 fcntl 函数设置给定的文件描述符 fd 为 O_NONBLOCK 模式， 如果设置失败则返回 \
-1 使用 anetSetError_ 函数打印错误信息并返回 ANET_ERR 即 -1

无异常则最终返回 ANET_OK 即 0

.. _`anetTcpNoDelay-func`:
.. `anetTcpNoDelay-func`

69 anetTcpNoDelay 函数
===============================================================================

.. code-block:: C 

    int anetTcpNoDelay(char *err, int fd)
    {
        int yes = 1;
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) == -1)
        {
            anetSetError(err, "setsockopt TCP_NODELAY: %s\n", strerror(errno));
            return ANET_ERR;
        }
        return ANET_OK;
    }

使用 setsockopt 函数设置给定的 socket 描述符 fd 的选项， 在本函数中， 将 \
IPPROTO_TCP 即 IP 协议族中的 TCP 协议的 TCP_NODELAY 选项设置为 yes 即 1 (真值)， \
如果执行失败则返回 -1 并设置相应的 errno， 使用 anetSetError_ 打印错误信息并返回 \
ANET_ERR 即 -1

无异常则最终返回 ANET_OK 即 0

.. _`selectDb-func`:
.. `selectDb-func`

70 selectDb 函数
===============================================================================

.. code-block:: C 

    static int selectDb(redisClient *c, int id) {
        if (id < 0 || id >= server.dbnum)
            return REDIS_ERR;
        c->dict = server.dict[id];
        return REDIS_OK;
    }

对给定的 redisClient c 选择指定 id 的数据库。

无异常最终返回 REDIS_OK 即 0

.. _`readQueryFromClient-func`:
.. `readQueryFromClient-func`

71 readQueryFromClient 函数
===============================================================================

.. code-block:: C 

    static void readQueryFromClient(aeEventLoop *el, int fd, void *privdata, int mask) {
        redisClient *c = (redisClient*) privdata;
        char buf[REDIS_QUERYBUF_LEN];
        int nread;
        REDIS_NOTUSED(el);
        REDIS_NOTUSED(mask);

        // 1
        nread = read(fd, buf, REDIS_QUERYBUF_LEN);
        if (nread == -1) {
            if (errno == EAGAIN) {
                nread = 0;
            } else {
                redisLog(REDIS_DEBUG, "Reading from client: %s",strerror(errno));
                freeClient(c);
                return;
            }
        } else if (nread == 0) {
        // 2    
            redisLog(REDIS_DEBUG, "Client closed connection");
            freeClient(c);
            return;
        }
        if (nread) {
            // 3
            c->querybuf = sdscatlen(c->querybuf, buf, nread);
            c->lastinteraction = time(NULL);
        } else {
            return;
        }

    again:
        if (c->bulklen == -1) {
            /* Read the first line of the query */
            // 4
            char *p = strchr(c->querybuf,'\n');
            size_t querylen;
            if (p) {
                sds query, *argv;
                int argc, j;
                
                // 5
                query = c->querybuf;
                c->querybuf = sdsempty();
                querylen = 1+(p-(query));
                if (sdslen(query) > querylen) {
                    /* leave data after the first line of the query in the buffer */
                    c->querybuf = sdscatlen(c->querybuf,query+querylen,sdslen(query)-querylen);
                }
                // 6
                *p = '\0'; /* remove "\n" */
                if (*(p-1) == '\r') *(p-1) = '\0'; /* and "\r" if any */
                sdsupdatelen(query);

                // 7
                /* Now we can split the query in arguments */
                if (sdslen(query) == 0) {
                    /* Ignore empty query */
                    sdsfree(query);
                    return;
                }

                // 8
                argv = sdssplitlen(query,sdslen(query)," ",1,&argc);
                sdsfree(query);
                if (argv == NULL) oom("Splitting query in token");
                for (j = 0; j < argc && j < REDIS_MAX_ARGS; j++) {
                    if (sdslen(argv[j])) {
                        c->argv[c->argc] = argv[j];
                        c->argc++;
                    } else {
                        sdsfree(argv[j]);
                    }
                }

                // 9
                free(argv);
                /* Execute the command. If the client is still valid
                * after processCommand() return and there is something
                * on the query buffer try to process the next command. */
                if (processCommand(c) && sdslen(c->querybuf)) goto again;
                return;
            } else if (sdslen(c->querybuf) >= 1024) {
                
                // 10
                redisLog(REDIS_DEBUG, "Client protocol error");
                freeClient(c);
                return;
            }
        } else {
        // 11
            /* Bulk read handling. Note that if we are at this point
            the client already sent a command terminated with a newline,
            we are reading the bulk data that is actually the last
            argument of the command. */
            int qbl = sdslen(c->querybuf);

            if (c->bulklen <= qbl) {
                /* Copy everything but the final CRLF as final argument */
                c->argv[c->argc] = sdsnewlen(c->querybuf,c->bulklen-2);
                c->argc++;
                c->querybuf = sdsrange(c->querybuf,c->bulklen,-1);
                processCommand(c);
                return;
            }
        }
    }

该函数太长， 分成几个小部分进行解读：

- STEP-1: 将没有使用的参数 el 和 mask 使用 REDIS_NOTUSED_ 宏转换为 void 类型， 将 \
  privdata 转换为 redisClient_ 类型， 然后从给定的文件描述符 fd 中读取 \
  REDIS_QUERYBUF_LEN 即 1024 字节存放到 buf 中， 如果读取失败返回 -1 且 errno 为 \
  EAGAIN 则将 nread 置为 0； 否则将记录日志， 使用 freeClient_ 函数释放 client 内存\
  并无值返回
- STEP-2: 当 nread 为 0 时， 记录日志释放 client 内存并无值返回 
- STEP-3: nread 正常时， 使用 sdscatlen_ 函数将读取到的内容 buf 与 c->querybuf 进\
  行拼接， 同时将 lastinteraction 属性置为当前时间。
- STEP-4: 从 c->querybuf 中查找第一个 '\\n' 换行符， 如果找到了就执行 if 内部语句否\
  则执行 else 内的语句。
- STEP-5: 将 c->querybuf 存为 query， 然后使用 sdsempty_ 函数清空 c->querybuf， p \
  - (query) 表示的是第一个换行符到 query 第一个字符的距离， 加上 1 就是 query 的长\
  度。 sdslen(query) > querylen 成立的时候说明 querybuf 中存在多个查询命令。 然后使\
  用 sdscatlen_ 函数拼接字符串， query+querylen 将字符串的起始指针移动， 使其去除已\
  经解析的 query。 
- STEP-6: 将 p 指向的字符置为 '\\0'， 达到去除 '\\n' 的目的， 如果 '\\n' 前面是 \
  '\\r'， 也置为 '\\0'， 然后使用 sdsupdatelen_ 函数更新一下 query 
- STEP-7: 如果查询 query 长度为 0 直接使用 sdsfree_ 函数释放其内存并无值返回。
- STEP-8: 使用 sdssplitlen_ 函数将 query 字符串以空格进行拆分。 拆分后将 query 释\
  放， argv 就是拆分后的结果， 如果 argv 为 NULL， 则执行 oom 警告。 然后经分割的结\
  果存入到 c->argv 数组中， 并将 argc 自增。 
- STEP-9: 将 argv 存入 c->argv 后将 argv 释放， 并执行 processCommand_ 函数进行处\
  理相关查询， 如果执行成功且 c->querybuf 的长度大于 0 则继续执行 again 代码块， 否\
  则无值返回。
- STEP-10: c->querybuf 长度大于等于 1024， 则直接记录错误日志释放 Client 并无值返\
  回， 因为之前读取的时候设置的长度是 REDIS_QUERYBUF_LEN 即 1024
- STEP-11: 读取最后一个查询命令， 将命令存入 argv 后， 将 querybuf 使用 sdsrange_ \
  函数截取剩下的字符； 之后使用 processCommand_ 函数处理查询命令， 最后无值返回。

.. _`sdsupdatelen`: #sdsupdatelen-func
.. _`sdssplitlen`: #sdssplitlen-func
.. _`processCommand`: #processCommand-func
.. _`sdsrange`: #sdsrange-func

.. _`sdsupdatelen-func`:
.. `sdsupdatelen-func`

72 sdsupdatelen 函数
===============================================================================

.. code-block:: C 

    void sdsupdatelen(sds s) {
        struct sdshdr *sh = (void*) (s-(sizeof(struct sdshdr)));
        int reallen = strlen(s);
        sh->free += (sh->len-reallen);
        sh->len = reallen;
    }

更新 sds 字符串的真实长度。

.. _`processCommand-func`:
.. `processCommand-func`

73 processCommand 函数
===============================================================================

.. code-block:: C 

    static int processCommand(redisClient *c) {
        struct redisCommand *cmd;

        // 1
        sdstolower(c->argv[0]);
        /* The QUIT command is handled as a special case. Normal command
        * procs are unable to close the client connection safely */
        if (!strcmp(c->argv[0],"quit")) {
            freeClient(c);
            return 0;
        }

        // 2
        cmd = lookupCommand(c->argv[0]);
        if (!cmd) {
            addReplySds(c,sdsnew("-ERR unknown command\r\n"));
            resetClient(c);
            return 1;
        } else if (cmd->arity != c->argc) {
        // 3    
            addReplySds(c,sdsnew("-ERR wrong number of arguments\r\n"));
            resetClient(c);
            return 1;
        } else if (cmd->type == REDIS_CMD_BULK && c->bulklen == -1) {
            
            // 4
            int bulklen = atoi(c->argv[c->argc-1]);

            sdsfree(c->argv[c->argc-1]);
            if (bulklen < 0 || bulklen > 1024*1024*1024) {
                c->argc--;
                c->argv[c->argc] = NULL;
                addReplySds(c,sdsnew("-ERR invalid bulk write count\r\n"));
                resetClient(c);
                return 1;
            }
            c->argv[c->argc-1] = NULL;
            c->argc--;
            c->bulklen = bulklen+2; /* add two bytes for CR+LF */
            /* It is possible that the bulk read is already in the
            * buffer. Check this condition and handle it accordingly */
            if ((signed)sdslen(c->querybuf) >= c->bulklen) {
                c->argv[c->argc] = sdsnewlen(c->querybuf,c->bulklen-2);
                c->argc++;
                c->querybuf = sdsrange(c->querybuf,c->bulklen,-1);
            } else {
                return 1;
            }
        }
        // 5
        /* Exec the command */
        cmd->proc(c);
        resetClient(c);
        return 1;
    }

该函数有些长， 分成 5 部分进行解析。

- STEP-1: 新建一个 redisCommand_ 结构体实例 cmd， 然后将第一个命令 argv[0] 使用 \
  sdstolower_ 函数转换为小写字符。 如果 argv[0] 等于 quit 则释放 client 内存并返回 \
  0， 就是 redis 的 quit 命令
- STEP-2: 使用 lookupCommand_ 函数查找命令需要执行的函数， 如果函数值 cmd 为假， 则\
  addReplySds_ 函数添加回复字符串， 并使用 resetClient_ 函数重置 client 最后返回 1
- STEP-3: 如果 cmd 的 arity 属性和 argc 属性不相等， 则同 STEP-2
- STEP-4: 如果 cmd 的 type 属性为 REDIS_CMD_BULK 即 1 且 c->bulklen 为 -1， TODO \
  这一段逻辑不太明白， 先放过
- STEP-5: 执行 cmd 的 proc 函数指针， 执行指向的函数， 然后使用 resetClient_ 函数重\
  置 client 最终返回 1

.. _`redisCommand`: beta-1-structures.rst#redisCommand-struct
.. _`sdstolower`: #sdstolower-func
.. _`lookupCommand`: #lookupCommand-func
.. _`addReplySds`: #addReplySds-func
.. _`resetClient`: #resetClient-func

.. _`sdstolower-func`:
.. `sdstolower-func`

74 sdstolower 函数
===============================================================================

.. code-block:: C 

    void sdstolower(sds s) {
        int len = sdslen(s), j;

        for (j = 0; j < len; j++) s[j] = tolower(s[j]);
    }

使用 for 循环， 将字符串中的每个字符使用 tolower 函数转换为小写字符

.. _`sdstoupper-func`:
.. `sdstoupper-func`

75 sdstoupper 函数
===============================================================================

.. code-block:: C 

    void sdstoupper(sds s) {
        int len = sdslen(s), j;

        for (j = 0; j < len; j++) s[j] = toupper(s[j]);
    }

同上， 使用 toupper 将字符串中的每个字符转换为大写字符

.. _`lookupCommand-func`:
.. `lookupCommand-func`

76 lookupCommand 函数
===============================================================================

.. code-block:: C 

    static struct redisCommand cmdTable[] = {
        {"get",getCommand,2,REDIS_CMD_INLINE},
        {"set",setCommand,3,REDIS_CMD_BULK},
        {"setnx",setnxCommand,3,REDIS_CMD_BULK},
        {"del",delCommand,2,REDIS_CMD_INLINE},
        {"exists",existsCommand,2,REDIS_CMD_INLINE},
        {"incr",incrCommand,2,REDIS_CMD_INLINE},
        {"decr",decrCommand,2,REDIS_CMD_INLINE},
        {"rpush",rpushCommand,3,REDIS_CMD_BULK},
        {"lpush",lpushCommand,3,REDIS_CMD_BULK},
        {"rpop",rpopCommand,2,REDIS_CMD_INLINE},
        {"lpop",lpopCommand,2,REDIS_CMD_INLINE},
        {"llen",llenCommand,2,REDIS_CMD_INLINE},
        {"lindex",lindexCommand,3,REDIS_CMD_INLINE},
        {"lrange",lrangeCommand,4,REDIS_CMD_INLINE},
        {"ltrim",ltrimCommand,4,REDIS_CMD_INLINE},
        {"randomkey",randomkeyCommand,1,REDIS_CMD_INLINE},
        {"select",selectCommand,2,REDIS_CMD_INLINE},
        {"move",moveCommand,3,REDIS_CMD_INLINE},
        {"rename",renameCommand,3,REDIS_CMD_INLINE},
        {"renamenx",renamenxCommand,3,REDIS_CMD_INLINE},
        {"keys",keysCommand,2,REDIS_CMD_INLINE},
        {"dbsize",dbsizeCommand,1,REDIS_CMD_INLINE},
        {"ping",pingCommand,1,REDIS_CMD_INLINE},
        {"echo",echoCommand,2,REDIS_CMD_BULK},
        {"save",saveCommand,1,REDIS_CMD_INLINE},
        {"bgsave",bgsaveCommand,1,REDIS_CMD_INLINE},
        {"shutdown",shutdownCommand,1,REDIS_CMD_INLINE},
        {"lastsave",lastsaveCommand,1,REDIS_CMD_INLINE},
        /* lpop, rpop, lindex, llen */
        /* dirty, lastsave, info */
        {"",NULL,0,0}
    };

    static struct redisCommand *lookupCommand(char *name) {
        int j = 0;
        while(cmdTable[j].name != NULL) {
            if (!strcmp(name,cmdTable[j].name)) return &cmdTable[j];
            j++;
        }
        return NULL;
    }

在 cmdTable 中用给定的命令名称 name 查找相应的执行函数， 如果找到对应的函数， 则返回\
这一条 redisCommand， 否则返回 NULL。 

.. _`addReplySds-func`:
.. `addReplySds-func`

77 addReplySds 函数
===============================================================================

.. code-block:: C 

    static void addReplySds(redisClient *c, sds s) {
        robj *o = createObject(REDIS_STRING,s);
        addReply(c,o);
        decrRefCount(o);
    }

以给定的字符串创建一个 REDIS_STRING 对象， 将这个对象使用 addReply_ 函数回复给 \
client， 然后将这个 REDIS_STRING 对象使用 decrRefCount_ 函数减少引用计数

.. _`addReply`:  #addReply-func

.. _`addReply-func`:
.. `addReply-func`

78 addReply 函数
===============================================================================

.. code-block:: C 

    static void addReply(redisClient *c, robj *obj) {
        if (listLength(c->reply) == 0 &&
            aeCreateFileEvent(server.el, c->fd, AE_WRITABLE,
            sendReplyToClient, c, NULL) == AE_ERR) return;
        if (!listAddNodeTail(c->reply,obj)) oom("listAddNodeTail");
        incrRefCount(obj);
    }

添加回复信息； 如果 client 的 reply 属性长度为 0 且创建回复 IO 事件失败即 \
sendReplyToClient_ 函数执行失败了则无值返回； 如果正常执行， 则将回复的 robj 添加到 \
reply list 尾节点， 同时使用 incrRefCount_ 函数将引用计数增加 1

.. _`sendReplyToClient`: #sendReplyToClient-func
.. _`incrRefCount`: #incrRefCount-func

.. _`sendReplyToClient-func`:
.. `sendReplyToClient-func`

79 sendReplyToClient 函数
===============================================================================

.. code-block:: C 

    static void sendReplyToClient(aeEventLoop *el, int fd, void *privdata, int mask) {
        redisClient *c = privdata;
        int nwritten = 0, totwritten = 0, objlen;
        robj *o;
        REDIS_NOTUSED(el);
        REDIS_NOTUSED(mask);

        while(listLength(c->reply)) {
            // 1
            o = listNodeValue(listFirst(c->reply));
            objlen = sdslen(o->ptr);

            if (objlen == 0) {
                listDelNode(c->reply,listFirst(c->reply));
                continue;
            }

            // 2
            nwritten = write(fd, o->ptr+c->sentlen, objlen - c->sentlen);
            if (nwritten <= 0) break;
            c->sentlen += nwritten;
            totwritten += nwritten;
            /* If we fully sent the object on head go to the next one */
            if (c->sentlen == objlen) {
                listDelNode(c->reply,listFirst(c->reply));
                c->sentlen = 0;
            }
        }
        // 3
        if (nwritten == -1) {
            if (errno == EAGAIN) {
                nwritten = 0;
            } else {
                redisLog(REDIS_DEBUG,
                    "Error writing to client: %s", strerror(errno));
                freeClient(c);
                return;
            }
        }

        // 4
        if (totwritten > 0) c->lastinteraction = time(NULL);
        if (listLength(c->reply) == 0) {
            c->sentlen = 0;
            aeDeleteFileEvent(server.el,c->fd,AE_WRITABLE);
        }
    }

该函数用于将 server 的回复发送给 client。 

- STEP-1: 使用 listNodeValue_ 宏获取 reply list 的头节点， 如果其长度为 0 则使用 \
  listDelNode_ 函数删除该头节点， 继续下一轮循环
- STEP-2: 将需要发送的信息使用 write 函数写入到 fd 中， 如果写入 0 或失败， 则执行 \
  break 语句中断循环； 正常情况下， 发送完毕后即 sentlen 等于 objlen， 将头节点删除， \
  并将 sentlen 重置为 0
- STEP-3: 当 write 写入失败时， 如果 errno 为 EAGAIN， 则将 nwritten 置为 0， 否则\
  记录日志， 使用 freeClient_ 函数释放 client 并无值返回。
- STEP-4: 当 totwritten 大于 0 时更新 lastinteraction 属性为当前时间； 如果 reply \
  长度为 0 则将 sentlen 置为 0 并使用 aeDeleteFileEvent_ 函数删除 IO 事件

.. _`incrRefCount-func`:
.. `incrRefCount-func`

80 incrRefCount 函数
===============================================================================

.. code-block:: C 

    static void incrRefCount(robj *o) {
        o->refcount++;
    }

直接将 robj 的 refcount 属性自增加 1 即引用计数加 1

.. _`sdsDictKeyDestructor-func`:
.. `sdsDictKeyDestructor-func`

81 sdsDictKeyDestructor 函数
===============================================================================

.. code-block:: C 

    static void sdsDictKeyDestructor(void *privdata, void *val)
    {
        DICT_NOTUSED(privdata);

        sdsfree(val);
    }

哈希表中的 key 销毁函数。 首先将没用到的 privdata 使用 DICT_NOTUSED_ 宏定义转换为 \
void 类型， 然后使用 sdsfree_ 函数将给定的 val 值释放掉

.. _`sdsDictValDestructor-func`:
.. `sdsDictValDestructor-func`

82 sdsDictValDestructor 函数
===============================================================================

.. code-block:: C 

    static void sdsDictValDestructor(void *privdata, void *val)
    {
        DICT_NOTUSED(privdata);

        decrRefCount(val);
    }

哈希表中的 val 销毁函数。 首先将没用到的 privdata 使用 DICT_NOTUSED_ 宏定义转换为 \
void 类型， 然后使用 decrRefCount_ 函数将给定的 val 值的引用计数进行相应的减少并释放\
对应的对象。

.. _`resetClient-func`:
.. `resetClient-func`

83 resetClient 函数
===============================================================================

.. code-block:: C 

    static void resetClient(redisClient *c) {
        freeClientArgv(c);
        c->bulklen = -1;
    }

重置 Client， 使用 freeClientArgv_ 函数将 Client 的 argv 属性清除， 并将 bulklen \
属性置为 -1

.. _`sdsrange-func`:
.. `sdsrange-func`

84 sdsrange 函数
===============================================================================

.. code-block:: C 

    sds sdsrange(sds s, long start, long end) {
        struct sdshdr *sh = (void*) (s-(sizeof(struct sdshdr)));
        size_t newlen, len = sdslen(s);

        if (len == 0) return s;
        if (start < 0) {
            start = len+start;
            if (start < 0) start = 0;
        }
        if (end < 0) {
            end = len+end;
            if (end < 0) end = 0;
        }
        newlen = (start > end) ? 0 : (end-start)+1;
        if (newlen != 0) {
            if (start >= (signed)len) start = len-1;
            if (end >= (signed)len) end = len-1;
            newlen = (start > end) ? 0 : (end-start)+1;
        } else {
            start = 0;
        }
        if (start != 0) memmove(sh->buf, sh->buf+start, newlen);
        sh->buf[newlen] = 0;
        sh->free = sh->free+(sh->len-newlen);
        sh->len = newlen;
        return s;
    }

该函数用于截取字符串。 start 是起始索引， end 是终止索引， 获取它们之间的字符串。

.. _`aeSearchNearestTimer-func`:
.. `aeSearchNearestTimer-func`

85 aeSearchNearestTimer 函数
===============================================================================

.. code-block:: C 

    static aeTimeEvent *aeSearchNearestTimer(aeEventLoop *eventLoop)
    {
        aeTimeEvent *te = eventLoop->timeEventHead;
        aeTimeEvent *nearest = NULL;

        while(te) {
            if (!nearest || te->when_sec < nearest->when_sec ||
                    (te->when_sec == nearest->when_sec &&
                    te->when_ms < nearest->when_ms))
                nearest = te;
            te = te->next;
        }
        return nearest;
    }

在 while 循环中判断最近的一个定时器； aeTimeEvent 是一个链表结构， 判断每个节点的相关\
的值:

1. te->when_sec 小于最近的点
2. te->when_sec 相等， te->when_ms 小于最近的点

满足上述任一一个， 就将这个点赋值给 nearest， 直到 while 循环将 aeTimeEvent 循环判断\
完毕， 最后返回 nearest

.. _`aeMain-func`:
.. `aeMain-func`

86 aeMain 函数
===============================================================================

.. code-block:: C 

    void aeMain(aeEventLoop *eventLoop)
    {
        eventLoop->stop = 0;
        while (!eventLoop->stop)
            aeProcessEvents(eventLoop, AE_ALL_EVENTS);
    }

一个死循环， 不停的执行 aeProcessEvents_ 函数， 处理事件

.. _`aeProcessEvents`: #aeProcessEvents-func


