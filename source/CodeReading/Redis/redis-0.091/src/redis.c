/*
 * Copyright (c) 2006-2009, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define REDIS_VERSION "0.091"

#include "fmacros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <limits.h>

#include "ae.h"     /* Event driven programming library */
#include "sds.h"    /* Dynamic safe strings */
#include "anet.h"   /* Networking the easy way */
#include "dict.h"   /* Hash tables */
#include "adlist.h" /* Linked lists */
#include "zmalloc.h" /* total memory usage aware version of malloc/free */
#include "lzf.h"

/* Error codes */
#define REDIS_OK                0
#define REDIS_ERR               -1

/* Static server configuration */
#define REDIS_SERVERPORT        6379    /* TCP port */
#define REDIS_MAXIDLETIME       (60*5)  /* default client timeout */
#define REDIS_QUERYBUF_LEN      1024
#define REDIS_LOADBUF_LEN       1024
#define REDIS_MAX_ARGS          16
#define REDIS_DEFAULT_DBNUM     16
#define REDIS_CONFIGLINE_MAX    1024
#define REDIS_OBJFREELIST_MAX   1000000 /* Max number of objects to cache */
#define REDIS_MAX_SYNC_TIME     60      /* Slave can't take more to sync */
#define REDIS_EXPIRELOOKUPS_PER_CRON    100 /* try to expire 100 keys/second */

/* Hash table parameters */
#define REDIS_HT_MINFILL        10      /* Minimal hash table fill 10% */
#define REDIS_HT_MINSLOTS       16384   /* Never resize the HT under this */

/* Command flags */
#define REDIS_CMD_BULK          1
#define REDIS_CMD_INLINE        2

/* Object types */
#define REDIS_STRING 0
#define REDIS_LIST 1
#define REDIS_SET 2
#define REDIS_HASH 3

/* Object types only used for dumping to disk */
#define REDIS_EXPIRETIME 253
#define REDIS_SELECTDB 254
#define REDIS_EOF 255

/* Defines related to the dump file format. To store 32 bits lengths for short
 * keys requires a lot of space, so we check the most significant 2 bits of
 * the first byte to interpreter the length:
 *
 * 00|000000 => if the two MSB are 00 the len is the 6 bits of this byte
 * 01|000000 00000000 =>  01, the len is 14 byes, 6 bits + 8 bits of next byte
 * 10|000000 [32 bit integer] => if it's 01, a full 32 bit len will follow
 * 11|000000 this means: specially encoded object will follow. The six bits
 *           number specify the kind of object that follows.
 *           See the REDIS_RDB_ENC_* defines.
 *
 * Lenghts up to 63 are stored using a single byte, most DB keys, and may
 * values, will fit inside. */
#define REDIS_RDB_6BITLEN 0
#define REDIS_RDB_14BITLEN 1
#define REDIS_RDB_32BITLEN 2
#define REDIS_RDB_ENCVAL 3
#define REDIS_RDB_LENERR UINT_MAX

/* When a length of a string object stored on disk has the first two bits
 * set, the remaining two bits specify a special encoding for the object
 * accordingly to the following defines: */
#define REDIS_RDB_ENC_INT8 0        /* 8 bit signed integer */
#define REDIS_RDB_ENC_INT16 1       /* 16 bit signed integer */
#define REDIS_RDB_ENC_INT32 2       /* 32 bit signed integer */
#define REDIS_RDB_ENC_LZF 3         /* string compressed with FASTLZ */

/* Client flags */
#define REDIS_CLOSE 1       /* This client connection should be closed ASAP */
#define REDIS_SLAVE 2       /* This client is a slave server */
#define REDIS_MASTER 4      /* This client is a master server */
#define REDIS_MONITOR 8      /* This client is a slave monitor, see MONITOR */

/* Server replication state */
#define REDIS_REPL_NONE 0   /* No active replication */
#define REDIS_REPL_CONNECT 1    /* Must connect to master */
#define REDIS_REPL_CONNECTED 2  /* Connected to master */

/* List related stuff */
#define REDIS_HEAD 0
#define REDIS_TAIL 1

/* Sort operations */
#define REDIS_SORT_GET 0
#define REDIS_SORT_DEL 1
#define REDIS_SORT_INCR 2
#define REDIS_SORT_DECR 3
#define REDIS_SORT_ASC 4
#define REDIS_SORT_DESC 5
#define REDIS_SORTKEY_MAX 1024

/* Log levels */
#define REDIS_DEBUG 0
#define REDIS_NOTICE 1
#define REDIS_WARNING 2

/* Anti-warning macro... */
#define REDIS_NOTUSED(V) ((void) V)

/*================================= Data types ============================== */

/* A redis object, that is a type able to hold a string / list / set */
/*
 * redis 对象
 * ptr: 对象指针
 * type: 类型 (string, list, set)
 * refcount: 引用计数
 */
typedef struct redisObject {
    void *ptr;
    int type;
    int refcount;
} robj;

/*
 * redis 数据库结构体
 * dict: 字典指针
 * expires: 过期时间
 * id: 数据编号 id
 */
typedef struct redisDb {
    dict *dict;
    dict *expires;
    int id;
} redisDb;

/* With multiplexing we need to take per-clinet state.
 * Clients are taken in a liked list. */
typedef struct redisClient {
    int fd;
    redisDb *db;
    int dictid;
    sds querybuf;
    robj *argv[REDIS_MAX_ARGS];
    int argc;
    int bulklen;    /* bulk read len. -1 if not in bulk read mode */
    list *reply;
    int sentlen;
    time_t lastinteraction; /* time of the last interaction, used for timeout */
    int flags; /* REDIS_CLOSE | REDIS_SLAVE | REDIS_MONITOR */
    int slaveseldb; /* slave selected db, if this client is a slave */
    int authenticated;    /* when requirepass is non-NULL */
} redisClient;

struct saveparam {
    time_t seconds;
    int changes;
};

/* Global server state structure */
struct redisServer {
    int port; // server 端口， int 类型
    int fd;   // server 的文件描述符， int 类型
    redisDb *db;  // redis db 数据库， redisDb 结构
    dict *sharingpool; // 共享池， dict 结构
    unsigned int sharingpoolsize; // 共享池大小， 无符号整数
    long long dirty;            /* changes to DB from the last save */
    list *clients;
    list *slaves, *monitors;
    char neterr[ANET_ERR_LEN];
    aeEventLoop *el;
    int cronloops;              /* number of times the cron function run */
    list *objfreelist;          /* A list of freed objects to avoid malloc() */
    time_t lastsave;            /* Unix time of last save succeeede */
    size_t usedmemory;             /* Used memory in megabytes */
    /* Fields used only for stats */
    time_t stat_starttime;         /* server start time */
    long long stat_numcommands;    /* number of processed commands */
    long long stat_numconnections; /* number of connections received */
    /* Configuration */
    int verbosity;
    int glueoutputbuf;
    int maxidletime;
    int dbnum;
    int daemonize;
    char *pidfile;
    int bgsaveinprogress;
    struct saveparam *saveparams;
    int saveparamslen;
    char *logfile;
    char *bindaddr;
    char *dbfilename;
    char *requirepass;
    int shareobjects;
    /* Replication related */
    int isslave;
    char *masterhost;
    int masterport;
    redisClient *master;
    int replstate;
    /* Sort parameters - qsort_r() is only available under BSD so we
     * have to take this state global, in order to pass it to sortCompare() */
    int sort_desc;
    int sort_alpha;
    int sort_bypattern;
};

typedef void redisCommandProc(redisClient *c);
struct redisCommand {
    char *name;
    redisCommandProc *proc;
    int arity;
    int flags;
};

typedef struct _redisSortObject {
    robj *obj;
    union {
        double score;
        robj *cmpobj;
    } u;
} redisSortObject;

typedef struct _redisSortOperation {
    int type;
    robj *pattern;
} redisSortOperation;

/*
 * 共享的对象
 */
struct sharedObjectsStruct {
    robj *crlf, *ok, *err, *emptybulk, *czero, *cone, *pong, *space,
    *colon, *nullbulk, *nullmultibulk,
    *emptymultibulk, *wrongtypeerr, *nokeyerr, *syntaxerr, *sameobjecterr,
    *outofrangeerr, *plus,
    *select0, *select1, *select2, *select3, *select4,
    *select5, *select6, *select7, *select8, *select9;
} shared;

/*================================ Prototypes =============================== */

static void freeStringObject(robj *o);
static void freeListObject(robj *o);
static void freeSetObject(robj *o);
static void decrRefCount(void *o);
static robj *createObject(int type, void *ptr);
static void freeClient(redisClient *c);
static int rdbLoad(char *filename);
static void addReply(redisClient *c, robj *obj);
static void addReplySds(redisClient *c, sds s);
static void incrRefCount(robj *o);
static int rdbSaveBackground(char *filename);
static robj *createStringObject(char *ptr, size_t len);
static void replicationFeedSlaves(list *slaves, struct redisCommand *cmd, int dictid, robj **argv, int argc);
static int syncWithMaster(void);
static robj *tryObjectSharing(robj *o);
static int removeExpire(redisDb *db, robj *key);
static int expireIfNeeded(redisDb *db, robj *key);
static int deleteIfVolatile(redisDb *db, robj *key);
static int deleteKey(redisDb *db, robj *key);
static time_t getExpire(redisDb *db, robj *key);
static int setExpire(redisDb *db, robj *key, time_t when);

static void authCommand(redisClient *c);
static void pingCommand(redisClient *c);
static void echoCommand(redisClient *c);
static void setCommand(redisClient *c);
static void setnxCommand(redisClient *c);
static void getCommand(redisClient *c);
static void delCommand(redisClient *c);
static void existsCommand(redisClient *c);
static void incrCommand(redisClient *c);
static void decrCommand(redisClient *c);
static void incrbyCommand(redisClient *c);
static void decrbyCommand(redisClient *c);
static void selectCommand(redisClient *c);
static void randomkeyCommand(redisClient *c);
static void keysCommand(redisClient *c);
static void dbsizeCommand(redisClient *c);
static void lastsaveCommand(redisClient *c);
static void saveCommand(redisClient *c);
static void bgsaveCommand(redisClient *c);
static void shutdownCommand(redisClient *c);
static void moveCommand(redisClient *c);
static void renameCommand(redisClient *c);
static void renamenxCommand(redisClient *c);
static void lpushCommand(redisClient *c);
static void rpushCommand(redisClient *c);
static void lpopCommand(redisClient *c);
static void rpopCommand(redisClient *c);
static void llenCommand(redisClient *c);
static void lindexCommand(redisClient *c);
static void lrangeCommand(redisClient *c);
static void ltrimCommand(redisClient *c);
static void typeCommand(redisClient *c);
static void lsetCommand(redisClient *c);
static void saddCommand(redisClient *c);
static void sremCommand(redisClient *c);
static void sismemberCommand(redisClient *c);
static void scardCommand(redisClient *c);
static void sinterCommand(redisClient *c);
static void sinterstoreCommand(redisClient *c);
static void syncCommand(redisClient *c);
static void flushdbCommand(redisClient *c);
static void flushallCommand(redisClient *c);
static void sortCommand(redisClient *c);
static void lremCommand(redisClient *c);
static void infoCommand(redisClient *c);
static void mgetCommand(redisClient *c);
static void monitorCommand(redisClient *c);
static void expireCommand(redisClient *c);

/*================================= Globals ================================= */

/* Global vars */
static struct redisServer server; /* server global state */
static struct redisCommand cmdTable[] = {
    {"get",getCommand,2,REDIS_CMD_INLINE},
    {"set",setCommand,3,REDIS_CMD_BULK},
    {"setnx",setnxCommand,3,REDIS_CMD_BULK},
    {"del",delCommand,2,REDIS_CMD_INLINE},
    {"exists",existsCommand,2,REDIS_CMD_INLINE},
    {"incr",incrCommand,2,REDIS_CMD_INLINE},
    {"decr",decrCommand,2,REDIS_CMD_INLINE},
    {"mget",mgetCommand,-2,REDIS_CMD_INLINE},
    {"rpush",rpushCommand,3,REDIS_CMD_BULK},
    {"lpush",lpushCommand,3,REDIS_CMD_BULK},
    {"rpop",rpopCommand,2,REDIS_CMD_INLINE},
    {"lpop",lpopCommand,2,REDIS_CMD_INLINE},
    {"llen",llenCommand,2,REDIS_CMD_INLINE},
    {"lindex",lindexCommand,3,REDIS_CMD_INLINE},
    {"lset",lsetCommand,4,REDIS_CMD_BULK},
    {"lrange",lrangeCommand,4,REDIS_CMD_INLINE},
    {"ltrim",ltrimCommand,4,REDIS_CMD_INLINE},
    {"lrem",lremCommand,4,REDIS_CMD_BULK},
    {"sadd",saddCommand,3,REDIS_CMD_BULK},
    {"srem",sremCommand,3,REDIS_CMD_BULK},
    {"sismember",sismemberCommand,3,REDIS_CMD_BULK},
    {"scard",scardCommand,2,REDIS_CMD_INLINE},
    {"sinter",sinterCommand,-2,REDIS_CMD_INLINE},
    {"sinterstore",sinterstoreCommand,-3,REDIS_CMD_INLINE},
    {"smembers",sinterCommand,2,REDIS_CMD_INLINE},
    {"incrby",incrbyCommand,3,REDIS_CMD_INLINE},
    {"decrby",decrbyCommand,3,REDIS_CMD_INLINE},
    {"randomkey",randomkeyCommand,1,REDIS_CMD_INLINE},
    {"select",selectCommand,2,REDIS_CMD_INLINE},
    {"move",moveCommand,3,REDIS_CMD_INLINE},
    {"rename",renameCommand,3,REDIS_CMD_INLINE},
    {"renamenx",renamenxCommand,3,REDIS_CMD_INLINE},
    {"keys",keysCommand,2,REDIS_CMD_INLINE},
    {"dbsize",dbsizeCommand,1,REDIS_CMD_INLINE},
    {"auth",authCommand,2,REDIS_CMD_INLINE},
    {"ping",pingCommand,1,REDIS_CMD_INLINE},
    {"echo",echoCommand,2,REDIS_CMD_BULK},
    {"save",saveCommand,1,REDIS_CMD_INLINE},
    {"bgsave",bgsaveCommand,1,REDIS_CMD_INLINE},
    {"shutdown",shutdownCommand,1,REDIS_CMD_INLINE},
    {"lastsave",lastsaveCommand,1,REDIS_CMD_INLINE},
    {"type",typeCommand,2,REDIS_CMD_INLINE},
    {"sync",syncCommand,1,REDIS_CMD_INLINE},
    {"flushdb",flushdbCommand,1,REDIS_CMD_INLINE},
    {"flushall",flushallCommand,1,REDIS_CMD_INLINE},
    {"sort",sortCommand,-2,REDIS_CMD_INLINE},
    {"info",infoCommand,1,REDIS_CMD_INLINE},
    {"monitor",monitorCommand,1,REDIS_CMD_INLINE},
    {"expire",expireCommand,3,REDIS_CMD_INLINE},
    {NULL,NULL,0,0}
};

/*============================ Utility functions ============================ */

/* Glob-style pattern matching. */
int stringmatchlen(const char *pattern, int patternLen,
        const char *string, int stringLen, int nocase)
{
    while(patternLen) {
        switch(pattern[0]) {
        case '*':
            while (pattern[1] == '*') {
                pattern++;
                patternLen--;
            }
            if (patternLen == 1)
                return 1; /* match */
            while(stringLen) {
                if (stringmatchlen(pattern+1, patternLen-1,
                            string, stringLen, nocase))
                    return 1; /* match */
                string++;
                stringLen--;
            }
            return 0; /* no match */
            break;
        case '?':
            if (stringLen == 0)
                return 0; /* no match */
            string++;
            stringLen--;
            break;
        case '[':
        {
            int not, match;

            pattern++;
            patternLen--;
            not = pattern[0] == '^';
            if (not) {
                pattern++;
                patternLen--;
            }
            match = 0;
            while(1) {
                if (pattern[0] == '\\') {
                    pattern++;
                    patternLen--;
                    if (pattern[0] == string[0])
                        match = 1;
                } else if (pattern[0] == ']') {
                    break;
                } else if (patternLen == 0) {
                    pattern--;
                    patternLen++;
                    break;
                } else if (pattern[1] == '-' && patternLen >= 3) {
                    int start = pattern[0];
                    int end = pattern[2];
                    int c = string[0];
                    if (start > end) {
                        int t = start;
                        start = end;
                        end = t;
                    }
                    if (nocase) {
                        start = tolower(start);
                        end = tolower(end);
                        c = tolower(c);
                    }
                    pattern += 2;
                    patternLen -= 2;
                    if (c >= start && c <= end)
                        match = 1;
                } else {
                    if (!nocase) {
                        if (pattern[0] == string[0])
                            match = 1;
                    } else {
                        if (tolower((int)pattern[0]) == tolower((int)string[0]))
                            match = 1;
                    }
                }
                pattern++;
                patternLen--;
            }
            if (not)
                match = !match;
            if (!match)
                return 0; /* no match */
            string++;
            stringLen--;
            break;
        }
        case '\\':
            if (patternLen >= 2) {
                pattern++;
                patternLen--;
            }
            /* fall through */
        default:
            if (!nocase) {
                if (pattern[0] != string[0])
                    return 0; /* no match */
            } else {
                if (tolower((int)pattern[0]) != tolower((int)string[0]))
                    return 0; /* no match */
            }
            string++;
            stringLen--;
            break;
        }
        pattern++;
        patternLen--;
        if (stringLen == 0) {
            while(*pattern == '*') {
                pattern++;
                patternLen--;
            }
            break;
        }
    }
    if (patternLen == 0 && stringLen == 0)
        return 1;
    return 0;
}

/*
 * 1. 记录 redis 运行日志
 * 2. 该函数不确定有多少参数， 但是有两个是可以确定的：
 *    a. level: 日志的级别
 *    b. fmt: 日志字符串
 *    c. ...: 为不确定部分， 这部分需要在 va_start 和 va_end 之间进行解析
 */
void redisLog(int level, const char *fmt, ...)
{
    va_list ap;
    FILE *fp;

    // 确认 server.logfile 是否为 NULL， 为 NULL 时直接输出到标准输出 stdout
    // 否则以追加的形式添加到 logfile 中
	fp = (server.logfile == NULL) ? stdout : fopen(server.logfile,"a");

	// 此处是为了拿到日志文件的文件描述符， 只有上一个步骤中 logfile 为非空情况下，
	// 才能拿到 fp， 否则的话， 直接输出到 stdout 中， 并在此处返回， 就没有后面的
	// 步骤了
	if (!fp) return;

	// 开始解析其余的参数， 其他不确定参数是在 fmt 参数后面
    va_start(ap, fmt);
    if (level >= server.verbosity) {   // server.verbosity 这个配置还没完全弄明白
        char *c = ".-*";
        fprintf(fp,"%c ",c[level]);    // 将格式化字符串输出到 fp 文件中
        // 会根据参数 fmt 字符串来转换并格式化数据，然后将结果输出到参数 stream 
        // 指定的文件中，直到出现字符串结束（'\0'）为止
		vfprintf(fp, fmt, ap);         
        fprintf(fp,"\n");              // 换行
		// 刷新流 stream 的输出缓冲区
		fflush(fp);
    }
    va_end(ap);

	// 记录日志完成后关闭这个文件流
    if (server.logfile) fclose(fp);
}

/*====================== Hash table type implementation  ==================== */

/* This is an hash table type that uses the SDS dynamic strings libary as
 * keys and radis objects as values (objects can hold SDS strings,
 * lists, sets). */

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

static void dictRedisObjectDestructor(void *privdata, void *val)
{
    DICT_NOTUSED(privdata);

    decrRefCount(val);
}

static int dictSdsKeyCompare(void *privdata, const void *key1,
        const void *key2)
{
    const robj *o1 = key1, *o2 = key2;
    return sdsDictKeyCompare(privdata,o1->ptr,o2->ptr);
}

static unsigned int dictSdsHash(const void *key) {
    const robj *o = key;
    return dictGenHashFunction(o->ptr, sdslen((sds)o->ptr));
}

static dictType setDictType = {
    dictSdsHash,               /* hash function */
    NULL,                      /* key dup */
    NULL,                      /* val dup */
    dictSdsKeyCompare,         /* key compare */
    dictRedisObjectDestructor, /* key destructor */
    NULL                       /* val destructor */
};

static dictType hashDictType = {
    dictSdsHash,                /* hash function */
    NULL,                       /* key dup */
    NULL,                       /* val dup */
    dictSdsKeyCompare,          /* key compare */
    dictRedisObjectDestructor,  /* key destructor */
    dictRedisObjectDestructor   /* val destructor */
};

/* ========================= Random utility functions ======================= */

/* Redis generally does not try to recover from out of memory conditions
 * when allocating objects or strings, it is not clear if it will be possible
 * to report this condition to the client since the networking layer itself
 * is based on heap allocation for send buffers, so we simply abort.
 * At least the code will be simpler to read... */
static void oom(const char *msg) {
    fprintf(stderr, "%s: Out of memory\n",msg);
    fflush(stderr);
    sleep(1);
    abort();
}

/* ====================== Redis server networking stuff ===================== */
void closeTimedoutClients(void) {
    redisClient *c;
    listIter *li;
    listNode *ln;
    time_t now = time(NULL);

    li = listGetIterator(server.clients,AL_START_HEAD);
    if (!li) return;
    while ((ln = listNextElement(li)) != NULL) {
        c = listNodeValue(ln);
        if (!(c->flags & REDIS_SLAVE) &&    /* no timeout for slaves */
             (now - c->lastinteraction > server.maxidletime)) {
            redisLog(REDIS_DEBUG,"Closing idle client");
            freeClient(c);
        }
    }
    listReleaseIterator(li);
}

int serverCron(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    int j, loops = server.cronloops++;
    REDIS_NOTUSED(eventLoop);
    REDIS_NOTUSED(id);
    REDIS_NOTUSED(clientData);

    /* Update the global state with the amount of used memory */
    server.usedmemory = zmalloc_used_memory();

    /* If the percentage of used slots in the HT reaches REDIS_HT_MINFILL
     * we resize the hash table to save memory */
    for (j = 0; j < server.dbnum; j++) {
        int size, used, vkeys;

        size = dictSlots(server.db[j].dict);
        used = dictSize(server.db[j].dict);
        vkeys = dictSize(server.db[j].expires);
        if (!(loops % 5) && used > 0) {
            redisLog(REDIS_DEBUG,"DB %d: %d keys (%d volatile) in %d slots HT.",j,used,vkeys,size);
            /* dictPrintStats(server.dict); */
        }
        if (size && used && size > REDIS_HT_MINSLOTS &&
            (used*100/size < REDIS_HT_MINFILL)) {
            redisLog(REDIS_NOTICE,"The hash table %d is too sparse, resize it...",j);
            dictResize(server.db[j].dict);
            redisLog(REDIS_NOTICE,"Hash table %d resized.",j);
        }
    }

    /* Show information about connected clients */
    if (!(loops % 5)) {
        redisLog(REDIS_DEBUG,"%d clients connected (%d slaves), %zu bytes in use",
            listLength(server.clients)-listLength(server.slaves),
            listLength(server.slaves),
            server.usedmemory,
            dictSize(server.sharingpool));
    }

    /* Close connections of timedout clients */
    if (!(loops % 10))
        closeTimedoutClients();

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
                rdbSaveBackground(server.dbfilename);
                break;
            }
         }
    }

    /* Try to expire a few timed out keys */
    for (j = 0; j < server.dbnum; j++) {
        redisDb *db = server.db+j;
        int num = dictSize(db->expires);

        if (num) {
            time_t now = time(NULL);

            if (num > REDIS_EXPIRELOOKUPS_PER_CRON)
                num = REDIS_EXPIRELOOKUPS_PER_CRON;
            while (num--) {
                dictEntry *de;
                time_t t;

                if ((de = dictGetRandomKey(db->expires)) == NULL) break;
                t = (time_t) dictGetEntryVal(de);
                if (now > t) {
                    deleteKey(db,dictGetEntryKey(de));
                }
            }
        }
    }

    /* Check if we should connect to a MASTER */
    if (server.replstate == REDIS_REPL_CONNECT) {
        redisLog(REDIS_NOTICE,"Connecting to MASTER...");
        if (syncWithMaster() == REDIS_OK) {
            redisLog(REDIS_NOTICE,"MASTER <-> SLAVE sync succeeded");
        }
    }
    return 1000;
}

/*
 * 创建一些共享的全局对象
 */
static void createSharedObjects(void) {
    shared.crlf = createObject(REDIS_STRING,sdsnew("\r\n"));
    shared.ok = createObject(REDIS_STRING,sdsnew("+OK\r\n"));
    shared.err = createObject(REDIS_STRING,sdsnew("-ERR\r\n"));
    shared.emptybulk = createObject(REDIS_STRING,sdsnew("$0\r\n\r\n"));
    shared.czero = createObject(REDIS_STRING,sdsnew(":0\r\n"));
    shared.cone = createObject(REDIS_STRING,sdsnew(":1\r\n"));
    shared.nullbulk = createObject(REDIS_STRING,sdsnew("$-1\r\n"));
    shared.nullmultibulk = createObject(REDIS_STRING,sdsnew("*-1\r\n"));
    shared.emptymultibulk = createObject(REDIS_STRING,sdsnew("*0\r\n"));
    /* no such key */
    shared.pong = createObject(REDIS_STRING,sdsnew("+PONG\r\n"));
    shared.wrongtypeerr = createObject(REDIS_STRING,sdsnew(
        "-ERR Operation against a key holding the wrong kind of value\r\n"));
    shared.nokeyerr = createObject(REDIS_STRING,sdsnew(
        "-ERR no such key\r\n"));
    shared.syntaxerr = createObject(REDIS_STRING,sdsnew(
        "-ERR syntax error\r\n"));
    shared.sameobjecterr = createObject(REDIS_STRING,sdsnew(
        "-ERR source and destination objects are the same\r\n"));
    shared.outofrangeerr = createObject(REDIS_STRING,sdsnew(
        "-ERR index out of range\r\n"));
    shared.space = createObject(REDIS_STRING,sdsnew(" "));
    shared.colon = createObject(REDIS_STRING,sdsnew(":"));
    shared.plus = createObject(REDIS_STRING,sdsnew("+"));
    shared.select0 = createStringObject("select 0\r\n",10);
    shared.select1 = createStringObject("select 1\r\n",10);
    shared.select2 = createStringObject("select 2\r\n",10);
    shared.select3 = createStringObject("select 3\r\n",10);
    shared.select4 = createStringObject("select 4\r\n",10);
    shared.select5 = createStringObject("select 5\r\n",10);
    shared.select6 = createStringObject("select 6\r\n",10);
    shared.select7 = createStringObject("select 7\r\n",10);
    shared.select8 = createStringObject("select 8\r\n",10);
    shared.select9 = createStringObject("select 9\r\n",10);
}

/*
 * 初步观察， 该静态函数是保存了一些数据， 首先分配了 saveparam 这个结构体占用的空
 * 间乘上 server.saveparamslen + 1 
 * todo 需要后续阅读理解
 */
static void appendServerSaveParams(time_t seconds, int changes) {
    server.saveparams = zrealloc(server.saveparams,sizeof(struct saveparam)*(server.saveparamslen+1));
	// 当 server.saveparams 为 NULL 时， 报错内存不足
	if (server.saveparams == NULL) oom("appendServerSaveParams");
	// 将 second 和 changes 储存
	server.saveparams[server.saveparamslen].seconds = seconds;
    server.saveparams[server.saveparamslen].changes = changes;
	// 上述存储完毕后， 将其长度自增加一
	server.saveparamslen++;
}

/*
 * 1. 静态函数， 只能在此文件中使用
 * 2. 重置 server.saveparams， 将 server.saveparams 的值置为 NULL， server.saveparamslen
 *    长度 置为 0
 */
static void ResetServerSaveParams() {
    zfree(server.saveparams);
    server.saveparams = NULL;
    server.saveparamslen = 0;
}

// 静态函数， 只能在此文件中使用
static void initServerConfig() {
    server.dbnum = REDIS_DEFAULT_DBNUM; // redis server 的默认数据库数量 16
    server.port = REDIS_SERVERPORT;     // redis server 的默认端口 6379
    server.verbosity = REDIS_DEBUG;     // redis server 日志等级
    server.maxidletime = REDIS_MAXIDLETIME; // redis server 连接默认超时时间
    server.saveparams = NULL;           // redis server
    server.logfile = NULL; /* NULL = log on standard output */
    server.bindaddr = NULL;             // redis server 绑定的地址
    server.glueoutputbuf = 1;           // 输出缓冲
    server.daemonize = 0;               // 守护进程
    server.pidfile = "/var/run/redis.pid"; // 进程 pid 文件地址
    server.dbfilename = "dump.rdb";     // 数据文件
    server.requirepass = NULL;
    server.shareobjects = 0;            // 共享对象初始为 0
    ResetServerSaveParams();            // 重置 server.saveparams 和 server.saveparamslen

    appendServerSaveParams(60*60,1);  /* save after 1 hour and 1 change */
    appendServerSaveParams(300,100);  /* save after 5 minutes and 100 changes */
    appendServerSaveParams(60,10000); /* save after 1 minute and 10000 changes */
    /* Replication related */
    server.isslave = 0;                 // 是否是从机
    server.masterhost = NULL;           // 主机的 host
    server.masterport = 6379;           // 主机的端口
    server.master = NULL;               //
    server.replstate = REDIS_REPL_NONE; //
}

/*
 * 1. 初始化 Server
 * 2. server 在本文件中是一个全局变量， 因此在这里初始化后， 其他地方用到就是已经被
 *    初始化后的值
 */
static void initServer() {
    int j;

    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    server.clients = listCreate();
    server.slaves = listCreate();
    server.monitors = listCreate();
    server.objfreelist = listCreate();
	// 创建共享对象 
    createSharedObjects();
	// 初始化事件循环
    server.el = aeCreateEventLoop();
	// 分配内存， 其大小是单个 redisDb 的大小乘 dbnum (数据库的数量)
	server.db = zmalloc(sizeof(redisDb)*server.dbnum);
	// 创建共享池
    server.sharingpool = dictCreate(&setDictType,NULL);
    server.sharingpoolsize = 1024;
	// 当 db， client, slaves， monitors， el 和 objfreelist 中任何一个为假， 报内存溢出异常
    if (!server.db || !server.clients || !server.slaves || !server.monitors || !server.el || !server.objfreelist)
        oom("server initialization"); /* Fatal OOM */
	// 获取并设置 server 的文件描述符
    server.fd = anetTcpServer(server.neterr, server.port, server.bindaddr);
    if (server.fd == -1) {
		// 文件描述符为 -1 时， 记录错误日志并退出程序
        redisLog(REDIS_WARNING, "Opening TCP port: %s", server.neterr);
        exit(1);
    }
	// 当有多个 db 时， 循环初始化 db 相关的数据
    for (j = 0; j < server.dbnum; j++) {
        server.db[j].dict = dictCreate(&hashDictType,NULL);
        server.db[j].expires = dictCreate(&setDictType,NULL);
        server.db[j].id = j;
    }
    server.cronloops = 0;
    server.bgsaveinprogress = 0;
    server.lastsave = time(NULL);
    server.dirty = 0;
    server.usedmemory = 0;
    server.stat_numcommands = 0;
    server.stat_numconnections = 0;
    server.stat_starttime = time(NULL);
	// 创建一个时间事件
    aeCreateTimeEvent(server.el, 1000, serverCron, NULL, NULL);
}

/* Empty the whole database */
static void emptyDb() {
    int j;

    for (j = 0; j < server.dbnum; j++) {
        dictEmpty(server.db[j].dict);
        dictEmpty(server.db[j].expires);
    }
}

/* I agree, this is a very rudimental way to load a configuration...
   will improve later if the config gets more complex */
/*
 * 1. 静态函数， 只能在此文件中使用
 * 2. 加载给定路径的配置文件， 解析配置文件， 并将配置设置给 server
 */
static void loadServerConfig(char *filename) {
	// 打开文件流， filename 是文件名称， 应该是带有路径的
	FILE *fp = fopen(filename,"r");
    char buf[REDIS_CONFIGLINE_MAX+1], *err = NULL;
    int linenum = 0;
    sds line = NULL;

	// 当 fp 文件句柄为假时， 说明没有正常打开配置文件， 将错误信息写入日志并退出程序
    if (!fp) {
        redisLog(REDIS_WARNING,"Fatal error, can't open config file");
        exit(1);
    }
	/*
	 * 1. 循环读取配置文件， 判断配置文件中的相关属性并赋值给 server 
	 * 2. C 库函数 char *fgets(char *str, int n, FILE *stream) 从指定的流 stream fp 
	 *    中读取一行， 并把它存储在 str(buf) 所指向的字符串内。当读取 (n-1) 即 
	 *    REDIS_CONFIGLINE_MAX 个字符时，或者读取到换行符时，或者到达文件末尾时，
	 *    它会停止，具体视情况而定。
	 * 3. 如果成功，该函数返回相同的 str 参数。 如果到达文件末尾或者没有读取到任何
	 *    字符， str 的内容保持不变，并返回一个空指针。
	 *    如果发生错误，返回一个空指针。
	 */
    while(fgets(buf,REDIS_CONFIGLINE_MAX+1,fp) != NULL) {  //不为 NULL， 说明读到内容了
        sds *argv;
        int argc, j;

        linenum++;
        line = sdsnew(buf);

		// 去除首尾的 " \t\r\n"
        line = sdstrim(line," \t\r\n");

        /* Skip comments and blank lines*/
        if (line[0] == '#' || line[0] == '\0') {
            sdsfree(line);
            continue;
        }

        /* Split into arguments */
		// 以空格进行拆分
        argv = sdssplitlen(line,sdslen(line)," ",1,&argc); 
        sdstolower(argv[0]);

        /* Execute config directives */
        if (!strcmp(argv[0],"timeout") && argc == 2) {
			// 1. C 库函数 int atoi(const char *str) 把参数 str 所指向的字符串转
			//    换为一个整数（类型为 int 型）。 该函数返回转换后的长整数，如果
			//    没有执行有效的转换，则返回零。
            server.maxidletime = atoi(argv[1]);
            if (server.maxidletime < 1) {
                err = "Invalid timeout value"; goto loaderr;
            }
        } else if (!strcmp(argv[0],"port") && argc == 2) {
            server.port = atoi(argv[1]);
			// 端口的返回是 1-65535
            if (server.port < 1 || server.port > 65535) {
                err = "Invalid port"; goto loaderr;
            }
        } else if (!strcmp(argv[0],"bind") && argc == 2) {
            server.bindaddr = zstrdup(argv[1]);
        } else if (!strcmp(argv[0],"save") && argc == 3) {
            int seconds = atoi(argv[1]);
            int changes = atoi(argv[2]);
            if (seconds < 1 || changes < 0) {
                err = "Invalid save parameters"; goto loaderr;
            }
            appendServerSaveParams(seconds,changes);
        } else if (!strcmp(argv[0],"dir") && argc == 2) {
            if (chdir(argv[1]) == -1) {
                redisLog(REDIS_WARNING,"Can't chdir to '%s': %s",
                    argv[1], strerror(errno));
                exit(1);
            }
        } else if (!strcmp(argv[0],"loglevel") && argc == 2) {
            if (!strcmp(argv[1],"debug")) server.verbosity = REDIS_DEBUG;
            else if (!strcmp(argv[1],"notice")) server.verbosity = REDIS_NOTICE;
            else if (!strcmp(argv[1],"warning")) server.verbosity = REDIS_WARNING;
            else {
                err = "Invalid log level. Must be one of debug, notice, warning";
                goto loaderr;
            }
        } else if (!strcmp(argv[0],"logfile") && argc == 2) {
            FILE *fp;

            server.logfile = zstrdup(argv[1]);
            if (!strcmp(server.logfile,"stdout")) {
                zfree(server.logfile);
                server.logfile = NULL;
            }
            if (server.logfile) {
                /* Test if we are able to open the file. The server will not
                 * be able to abort just for this problem later... */
                fp = fopen(server.logfile,"a");
                if (fp == NULL) {
                    err = sdscatprintf(sdsempty(),
                        "Can't open the log file: %s", strerror(errno));
                    goto loaderr;
                }
                fclose(fp);
            }
        } else if (!strcmp(argv[0],"databases") && argc == 2) {
            server.dbnum = atoi(argv[1]);
            if (server.dbnum < 1) {
                err = "Invalid number of databases"; goto loaderr;
            }
        } else if (!strcmp(argv[0],"slaveof") && argc == 3) {
            server.masterhost = sdsnew(argv[1]);
            server.masterport = atoi(argv[2]);
            server.replstate = REDIS_REPL_CONNECT;
        } else if (!strcmp(argv[0],"glueoutputbuf") && argc == 2) {
            sdstolower(argv[1]);
            if (!strcmp(argv[1],"yes")) server.glueoutputbuf = 1;
            else if (!strcmp(argv[1],"no")) server.glueoutputbuf = 0;
            else {
                err = "argument must be 'yes' or 'no'"; goto loaderr;
            }
        } else if (!strcmp(argv[0],"shareobjects") && argc == 2) {
            sdstolower(argv[1]);
            if (!strcmp(argv[1],"yes")) server.shareobjects = 1;
            else if (!strcmp(argv[1],"no")) server.shareobjects = 0;
            else {
                err = "argument must be 'yes' or 'no'"; goto loaderr;
            }
        } else if (!strcmp(argv[0],"daemonize") && argc == 2) {
            sdstolower(argv[1]);
            if (!strcmp(argv[1],"yes")) server.daemonize = 1;
            else if (!strcmp(argv[1],"no")) server.daemonize = 0;
            else {
                err = "argument must be 'yes' or 'no'"; goto loaderr;
            }
        } else if (!strcmp(argv[0],"requirepass") && argc == 2) {
          server.requirepass = zstrdup(argv[1]);
        } else if (!strcmp(argv[0],"pidfile") && argc == 2) {
          server.pidfile = zstrdup(argv[1]);
        } else {
            err = "Bad directive or wrong number of arguments"; goto loaderr;
        }
        for (j = 0; j < argc; j++)
            sdsfree(argv[j]);
        zfree(argv);
        sdsfree(line);
    }
    fclose(fp);
    return;

loaderr:
    fprintf(stderr, "\n*** FATAL CONFIG FILE ERROR ***\n");
    fprintf(stderr, "Reading the configuration file, at line %d\n", linenum);
    fprintf(stderr, ">>> '%s'\n", line);
    fprintf(stderr, "%s\n", err);
    exit(1);
}

static void freeClientArgv(redisClient *c) {
    int j;

    for (j = 0; j < c->argc; j++)
        decrRefCount(c->argv[j]);
    c->argc = 0;
}

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
    if (c->flags & REDIS_SLAVE) {
        list *l = (c->flags & REDIS_MONITOR) ? server.monitors : server.slaves;
        ln = listSearchKey(l,c);
        assert(ln != NULL);
        listDelNode(l,ln);
    }
    if (c->flags & REDIS_MASTER) {
        server.master = NULL;
        server.replstate = REDIS_REPL_CONNECT;
    }
    zfree(c);
}

static void glueReplyBuffersIfNeeded(redisClient *c) {
    int totlen = 0;
    listNode *ln = c->reply->head, *next;
    robj *o;

    while(ln) {
        o = ln->value;
        totlen += sdslen(o->ptr);
        ln = ln->next;
        /* This optimization makes more sense if we don't have to copy
         * too much data */
        if (totlen > 1024) return;
    }
    if (totlen > 0) {
        char buf[1024];
        int copylen = 0;

        ln = c->reply->head;
        while(ln) {
            next = ln->next;
            o = ln->value;
            memcpy(buf+copylen,o->ptr,sdslen(o->ptr));
            copylen += sdslen(o->ptr);
            listDelNode(c->reply,ln);
            ln = next;
        }
        /* Now the output buffer is empty, add the new single element */
        addReplySds(c,sdsnewlen(buf,totlen));
    }
}

static void sendReplyToClient(aeEventLoop *el, int fd, void *privdata, int mask) {
    redisClient *c = privdata;
    int nwritten = 0, totwritten = 0, objlen;
    robj *o;
    REDIS_NOTUSED(el);
    REDIS_NOTUSED(mask);

    if (server.glueoutputbuf && listLength(c->reply) > 1)
        glueReplyBuffersIfNeeded(c);
    while(listLength(c->reply)) {
        o = listNodeValue(listFirst(c->reply));
        objlen = sdslen(o->ptr);

        if (objlen == 0) {
            listDelNode(c->reply,listFirst(c->reply));
            continue;
        }

        if (c->flags & REDIS_MASTER) {
            nwritten = objlen - c->sentlen;
        } else {
            nwritten = write(fd, ((char*)o->ptr)+c->sentlen, objlen - c->sentlen);
            if (nwritten <= 0) break;
        }
        c->sentlen += nwritten;
        totwritten += nwritten;
        /* If we fully sent the object on head go to the next one */
        if (c->sentlen == objlen) {
            listDelNode(c->reply,listFirst(c->reply));
            c->sentlen = 0;
        }
    }
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
    if (totwritten > 0) c->lastinteraction = time(NULL);
    if (listLength(c->reply) == 0) {
        c->sentlen = 0;
        aeDeleteFileEvent(server.el,c->fd,AE_WRITABLE);
    }
}

static struct redisCommand *lookupCommand(char *name) {
    int j = 0;
    while(cmdTable[j].name != NULL) {
        if (!strcmp(name,cmdTable[j].name)) return &cmdTable[j];
        j++;
    }
    return NULL;
}

/* resetClient prepare the client to process the next command */
static void resetClient(redisClient *c) {
    freeClientArgv(c);
    c->bulklen = -1;
}

/* If this function gets called we already read a whole
 * command, argments are in the client argv/argc fields.
 * processCommand() execute the command or prepare the
 * server for a bulk read from the client.
 *
 * If 1 is returned the client is still alive and valid and
 * and other operations can be performed by the caller. Otherwise
 * if 0 is returned the client was destroied (i.e. after QUIT). */
static int processCommand(redisClient *c) {
    struct redisCommand *cmd;
    long long dirty;

    sdstolower(c->argv[0]->ptr);
    /* The QUIT command is handled as a special case. Normal command
     * procs are unable to close the client connection safely */
    if (!strcmp(c->argv[0]->ptr,"quit")) {
        freeClient(c);
        return 0;
    }
    cmd = lookupCommand(c->argv[0]->ptr);
    if (!cmd) {
        addReplySds(c,sdsnew("-ERR unknown command\r\n"));
        resetClient(c);
        return 1;
    } else if ((cmd->arity > 0 && cmd->arity != c->argc) ||
               (c->argc < -cmd->arity)) {
        addReplySds(c,sdsnew("-ERR wrong number of arguments\r\n"));
        resetClient(c);
        return 1;
    } else if (cmd->flags & REDIS_CMD_BULK && c->bulklen == -1) {
        int bulklen = atoi(c->argv[c->argc-1]->ptr);

        decrRefCount(c->argv[c->argc-1]);
        if (bulklen < 0 || bulklen > 1024*1024*1024) {
            c->argc--;
            addReplySds(c,sdsnew("-ERR invalid bulk write count\r\n"));
            resetClient(c);
            return 1;
        }
        c->argc--;
        c->bulklen = bulklen+2; /* add two bytes for CR+LF */
        /* It is possible that the bulk read is already in the
         * buffer. Check this condition and handle it accordingly */
        if ((signed)sdslen(c->querybuf) >= c->bulklen) {
            c->argv[c->argc] = createStringObject(c->querybuf,c->bulklen-2);
            c->argc++;
            c->querybuf = sdsrange(c->querybuf,c->bulklen,-1);
        } else {
            return 1;
        }
    }
    /* Let's try to share objects on the command arguments vector */
    if (server.shareobjects) {
        int j;
        for(j = 1; j < c->argc; j++)
            c->argv[j] = tryObjectSharing(c->argv[j]);
    }
    /* Check if the user is authenticated */
    if (server.requirepass && !c->authenticated && cmd->proc != authCommand) {
        addReplySds(c,sdsnew("-ERR operation not permitted\r\n"));
        resetClient(c);
        return 1;
    }

    /* Exec the command */
    dirty = server.dirty;
    cmd->proc(c);
    if (server.dirty-dirty != 0 && listLength(server.slaves))
        replicationFeedSlaves(server.slaves,cmd,c->db->id,c->argv,c->argc);
    if (listLength(server.monitors))
        replicationFeedSlaves(server.monitors,cmd,c->db->id,c->argv,c->argc);
    server.stat_numcommands++;

    /* Prepare the client for the next command */
    if (c->flags & REDIS_CLOSE) {
        freeClient(c);
        return 0;
    }
    resetClient(c);
    return 1;
}

static void replicationFeedSlaves(list *slaves, struct redisCommand *cmd, int dictid, robj **argv, int argc) {
    listNode *ln = slaves->head;
    robj *outv[REDIS_MAX_ARGS*4]; /* enough room for args, spaces, newlines */
    int outc = 0, j;
    
    for (j = 0; j < argc; j++) {
        if (j != 0) outv[outc++] = shared.space;
        if ((cmd->flags & REDIS_CMD_BULK) && j == argc-1) {
            robj *lenobj;

            lenobj = createObject(REDIS_STRING,
                sdscatprintf(sdsempty(),"%d\r\n",sdslen(argv[j]->ptr)));
            lenobj->refcount = 0;
            outv[outc++] = lenobj;
        }
        outv[outc++] = argv[j];
    }
    outv[outc++] = shared.crlf;

    while(ln) {
        redisClient *slave = ln->value;
        if (slave->slaveseldb != dictid) {
            robj *selectcmd;

            switch(dictid) {
            case 0: selectcmd = shared.select0; break;
            case 1: selectcmd = shared.select1; break;
            case 2: selectcmd = shared.select2; break;
            case 3: selectcmd = shared.select3; break;
            case 4: selectcmd = shared.select4; break;
            case 5: selectcmd = shared.select5; break;
            case 6: selectcmd = shared.select6; break;
            case 7: selectcmd = shared.select7; break;
            case 8: selectcmd = shared.select8; break;
            case 9: selectcmd = shared.select9; break;
            default:
                selectcmd = createObject(REDIS_STRING,
                    sdscatprintf(sdsempty(),"select %d\r\n",dictid));
                selectcmd->refcount = 0;
                break;
            }
            addReply(slave,selectcmd);
            slave->slaveseldb = dictid;
        }
        for (j = 0; j < outc; j++) addReply(slave,outv[j]);
        ln = ln->next;
    }
}

static void readQueryFromClient(aeEventLoop *el, int fd, void *privdata, int mask) {
    redisClient *c = (redisClient*) privdata;
    char buf[REDIS_QUERYBUF_LEN];
    int nread;
    REDIS_NOTUSED(el);
    REDIS_NOTUSED(mask);

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
        redisLog(REDIS_DEBUG, "Client closed connection");
        freeClient(c);
        return;
    }
    if (nread) {
        c->querybuf = sdscatlen(c->querybuf, buf, nread);
        c->lastinteraction = time(NULL);
    } else {
        return;
    }

again:
    if (c->bulklen == -1) {
        /* Read the first line of the query */
        char *p = strchr(c->querybuf,'\n');
        size_t querylen;
        if (p) {
            sds query, *argv;
            int argc, j;
            
            query = c->querybuf;
            c->querybuf = sdsempty();
            querylen = 1+(p-(query));
            if (sdslen(query) > querylen) {
                /* leave data after the first line of the query in the buffer */
                c->querybuf = sdscatlen(c->querybuf,query+querylen,sdslen(query)-querylen);
            }
            *p = '\0'; /* remove "\n" */
            if (*(p-1) == '\r') *(p-1) = '\0'; /* and "\r" if any */
            sdsupdatelen(query);

            /* Now we can split the query in arguments */
            if (sdslen(query) == 0) {
                /* Ignore empty query */
                sdsfree(query);
                return;
            }
            argv = sdssplitlen(query,sdslen(query)," ",1,&argc);
            sdsfree(query);
            if (argv == NULL) oom("sdssplitlen");
            for (j = 0; j < argc && j < REDIS_MAX_ARGS; j++) {
                if (sdslen(argv[j])) {
                    c->argv[c->argc] = createObject(REDIS_STRING,argv[j]);
                    c->argc++;
                } else {
                    sdsfree(argv[j]);
                }
            }
            zfree(argv);
            /* Execute the command. If the client is still valid
             * after processCommand() return and there is something
             * on the query buffer try to process the next command. */
            if (processCommand(c) && sdslen(c->querybuf)) goto again;
            return;
        } else if (sdslen(c->querybuf) >= 1024) {
            redisLog(REDIS_DEBUG, "Client protocol error");
            freeClient(c);
            return;
        }
    } else {
        /* Bulk read handling. Note that if we are at this point
           the client already sent a command terminated with a newline,
           we are reading the bulk data that is actually the last
           argument of the command. */
        int qbl = sdslen(c->querybuf);

        if (c->bulklen <= qbl) {
            /* Copy everything but the final CRLF as final argument */
            c->argv[c->argc] = createStringObject(c->querybuf,c->bulklen-2);
            c->argc++;
            c->querybuf = sdsrange(c->querybuf,c->bulklen,-1);
            processCommand(c);
            return;
        }
    }
}

static int selectDb(redisClient *c, int id) {
    if (id < 0 || id >= server.dbnum)
        return REDIS_ERR;
    c->db = &server.db[id];
    return REDIS_OK;
}

static redisClient *createClient(int fd) {
    redisClient *c = zmalloc(sizeof(*c));

    anetNonBlock(NULL,fd);
    anetTcpNoDelay(NULL,fd);
    if (!c) return NULL;
    selectDb(c,0);
    c->fd = fd;
    c->querybuf = sdsempty();
    c->argc = 0;
    c->bulklen = -1;
    c->sentlen = 0;
    c->flags = 0;
    c->lastinteraction = time(NULL);
    c->authenticated = 0;
    if ((c->reply = listCreate()) == NULL) oom("listCreate");
    listSetFreeMethod(c->reply,decrRefCount);
    if (aeCreateFileEvent(server.el, c->fd, AE_READABLE,
        readQueryFromClient, c, NULL) == AE_ERR) {
        freeClient(c);
        return NULL;
    }
    if (!listAddNodeTail(server.clients,c)) oom("listAddNodeTail");
    return c;
}

static void addReply(redisClient *c, robj *obj) {
    if (listLength(c->reply) == 0 &&
        aeCreateFileEvent(server.el, c->fd, AE_WRITABLE,
        sendReplyToClient, c, NULL) == AE_ERR) return;
    if (!listAddNodeTail(c->reply,obj)) oom("listAddNodeTail");
    incrRefCount(obj);
}

static void addReplySds(redisClient *c, sds s) {
    robj *o = createObject(REDIS_STRING,s);
    addReply(c,o);
    decrRefCount(o);
}

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
    if (createClient(cfd) == NULL) {
        redisLog(REDIS_WARNING,"Error allocating resoures for the client");
        close(cfd); /* May be already closed, just ingore errors */
        return;
    }
    server.stat_numconnections++;
}

/* ======================= Redis objects implementation ===================== */

/*
 * 创建 redis 对象
 */
static robj *createObject(int type, void *ptr) {
	// 创建一个 redis 对象 o
	robj *o;

	// 当 server.objfreelist 长度不为空时， 删除一个头结点， 并新建一个 redis 对象
    if (listLength(server.objfreelist)) {
		// 获取 server.objfreelist 中的第一个元素结点
        listNode *head = listFirst(server.objfreelist);
		// 获取第一个元素结点的值
        o = listNodeValue(head);
		// 删除 objfreelist 的头结点
        listDelNode(server.objfreelist,head);
    } else {
		// 当 objfreelist 为空时， 说明没有需要释放的对象， 直接新建对象
        o = zmalloc(sizeof(*o));
    }
	// 当 o 没有值的时候， 说明没有拿到 objfreelist 中值， 也没有分配到内存， 那么
	// 内存不足
    if (!o) oom("createObject");
    o->type = type;  // 创建对象的类型
    o->ptr = ptr;    // 创建对象的指针
    o->refcount = 1; // 新建对象的引用计数， 为 0 将会被删除
    return o;
}

/*
 * 创建 redis string 对象， 参数是 ptr 指针和 len 长度
 */
static robj *createStringObject(char *ptr, size_t len) {
	// 调用 createObject 创建 type 为 REDIS_STRING 指针为 sdsnewlen(ptr,len) 的字符串对象
	// sdsnewlen 新建了一个新的字符串， 长度是 len
    return createObject(REDIS_STRING,sdsnewlen(ptr,len));
}

/*
 * 创建 redis list 对象
 */
static robj *createListObject(void) {
    list *l = listCreate();

    if (!l) oom("listCreate");
	// 将这个创建的 list 的释放方法设置为 decrRefCount
    listSetFreeMethod(l,decrRefCount);
    return createObject(REDIS_LIST,l);
}

/*
 * 创建 redis set 对象
 */
static robj *createSetObject(void) {
    dict *d = dictCreate(&setDictType,NULL);
    if (!d) oom("dictCreate");
    return createObject(REDIS_SET,d);
}

/*
 * 释放字符串对象的内存
 */
static void freeStringObject(robj *o) {
    sdsfree(o->ptr);
}

/*
 * 释放链表对象的内存
 */
static void freeListObject(robj *o) {
    listRelease((list*) o->ptr);
}

/*
 * 释放集合对象的内存
 */
static void freeSetObject(robj *o) {
    dictRelease((dict*) o->ptr);
}

/*
 * 释放 hash 对象的内存
 */
static void freeHashObject(robj *o) {
    dictRelease((dict*) o->ptr);
}

/*
 * 增加 redis 对象的引用计数
 */
static void incrRefCount(robj *o) {
    o->refcount++; // 自增加 1
#ifdef DEBUG_REFCOUNT
    if (o->type == REDIS_STRING)
        printf("Increment '%s'(%p), now is: %d\n",o->ptr,o,o->refcount);
#endif
}

/*
 * 减少 redis 对象的引用计数
 */
static void decrRefCount(void *obj) {
    robj *o = obj;

#ifdef DEBUG_REFCOUNT
    if (o->type == REDIS_STRING)
        printf("Decrement '%s'(%p), now is: %d\n",o->ptr,o,o->refcount-1);
#endif
	// 先减一才等于 0 说明引用计数开始为 1
    if (--(o->refcount) == 0) {
		// 根据 redis 对象的类型执行响应的处理
        switch(o->type) {
        case REDIS_STRING: freeStringObject(o); break;
        case REDIS_LIST: freeListObject(o); break;
        case REDIS_SET: freeSetObject(o); break;
        case REDIS_HASH: freeHashObject(o); break;
        default: assert(0 != 0); break;
        }
        if (listLength(server.objfreelist) > REDIS_OBJFREELIST_MAX ||
            !listAddNodeHead(server.objfreelist,o))
            zfree(o);
    }
}

/* Try to share an object against the shared objects pool */
/*
 * 尝试创建一个共享对象
 */
static robj *tryObjectSharing(robj *o) {
    struct dictEntry *de;
    unsigned long c;

    if (o == NULL || server.shareobjects == 0) return o;

    assert(o->type == REDIS_STRING);
    de = dictFind(server.sharingpool,o);
    if (de) {
        robj *shared = dictGetEntryKey(de);

        c = ((unsigned long) dictGetEntryVal(de))+1;
        dictGetEntryVal(de) = (void*) c;
        incrRefCount(shared);
        decrRefCount(o);
        return shared;
    } else {
        /* Here we are using a stream algorihtm: Every time an object is
         * shared we increment its count, everytime there is a miss we
         * recrement the counter of a random object. If this object reaches
         * zero we remove the object and put the current object instead. */
        if (dictSize(server.sharingpool) >=
                server.sharingpoolsize) {
            de = dictGetRandomKey(server.sharingpool);
            assert(de != NULL);
            c = ((unsigned long) dictGetEntryVal(de))-1;
            dictGetEntryVal(de) = (void*) c;
            if (c == 0) {
                dictDelete(server.sharingpool,de->key);
            }
        } else {
            c = 0; /* If the pool is empty we want to add this object */
        }
        if (c == 0) {
            int retval;

            retval = dictAdd(server.sharingpool,o,(void*)1);
            assert(retval == DICT_OK);
            incrRefCount(o);
        }
        return o;
    }
}

static robj *lookupKey(redisDb *db, robj *key) {
    dictEntry *de = dictFind(db->dict,key);
    return de ? dictGetEntryVal(de) : NULL;
}

static robj *lookupKeyRead(redisDb *db, robj *key) {
    expireIfNeeded(db,key);
    return lookupKey(db,key);
}

static robj *lookupKeyWrite(redisDb *db, robj *key) {
    deleteIfVolatile(db,key);
    return lookupKey(db,key);
}

/*
 * 删除 redis 数据库中的 redis 对象
 */
static int deleteKey(redisDb *db, robj *key) {
    int retval;

    /* We need to protect key from destruction: after the first dictDelete()
     * it may happen that 'key' is no longer valid if we don't increment
     * it's count. This may happen when we get the object reference directly
     * from the hash table with dictRandomKey() or dict iterators */
	// 向对 key 这个对象的引用计数加 1， 防止删除的时候， key 的引用计数变成 0
	incrRefCount(key);

	// expires 字典为非空时， 执行 dictDelete 操作， 将 expires 字典中的 key 对象删除
    if (dictSize(db->expires)) dictDelete(db->expires,key);
	// 在将 redis db 的 dict (此处应该是数据字典)， 将 key 对象删除
    retval = dictDelete(db->dict,key);
	// 将 key 的引用计数减 1
    decrRefCount(key);

    return retval == DICT_OK;
}

/*============================ DB saving/loading ============================ */

static int rdbSaveType(FILE *fp, unsigned char type) {
    if (fwrite(&type,1,1,fp) == 0) return -1;
    return 0;
}

static int rdbSaveTime(FILE *fp, time_t t) {
    int32_t t32 = (int32_t) t;
    if (fwrite(&t32,4,1,fp) == 0) return -1;
    return 0;
}

/* check rdbLoadLen() comments for more info */
static int rdbSaveLen(FILE *fp, uint32_t len) {
    unsigned char buf[2];

    if (len < (1<<6)) {
        /* Save a 6 bit len */
        buf[0] = (len&0xFF)|(REDIS_RDB_6BITLEN<<6);
        if (fwrite(buf,1,1,fp) == 0) return -1;
    } else if (len < (1<<14)) {
        /* Save a 14 bit len */
        buf[0] = ((len>>8)&0xFF)|(REDIS_RDB_14BITLEN<<6);
        buf[1] = len&0xFF;
        if (fwrite(buf,2,1,fp) == 0) return -1;
    } else {
        /* Save a 32 bit len */
        buf[0] = (REDIS_RDB_32BITLEN<<6);
        if (fwrite(buf,1,1,fp) == 0) return -1;
        len = htonl(len);
        if (fwrite(&len,4,1,fp) == 0) return -1;
    }
    return 0;
}

/* String objects in the form "2391" "-100" without any space and with a
 * range of values that can fit in an 8, 16 or 32 bit signed value can be
 * encoded as integers to save space */
int rdbTryIntegerEncoding(sds s, unsigned char *enc) {
    long long value;
    char *endptr, buf[32];

    /* Check if it's possible to encode this value as a number */
    value = strtoll(s, &endptr, 10);
    if (endptr[0] != '\0') return 0;
    snprintf(buf,32,"%lld",value);

    /* If the number converted back into a string is not identical
     * then it's not possible to encode the string as integer */
    if (strlen(buf) != sdslen(s) || memcmp(buf,s,sdslen(s))) return 0;

    /* Finally check if it fits in our ranges */
    if (value >= -(1<<7) && value <= (1<<7)-1) {
        enc[0] = (REDIS_RDB_ENCVAL<<6)|REDIS_RDB_ENC_INT8;
        enc[1] = value&0xFF;
        return 2;
    } else if (value >= -(1<<15) && value <= (1<<15)-1) {
        enc[0] = (REDIS_RDB_ENCVAL<<6)|REDIS_RDB_ENC_INT16;
        enc[1] = value&0xFF;
        enc[2] = (value>>8)&0xFF;
        return 3;
    } else if (value >= -((long long)1<<31) && value <= ((long long)1<<31)-1) {
        enc[0] = (REDIS_RDB_ENCVAL<<6)|REDIS_RDB_ENC_INT32;
        enc[1] = value&0xFF;
        enc[2] = (value>>8)&0xFF;
        enc[3] = (value>>16)&0xFF;
        enc[4] = (value>>24)&0xFF;
        return 5;
    } else {
        return 0;
    }
}

static int rdbSaveLzfStringObject(FILE *fp, robj *obj) {
    unsigned int comprlen, outlen;
    unsigned char byte;
    void *out;

    /* We require at least four bytes compression for this to be worth it */
    outlen = sdslen(obj->ptr)-4;
    if (outlen <= 0) return 0;
    if ((out = zmalloc(outlen)) == NULL) return 0;
    comprlen = lzf_compress(obj->ptr, sdslen(obj->ptr), out, outlen);
    if (comprlen == 0) {
        zfree(out);
        return 0;
    }
    /* Data compressed! Let's save it on disk */
    byte = (REDIS_RDB_ENCVAL<<6)|REDIS_RDB_ENC_LZF;
    if (fwrite(&byte,1,1,fp) == 0) goto writeerr;
    if (rdbSaveLen(fp,comprlen) == -1) goto writeerr;
    if (rdbSaveLen(fp,sdslen(obj->ptr)) == -1) goto writeerr;
    if (fwrite(out,comprlen,1,fp) == 0) goto writeerr;
    zfree(out);
    return comprlen;

writeerr:
    zfree(out);
    return -1;
}

/* Save a string objet as [len][data] on disk. If the object is a string
 * representation of an integer value we try to safe it in a special form */
static int rdbSaveStringObject(FILE *fp, robj *obj) {
    size_t len = sdslen(obj->ptr);
    int enclen;

    /* Try integer encoding */
    if (len <= 11) {
        unsigned char buf[5];
        if ((enclen = rdbTryIntegerEncoding(obj->ptr,buf)) > 0) {
            if (fwrite(buf,enclen,1,fp) == 0) return -1;
            return 0;
        }
    }

    /* Try LZF compression - under 20 bytes it's unable to compress even
     * aaaaaaaaaaaaaaaaaa so skip it */
    if (len > 20) {
        int retval;

        retval = rdbSaveLzfStringObject(fp,obj);
        if (retval == -1) return -1;
        if (retval > 0) return 0;
        /* retval == 0 means data can't be compressed, save the old way */
    }

    /* Store verbatim */
    if (rdbSaveLen(fp,len) == -1) return -1;
    if (len && fwrite(obj->ptr,len,1,fp) == 0) return -1;
    return 0;
}

/* Save the DB on disk. Return REDIS_ERR on error, REDIS_OK on success */
static int rdbSave(char *filename) {
    dictIterator *di = NULL;
    dictEntry *de;
    FILE *fp;
    char tmpfile[256];
    int j;
    time_t now = time(NULL);

    snprintf(tmpfile,256,"temp-%d.%ld.rdb",(int)time(NULL),(long int)random());
    fp = fopen(tmpfile,"w");
    if (!fp) {
        redisLog(REDIS_WARNING, "Failed saving the DB: %s", strerror(errno));
        return REDIS_ERR;
    }
    if (fwrite("REDIS0001",9,1,fp) == 0) goto werr;
    for (j = 0; j < server.dbnum; j++) {
        redisDb *db = server.db+j;
        dict *d = db->dict;
        if (dictSize(d) == 0) continue;
        di = dictGetIterator(d);
        if (!di) {
            fclose(fp);
            return REDIS_ERR;
        }

        /* Write the SELECT DB opcode */
        if (rdbSaveType(fp,REDIS_SELECTDB) == -1) goto werr;
        if (rdbSaveLen(fp,j) == -1) goto werr;

        /* Iterate this DB writing every entry */
        while((de = dictNext(di)) != NULL) {
            robj *key = dictGetEntryKey(de);
            robj *o = dictGetEntryVal(de);
            time_t expiretime = getExpire(db,key);

            /* Save the expire time */
            if (expiretime != -1) {
                /* If this key is already expired skip it */
                if (expiretime < now) continue;
                if (rdbSaveType(fp,REDIS_EXPIRETIME) == -1) goto werr;
                if (rdbSaveTime(fp,expiretime) == -1) goto werr;
            }
            /* Save the key and associated value */
            if (rdbSaveType(fp,o->type) == -1) goto werr;
            if (rdbSaveStringObject(fp,key) == -1) goto werr;
            if (o->type == REDIS_STRING) {
                /* Save a string value */
                if (rdbSaveStringObject(fp,o) == -1) goto werr;
            } else if (o->type == REDIS_LIST) {
                /* Save a list value */
                list *list = o->ptr;
                listNode *ln = list->head;

                if (rdbSaveLen(fp,listLength(list)) == -1) goto werr;
                while(ln) {
                    robj *eleobj = listNodeValue(ln);

                    if (rdbSaveStringObject(fp,eleobj) == -1) goto werr;
                    ln = ln->next;
                }
            } else if (o->type == REDIS_SET) {
                /* Save a set value */
                dict *set = o->ptr;
                dictIterator *di = dictGetIterator(set);
                dictEntry *de;

                if (!set) oom("dictGetIteraotr");
                if (rdbSaveLen(fp,dictSize(set)) == -1) goto werr;
                while((de = dictNext(di)) != NULL) {
                    robj *eleobj = dictGetEntryKey(de);

                    if (rdbSaveStringObject(fp,eleobj) == -1) goto werr;
                }
                dictReleaseIterator(di);
            } else {
                assert(0 != 0);
            }
        }
        dictReleaseIterator(di);
    }
    /* EOF opcode */
    if (rdbSaveType(fp,REDIS_EOF) == -1) goto werr;

    /* Make sure data will not remain on the OS's output buffers */
    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);
    
    /* Use RENAME to make sure the DB file is changed atomically only
     * if the generate DB file is ok. */
    if (rename(tmpfile,filename) == -1) {
        redisLog(REDIS_WARNING,"Error moving temp DB file on the final destionation: %s", strerror(errno));
        unlink(tmpfile);
        return REDIS_ERR;
    }
    redisLog(REDIS_NOTICE,"DB saved on disk");
    server.dirty = 0;
    server.lastsave = time(NULL);
    return REDIS_OK;

werr:
    fclose(fp);
    unlink(tmpfile);
    redisLog(REDIS_WARNING,"Write error saving DB on disk: %s", strerror(errno));
    if (di) dictReleaseIterator(di);
    return REDIS_ERR;
}

static int rdbSaveBackground(char *filename) {
    pid_t childpid;

    if (server.bgsaveinprogress) return REDIS_ERR;
    if ((childpid = fork()) == 0) {
        /* Child */
        close(server.fd);
        if (rdbSave(filename) == REDIS_OK) {
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

/*
 * 从给定的文件流中获取 type
 */
static int rdbLoadType(FILE *fp) {
    unsigned char type;
	// 从 fp 文件流中读取 1 个 1 字节的数据并存储到 type， 最终返回 type(正常情况下) 
    if (fread(&type,1,1,fp) == 0) return -1;
    return type;
}

/*
 * 从给定的文件流中获取 time
 */
static time_t rdbLoadTime(FILE *fp) {
    int32_t t32;
	// 从 fp 文件流中读取 1 个 4 字节的数据并存储到 t32， 最终返回 t32(正常情况下) 
    if (fread(&t32,4,1,fp) == 0) return -1;
    return (time_t) t32;
}

/* Load an encoded length from the DB, see the REDIS_RDB_* defines on the top
 * of this file for a description of how this are stored on disk.
 *
 * isencoded is set to 1 if the readed length is not actually a length but
 * an "encoding type", check the above comments for more info */
/*
 * 从 db 文件中加载长度 len， 有点儿小复杂
 * todo
 */
static uint32_t rdbLoadLen(FILE *fp, int rdbver, int *isencoded) {
    unsigned char buf[2];
    uint32_t len;

    if (isencoded) *isencoded = 0;
    if (rdbver == 0) {
        if (fread(&len,4,1,fp) == 0) return REDIS_RDB_LENERR;
        return ntohl(len);
    } else {
        int type;

        if (fread(buf,1,1,fp) == 0) return REDIS_RDB_LENERR;
        type = (buf[0]&0xC0)>>6;
        if (type == REDIS_RDB_6BITLEN) {
            /* Read a 6 bit len */
            return buf[0]&0x3F;
        } else if (type == REDIS_RDB_ENCVAL) {
            /* Read a 6 bit len encoding type */
            if (isencoded) *isencoded = 1;
            return buf[0]&0x3F;
        } else if (type == REDIS_RDB_14BITLEN) {
            /* Read a 14 bit len */
            if (fread(buf+1,1,1,fp) == 0) return REDIS_RDB_LENERR;
            return ((buf[0]&0x3F)<<8)|buf[1];
        } else {
            /* Read a 32 bit len */
            if (fread(&len,4,1,fp) == 0) return REDIS_RDB_LENERR;
            return ntohl(len);
        }
    }
}

/*
 * 加载整数类型对象
 */
static robj *rdbLoadIntegerObject(FILE *fp, int enctype) {
    unsigned char enc[4];
    long long val;

    if (enctype == REDIS_RDB_ENC_INT8) {
        if (fread(enc,1,1,fp) == 0) return NULL;
        val = (signed char)enc[0];
    } else if (enctype == REDIS_RDB_ENC_INT16) {
        uint16_t v;
        if (fread(enc,2,1,fp) == 0) return NULL;
        v = enc[0]|(enc[1]<<8);
        val = (int16_t)v;
    } else if (enctype == REDIS_RDB_ENC_INT32) {
        uint32_t v;
        if (fread(enc,4,1,fp) == 0) return NULL;
        v = enc[0]|(enc[1]<<8)|(enc[2]<<16)|(enc[3]<<24);
        val = (int32_t)v;
    } else {
        val = 0; /* anti-warning */
        assert(0!=0);
    }
    return createObject(REDIS_STRING,sdscatprintf(sdsempty(),"%lld",val));
}

static robj *rdbLoadLzfStringObject(FILE*fp, int rdbver) {
    unsigned int len, clen;
    unsigned char *c = NULL;
    sds val = NULL;

    if ((clen = rdbLoadLen(fp,rdbver,NULL)) == REDIS_RDB_LENERR) return NULL;
    if ((len = rdbLoadLen(fp,rdbver,NULL)) == REDIS_RDB_LENERR) return NULL;
    if ((c = zmalloc(clen)) == NULL) goto err;
    if ((val = sdsnewlen(NULL,len)) == NULL) goto err;
    if (fread(c,clen,1,fp) == 0) goto err;
    if (lzf_decompress(c,clen,val,len) == 0) goto err;
    return createObject(REDIS_STRING,val);
err:
    zfree(c);
    sdsfree(val);
    return NULL;
}

/*
 * 加载字符串对象
 */
static robj *rdbLoadStringObject(FILE*fp, int rdbver) {
    int isencoded;
    uint32_t len;
    sds val;

    len = rdbLoadLen(fp,rdbver,&isencoded);
    if (isencoded) {
        switch(len) {
        case REDIS_RDB_ENC_INT8:
        case REDIS_RDB_ENC_INT16:
        case REDIS_RDB_ENC_INT32:
            return tryObjectSharing(rdbLoadIntegerObject(fp,len));
        case REDIS_RDB_ENC_LZF:
            return tryObjectSharing(rdbLoadLzfStringObject(fp,rdbver));
        default:
            assert(0!=0);
        }
    }

    if (len == REDIS_RDB_LENERR) return NULL;
    val = sdsnewlen(NULL,len);
    if (len && fread(val,len,1,fp) == 0) {
        sdsfree(val);
        return NULL;
    }
    return tryObjectSharing(createObject(REDIS_STRING,val));
}

/*
 * 加载 redis 数据文件
 */
static int rdbLoad(char *filename) {
    FILE *fp;
    robj *keyobj = NULL;
    uint32_t dbid;
    int type, retval, rdbver;
    dict *d = server.db[0].dict;
    redisDb *db = server.db+0;
    char buf[1024];
    time_t expiretime = -1, now = time(NULL);

	// 打开文件流
    fp = fopen(filename,"r");
    if (!fp) return REDIS_ERR;
	// C 库函数 size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) 
	// 从给定流 stream 读取数据到 ptr 所指向的数组中。成功读取的元素总数会以 
	// size_t 对象返回，size_t 对象是一个整型数据类型。如果总数与 nmemb 参数不同，
	// 则可能发生了一个错误或者到达了文件末尾。
    if (fread(buf,9,1,fp) == 0) goto eoferr;
    buf[9] = '\0';
	// C 库函数 int memcmp(const void *str1, const void *str2, size_t n)) 把存储
	// 区 str1 和存储区 str2 的前 n 个字节进行比较。
	// 如果返回值 < 0，则表示 str1 小于 str2。
	// 如果返回值 > 0，则表示 str1 大于 str2。
	// 如果返回值 = 0，则表示 str1 等于 str2。
    if (memcmp(buf,"REDIS",5) != 0) {
		// 不相等的话， 说明加载的文件不是 redis 数据文件
        fclose(fp);
        redisLog(REDIS_WARNING,"Wrong signature trying to load DB from file");
        return REDIS_ERR;
    }
    rdbver = atoi(buf+5);
    if (rdbver > 1) {
		// rdb 版本不对
        fclose(fp);
        redisLog(REDIS_WARNING,"Can't handle RDB format version %d",rdbver);
        return REDIS_ERR;
    }
    while(1) {
        robj *o;

        /* Read type. */
        if ((type = rdbLoadType(fp)) == -1) goto eoferr;
        if (type == REDIS_EXPIRETIME) {
            if ((expiretime = rdbLoadTime(fp)) == -1) goto eoferr;
            /* We read the time so we need to read the object type again */
            if ((type = rdbLoadType(fp)) == -1) goto eoferr;
        }
        if (type == REDIS_EOF) break;
        /* Handle SELECT DB opcode as a special case */
        if (type == REDIS_SELECTDB) {
            if ((dbid = rdbLoadLen(fp,rdbver,NULL)) == REDIS_RDB_LENERR)
                goto eoferr;
            if (dbid >= (unsigned)server.dbnum) {
                redisLog(REDIS_WARNING,"FATAL: Data file was created with a Redis server configured to handle more than %d databases. Exiting\n", server.dbnum);
                exit(1);
            }
            db = server.db+dbid;
            d = db->dict;
            continue;
        }
        /* Read key */
        if ((keyobj = rdbLoadStringObject(fp,rdbver)) == NULL) goto eoferr;

        if (type == REDIS_STRING) {
            /* Read string value */
            if ((o = rdbLoadStringObject(fp,rdbver)) == NULL) goto eoferr;
        } else if (type == REDIS_LIST || type == REDIS_SET) {
            /* Read list/set value */
            uint32_t listlen;

            if ((listlen = rdbLoadLen(fp,rdbver,NULL)) == REDIS_RDB_LENERR)
                goto eoferr;
            o = (type == REDIS_LIST) ? createListObject() : createSetObject();
            /* Load every single element of the list/set */
            while(listlen--) {
                robj *ele;

                if ((ele = rdbLoadStringObject(fp,rdbver)) == NULL) goto eoferr;
                if (type == REDIS_LIST) {
                    if (!listAddNodeTail((list*)o->ptr,ele))
                        oom("listAddNodeTail");
                } else {
                    if (dictAdd((dict*)o->ptr,ele,NULL) == DICT_ERR)
                        oom("dictAdd");
                }
            }
        } else {
            assert(0 != 0);
        }
        /* Add the new object in the hash table */
        retval = dictAdd(d,keyobj,o);
        if (retval == DICT_ERR) {
            redisLog(REDIS_WARNING,"Loading DB, duplicated key (%s) found! Unrecoverable error, exiting now.", keyobj->ptr);
            exit(1);
        }
        /* Set the expire time if needed */
        if (expiretime != -1) {
            setExpire(db,keyobj,expiretime);
            /* Delete this key if already expired */
            if (expiretime < now) deleteKey(db,keyobj);
            expiretime = -1;
        }
        keyobj = o = NULL;
    }
    fclose(fp);
    return REDIS_OK;

eoferr: /* unexpected end of file is handled here with a fatal exit */
    if (keyobj) decrRefCount(keyobj);
    redisLog(REDIS_WARNING,"Short read or OOM loading DB. Unrecoverable error, exiting now.");
    exit(1);
    return REDIS_ERR; /* Just to avoid warning */
}

/*================================== Commands =============================== */

static void authCommand(redisClient *c) {
    if (!server.requirepass || !strcmp(c->argv[1]->ptr, server.requirepass)) {
      c->authenticated = 1;
      addReply(c,shared.ok);
    } else {
      c->authenticated = 0;
      addReply(c,shared.err);
    }
}

static void pingCommand(redisClient *c) {
    addReply(c,shared.pong);
}

static void echoCommand(redisClient *c) {
    addReplySds(c,sdscatprintf(sdsempty(),"$%d\r\n",
        (int)sdslen(c->argv[1]->ptr)));
    addReply(c,c->argv[1]);
    addReply(c,shared.crlf);
}

/*=================================== Strings =============================== */

static void setGenericCommand(redisClient *c, int nx) {
    int retval;

    retval = dictAdd(c->db->dict,c->argv[1],c->argv[2]);
    if (retval == DICT_ERR) {
        if (!nx) {
            dictReplace(c->db->dict,c->argv[1],c->argv[2]);
            incrRefCount(c->argv[2]);
        } else {
            addReply(c,shared.czero);
            return;
        }
    } else {
        incrRefCount(c->argv[1]);
        incrRefCount(c->argv[2]);
    }
    server.dirty++;
    removeExpire(c->db,c->argv[1]);
    addReply(c, nx ? shared.cone : shared.ok);
}

static void setCommand(redisClient *c) {
    setGenericCommand(c,0);
}

static void setnxCommand(redisClient *c) {
    setGenericCommand(c,1);
}

static void getCommand(redisClient *c) {
    robj *o = lookupKeyRead(c->db,c->argv[1]);

    if (o == NULL) {
        addReply(c,shared.nullbulk);
    } else {
        if (o->type != REDIS_STRING) {
            addReply(c,shared.wrongtypeerr);
        } else {
            addReplySds(c,sdscatprintf(sdsempty(),"$%d\r\n",(int)sdslen(o->ptr)));
            addReply(c,o);
            addReply(c,shared.crlf);
        }
    }
}

static void mgetCommand(redisClient *c) {
    int j;
  
    addReplySds(c,sdscatprintf(sdsempty(),"*%d\r\n",c->argc-1));
    for (j = 1; j < c->argc; j++) {
        robj *o = lookupKeyRead(c->db,c->argv[j]);
        if (o == NULL) {
            addReply(c,shared.nullbulk);
        } else {
            if (o->type != REDIS_STRING) {
                addReply(c,shared.nullbulk);
            } else {
                addReplySds(c,sdscatprintf(sdsempty(),"$%d\r\n",(int)sdslen(o->ptr)));
                addReply(c,o);
                addReply(c,shared.crlf);
            }
        }
    }
}

static void incrDecrCommand(redisClient *c, int incr) {
    long long value;
    int retval;
    robj *o;
    
    o = lookupKeyWrite(c->db,c->argv[1]);
    if (o == NULL) {
        value = 0;
    } else {
        if (o->type != REDIS_STRING) {
            value = 0;
        } else {
            char *eptr;

            value = strtoll(o->ptr, &eptr, 10);
        }
    }

    value += incr;
    o = createObject(REDIS_STRING,sdscatprintf(sdsempty(),"%lld",value));
    retval = dictAdd(c->db->dict,c->argv[1],o);
    if (retval == DICT_ERR) {
        dictReplace(c->db->dict,c->argv[1],o);
        removeExpire(c->db,c->argv[1]);
    } else {
        incrRefCount(c->argv[1]);
    }
    server.dirty++;
    addReply(c,shared.colon);
    addReply(c,o);
    addReply(c,shared.crlf);
}

static void incrCommand(redisClient *c) {
    incrDecrCommand(c,1);
}

static void decrCommand(redisClient *c) {
    incrDecrCommand(c,-1);
}

static void incrbyCommand(redisClient *c) {
    int incr = atoi(c->argv[2]->ptr);
    incrDecrCommand(c,incr);
}

static void decrbyCommand(redisClient *c) {
    int incr = atoi(c->argv[2]->ptr);
    incrDecrCommand(c,-incr);
}

/* ========================= Type agnostic commands ========================= */

static void delCommand(redisClient *c) {
    if (deleteKey(c->db,c->argv[1])) {
        server.dirty++;
        addReply(c,shared.cone);
    } else {
        addReply(c,shared.czero);
    }
}

static void existsCommand(redisClient *c) {
    addReply(c,lookupKeyRead(c->db,c->argv[1]) ? shared.cone : shared.czero);
}

static void selectCommand(redisClient *c) {
    int id = atoi(c->argv[1]->ptr);
    
    if (selectDb(c,id) == REDIS_ERR) {
        addReplySds(c,sdsnew("-ERR invalid DB index\r\n"));
    } else {
        addReply(c,shared.ok);
    }
}

static void randomkeyCommand(redisClient *c) {
    dictEntry *de;
   
    while(1) {
        de = dictGetRandomKey(c->db->dict);
        if (!de || expireIfNeeded(c->db,dictGetEntryKey(de)) == 0) break;
    }
    if (de == NULL) {
        addReply(c,shared.plus);
        addReply(c,shared.crlf);
    } else {
        addReply(c,shared.plus);
        addReply(c,dictGetEntryKey(de));
        addReply(c,shared.crlf);
    }
}

static void keysCommand(redisClient *c) {
    dictIterator *di;
    dictEntry *de;
    sds pattern = c->argv[1]->ptr;
    int plen = sdslen(pattern);
    int numkeys = 0, keyslen = 0;
    robj *lenobj = createObject(REDIS_STRING,NULL);

    di = dictGetIterator(c->db->dict);
    if (!di) oom("dictGetIterator");
    addReply(c,lenobj);
    decrRefCount(lenobj);
    while((de = dictNext(di)) != NULL) {
        robj *keyobj = dictGetEntryKey(de);

        sds key = keyobj->ptr;
        if ((pattern[0] == '*' && pattern[1] == '\0') ||
            stringmatchlen(pattern,plen,key,sdslen(key),0)) {
            if (expireIfNeeded(c->db,keyobj) == 0) {
                if (numkeys != 0)
                    addReply(c,shared.space);
                addReply(c,keyobj);
                numkeys++;
                keyslen += sdslen(key);
            }
        }
    }
    dictReleaseIterator(di);
    lenobj->ptr = sdscatprintf(sdsempty(),"$%lu\r\n",keyslen+(numkeys ? (numkeys-1) : 0));
    addReply(c,shared.crlf);
}

static void dbsizeCommand(redisClient *c) {
    addReplySds(c,
        sdscatprintf(sdsempty(),":%lu\r\n",dictSize(c->db->dict)));
}

static void lastsaveCommand(redisClient *c) {
    addReplySds(c,
        sdscatprintf(sdsempty(),":%lu\r\n",server.lastsave));
}

static void typeCommand(redisClient *c) {
    robj *o;
    char *type;

    o = lookupKeyRead(c->db,c->argv[1]);
    if (o == NULL) {
        type = "+none";
    } else {
        switch(o->type) {
        case REDIS_STRING: type = "+string"; break;
        case REDIS_LIST: type = "+list"; break;
        case REDIS_SET: type = "+set"; break;
        default: type = "unknown"; break;
        }
    }
    addReplySds(c,sdsnew(type));
    addReply(c,shared.crlf);
}

static void saveCommand(redisClient *c) {
    if (server.bgsaveinprogress) {
        addReplySds(c,sdsnew("-ERR background save in progress\r\n"));
        return;
    }
    if (rdbSave(server.dbfilename) == REDIS_OK) {
        addReply(c,shared.ok);
    } else {
        addReply(c,shared.err);
    }
}

static void bgsaveCommand(redisClient *c) {
    if (server.bgsaveinprogress) {
        addReplySds(c,sdsnew("-ERR background save already in progress\r\n"));
        return;
    }
    if (rdbSaveBackground(server.dbfilename) == REDIS_OK) {
        addReply(c,shared.ok);
    } else {
        addReply(c,shared.err);
    }
}

static void shutdownCommand(redisClient *c) {
    redisLog(REDIS_WARNING,"User requested shutdown, saving DB...");
    if (rdbSave(server.dbfilename) == REDIS_OK) {
        if (server.daemonize) {
          unlink(server.pidfile);
        }
        redisLog(REDIS_WARNING,"Server exit now, bye bye...");
        exit(1);
    } else {
        redisLog(REDIS_WARNING,"Error trying to save the DB, can't exit"); 
        addReplySds(c,sdsnew("-ERR can't quit, problems saving the DB\r\n"));
    }
}

static void renameGenericCommand(redisClient *c, int nx) {
    robj *o;

    /* To use the same key as src and dst is probably an error */
    if (sdscmp(c->argv[1]->ptr,c->argv[2]->ptr) == 0) {
        addReply(c,shared.sameobjecterr);
        return;
    }

    o = lookupKeyWrite(c->db,c->argv[1]);
    if (o == NULL) {
        addReply(c,shared.nokeyerr);
        return;
    }
    incrRefCount(o);
    deleteIfVolatile(c->db,c->argv[2]);
    if (dictAdd(c->db->dict,c->argv[2],o) == DICT_ERR) {
        if (nx) {
            decrRefCount(o);
            addReply(c,shared.czero);
            return;
        }
        dictReplace(c->db->dict,c->argv[2],o);
    } else {
        incrRefCount(c->argv[2]);
    }
    deleteKey(c->db,c->argv[1]);
    server.dirty++;
    addReply(c,nx ? shared.cone : shared.ok);
}

static void renameCommand(redisClient *c) {
    renameGenericCommand(c,0);
}

static void renamenxCommand(redisClient *c) {
    renameGenericCommand(c,1);
}

static void moveCommand(redisClient *c) {
    robj *o;
    redisDb *src, *dst;
    int srcid;

    /* Obtain source and target DB pointers */
    src = c->db;
    srcid = c->db->id;
    if (selectDb(c,atoi(c->argv[2]->ptr)) == REDIS_ERR) {
        addReply(c,shared.outofrangeerr);
        return;
    }
    dst = c->db;
    selectDb(c,srcid); /* Back to the source DB */

    /* If the user is moving using as target the same
     * DB as the source DB it is probably an error. */
    if (src == dst) {
        addReply(c,shared.sameobjecterr);
        return;
    }

    /* Check if the element exists and get a reference */
    o = lookupKeyWrite(c->db,c->argv[1]);
    if (!o) {
        addReply(c,shared.czero);
        return;
    }

    /* Try to add the element to the target DB */
    deleteIfVolatile(dst,c->argv[1]);
    if (dictAdd(dst->dict,c->argv[1],o) == DICT_ERR) {
        addReply(c,shared.czero);
        return;
    }
    incrRefCount(c->argv[1]);
    incrRefCount(o);

    /* OK! key moved, free the entry in the source DB */
    deleteKey(src,c->argv[1]);
    server.dirty++;
    addReply(c,shared.cone);
}

/* =================================== Lists ================================ */
static void pushGenericCommand(redisClient *c, int where) {
    robj *lobj;
    list *list;

    lobj = lookupKeyWrite(c->db,c->argv[1]);
    if (lobj == NULL) {
        lobj = createListObject();
        list = lobj->ptr;
        if (where == REDIS_HEAD) {
            if (!listAddNodeHead(list,c->argv[2])) oom("listAddNodeHead");
        } else {
            if (!listAddNodeTail(list,c->argv[2])) oom("listAddNodeTail");
        }
        dictAdd(c->db->dict,c->argv[1],lobj);
        incrRefCount(c->argv[1]);
        incrRefCount(c->argv[2]);
    } else {
        if (lobj->type != REDIS_LIST) {
            addReply(c,shared.wrongtypeerr);
            return;
        }
        list = lobj->ptr;
        if (where == REDIS_HEAD) {
            if (!listAddNodeHead(list,c->argv[2])) oom("listAddNodeHead");
        } else {
            if (!listAddNodeTail(list,c->argv[2])) oom("listAddNodeTail");
        }
        incrRefCount(c->argv[2]);
    }
    server.dirty++;
    addReply(c,shared.ok);
}

static void lpushCommand(redisClient *c) {
    pushGenericCommand(c,REDIS_HEAD);
}

static void rpushCommand(redisClient *c) {
    pushGenericCommand(c,REDIS_TAIL);
}

static void llenCommand(redisClient *c) {
    robj *o;
    list *l;
    
    o = lookupKeyRead(c->db,c->argv[1]);
    if (o == NULL) {
        addReply(c,shared.czero);
        return;
    } else {
        if (o->type != REDIS_LIST) {
            addReply(c,shared.wrongtypeerr);
        } else {
            l = o->ptr;
            addReplySds(c,sdscatprintf(sdsempty(),":%d\r\n",listLength(l)));
        }
    }
}

static void lindexCommand(redisClient *c) {
    robj *o;
    int index = atoi(c->argv[2]->ptr);
    
    o = lookupKeyRead(c->db,c->argv[1]);
    if (o == NULL) {
        addReply(c,shared.nullbulk);
    } else {
        if (o->type != REDIS_LIST) {
            addReply(c,shared.wrongtypeerr);
        } else {
            list *list = o->ptr;
            listNode *ln;
            
            ln = listIndex(list, index);
            if (ln == NULL) {
                addReply(c,shared.nullbulk);
            } else {
                robj *ele = listNodeValue(ln);
                addReplySds(c,sdscatprintf(sdsempty(),"$%d\r\n",(int)sdslen(ele->ptr)));
                addReply(c,ele);
                addReply(c,shared.crlf);
            }
        }
    }
}

static void lsetCommand(redisClient *c) {
    robj *o;
    int index = atoi(c->argv[2]->ptr);
    
    o = lookupKeyWrite(c->db,c->argv[1]);
    if (o == NULL) {
        addReply(c,shared.nokeyerr);
    } else {
        if (o->type != REDIS_LIST) {
            addReply(c,shared.wrongtypeerr);
        } else {
            list *list = o->ptr;
            listNode *ln;
            
            ln = listIndex(list, index);
            if (ln == NULL) {
                addReply(c,shared.outofrangeerr);
            } else {
                robj *ele = listNodeValue(ln);

                decrRefCount(ele);
                listNodeValue(ln) = c->argv[3];
                incrRefCount(c->argv[3]);
                addReply(c,shared.ok);
                server.dirty++;
            }
        }
    }
}

static void popGenericCommand(redisClient *c, int where) {
    robj *o;

    o = lookupKeyWrite(c->db,c->argv[1]);
    if (o == NULL) {
        addReply(c,shared.nullbulk);
    } else {
        if (o->type != REDIS_LIST) {
            addReply(c,shared.wrongtypeerr);
        } else {
            list *list = o->ptr;
            listNode *ln;

            if (where == REDIS_HEAD)
                ln = listFirst(list);
            else
                ln = listLast(list);

            if (ln == NULL) {
                addReply(c,shared.nullbulk);
            } else {
                robj *ele = listNodeValue(ln);
                addReplySds(c,sdscatprintf(sdsempty(),"$%d\r\n",(int)sdslen(ele->ptr)));
                addReply(c,ele);
                addReply(c,shared.crlf);
                listDelNode(list,ln);
                server.dirty++;
            }
        }
    }
}

static void lpopCommand(redisClient *c) {
    popGenericCommand(c,REDIS_HEAD);
}

static void rpopCommand(redisClient *c) {
    popGenericCommand(c,REDIS_TAIL);
}

static void lrangeCommand(redisClient *c) {
    robj *o;
    int start = atoi(c->argv[2]->ptr);
    int end = atoi(c->argv[3]->ptr);

    o = lookupKeyRead(c->db,c->argv[1]);
    if (o == NULL) {
        addReply(c,shared.nullmultibulk);
    } else {
        if (o->type != REDIS_LIST) {
            addReply(c,shared.wrongtypeerr);
        } else {
            list *list = o->ptr;
            listNode *ln;
            int llen = listLength(list);
            int rangelen, j;
            robj *ele;

            /* convert negative indexes */
            if (start < 0) start = llen+start;
            if (end < 0) end = llen+end;
            if (start < 0) start = 0;
            if (end < 0) end = 0;

            /* indexes sanity checks */
            if (start > end || start >= llen) {
                /* Out of range start or start > end result in empty list */
                addReply(c,shared.emptymultibulk);
                return;
            }
            if (end >= llen) end = llen-1;
            rangelen = (end-start)+1;

            /* Return the result in form of a multi-bulk reply */
            ln = listIndex(list, start);
            addReplySds(c,sdscatprintf(sdsempty(),"*%d\r\n",rangelen));
            for (j = 0; j < rangelen; j++) {
                ele = listNodeValue(ln);
                addReplySds(c,sdscatprintf(sdsempty(),"$%d\r\n",(int)sdslen(ele->ptr)));
                addReply(c,ele);
                addReply(c,shared.crlf);
                ln = ln->next;
            }
        }
    }
}

static void ltrimCommand(redisClient *c) {
    robj *o;
    int start = atoi(c->argv[2]->ptr);
    int end = atoi(c->argv[3]->ptr);
    
    o = lookupKeyWrite(c->db,c->argv[1]);
    if (o == NULL) {
        addReply(c,shared.nokeyerr);
    } else {
        if (o->type != REDIS_LIST) {
            addReply(c,shared.wrongtypeerr);
        } else {
            list *list = o->ptr;
            listNode *ln;
            int llen = listLength(list);
            int j, ltrim, rtrim;

            /* convert negative indexes */
            if (start < 0) start = llen+start;
            if (end < 0) end = llen+end;
            if (start < 0) start = 0;
            if (end < 0) end = 0;

            /* indexes sanity checks */
            if (start > end || start >= llen) {
                /* Out of range start or start > end result in empty list */
                ltrim = llen;
                rtrim = 0;
            } else {
                if (end >= llen) end = llen-1;
                ltrim = start;
                rtrim = llen-end-1;
            }

            /* Remove list elements to perform the trim */
            for (j = 0; j < ltrim; j++) {
                ln = listFirst(list);
                listDelNode(list,ln);
            }
            for (j = 0; j < rtrim; j++) {
                ln = listLast(list);
                listDelNode(list,ln);
            }
            addReply(c,shared.ok);
            server.dirty++;
        }
    }
}

static void lremCommand(redisClient *c) {
    robj *o;
    
    o = lookupKeyWrite(c->db,c->argv[1]);
    if (o == NULL) {
        addReply(c,shared.nokeyerr);
    } else {
        if (o->type != REDIS_LIST) {
            addReply(c,shared.wrongtypeerr);
        } else {
            list *list = o->ptr;
            listNode *ln, *next;
            int toremove = atoi(c->argv[2]->ptr);
            int removed = 0;
            int fromtail = 0;

            if (toremove < 0) {
                toremove = -toremove;
                fromtail = 1;
            }
            ln = fromtail ? list->tail : list->head;
            while (ln) {
                robj *ele = listNodeValue(ln);

                next = fromtail ? ln->prev : ln->next;
                if (sdscmp(ele->ptr,c->argv[3]->ptr) == 0) {
                    listDelNode(list,ln);
                    server.dirty++;
                    removed++;
                    if (toremove && removed == toremove) break;
                }
                ln = next;
            }
            addReplySds(c,sdscatprintf(sdsempty(),":%d\r\n",removed));
        }
    }
}

/* ==================================== Sets ================================ */

static void saddCommand(redisClient *c) {
    robj *set;

    set = lookupKeyWrite(c->db,c->argv[1]);
    if (set == NULL) {
        set = createSetObject();
        dictAdd(c->db->dict,c->argv[1],set);
        incrRefCount(c->argv[1]);
    } else {
        if (set->type != REDIS_SET) {
            addReply(c,shared.wrongtypeerr);
            return;
        }
    }
    if (dictAdd(set->ptr,c->argv[2],NULL) == DICT_OK) {
        incrRefCount(c->argv[2]);
        server.dirty++;
        addReply(c,shared.cone);
    } else {
        addReply(c,shared.czero);
    }
}

static void sremCommand(redisClient *c) {
    robj *set;

    set = lookupKeyWrite(c->db,c->argv[1]);
    if (set == NULL) {
        addReply(c,shared.czero);
    } else {
        if (set->type != REDIS_SET) {
            addReply(c,shared.wrongtypeerr);
            return;
        }
        if (dictDelete(set->ptr,c->argv[2]) == DICT_OK) {
            server.dirty++;
            addReply(c,shared.cone);
        } else {
            addReply(c,shared.czero);
        }
    }
}

static void sismemberCommand(redisClient *c) {
    robj *set;

    set = lookupKeyRead(c->db,c->argv[1]);
    if (set == NULL) {
        addReply(c,shared.czero);
    } else {
        if (set->type != REDIS_SET) {
            addReply(c,shared.wrongtypeerr);
            return;
        }
        if (dictFind(set->ptr,c->argv[2]))
            addReply(c,shared.cone);
        else
            addReply(c,shared.czero);
    }
}

static void scardCommand(redisClient *c) {
    robj *o;
    dict *s;
    
    o = lookupKeyRead(c->db,c->argv[1]);
    if (o == NULL) {
        addReply(c,shared.czero);
        return;
    } else {
        if (o->type != REDIS_SET) {
            addReply(c,shared.wrongtypeerr);
        } else {
            s = o->ptr;
            addReplySds(c,sdscatprintf(sdsempty(),":%d\r\n",
                dictSize(s)));
        }
    }
}

static int qsortCompareSetsByCardinality(const void *s1, const void *s2) {
    dict **d1 = (void*) s1, **d2 = (void*) s2;

    return dictSize(*d1)-dictSize(*d2);
}

static void sinterGenericCommand(redisClient *c, robj **setskeys, int setsnum, robj *dstkey) {
    dict **dv = zmalloc(sizeof(dict*)*setsnum);
    dictIterator *di;
    dictEntry *de;
    robj *lenobj = NULL, *dstset = NULL;
    int j, cardinality = 0;

    if (!dv) oom("sinterCommand");
    for (j = 0; j < setsnum; j++) {
        robj *setobj;

        setobj = dstkey ?
                    lookupKeyWrite(c->db,setskeys[j]) :
                    lookupKeyRead(c->db,setskeys[j]);
        if (!setobj) {
            zfree(dv);
            if (dstkey) {
                deleteKey(c->db,dstkey);
                addReply(c,shared.ok);
            } else {
                addReply(c,shared.nullmultibulk);
            }
            return;
        }
        if (setobj->type != REDIS_SET) {
            zfree(dv);
            addReply(c,shared.wrongtypeerr);
            return;
        }
        dv[j] = setobj->ptr;
    }
    /* Sort sets from the smallest to largest, this will improve our
     * algorithm's performace */
    qsort(dv,setsnum,sizeof(dict*),qsortCompareSetsByCardinality);

    /* The first thing we should output is the total number of elements...
     * since this is a multi-bulk write, but at this stage we don't know
     * the intersection set size, so we use a trick, append an empty object
     * to the output list and save the pointer to later modify it with the
     * right length */
    if (!dstkey) {
        lenobj = createObject(REDIS_STRING,NULL);
        addReply(c,lenobj);
        decrRefCount(lenobj);
    } else {
        /* If we have a target key where to store the resulting set
         * create this key with an empty set inside */
        dstset = createSetObject();
        deleteKey(c->db,dstkey);
        dictAdd(c->db->dict,dstkey,dstset);
        incrRefCount(dstkey);
        server.dirty++;
    }

    /* Iterate all the elements of the first (smallest) set, and test
     * the element against all the other sets, if at least one set does
     * not include the element it is discarded */
    di = dictGetIterator(dv[0]);
    if (!di) oom("dictGetIterator");

    while((de = dictNext(di)) != NULL) {
        robj *ele;

        for (j = 1; j < setsnum; j++)
            if (dictFind(dv[j],dictGetEntryKey(de)) == NULL) break;
        if (j != setsnum)
            continue; /* at least one set does not contain the member */
        ele = dictGetEntryKey(de);
        if (!dstkey) {
            addReplySds(c,sdscatprintf(sdsempty(),"$%d\r\n",sdslen(ele->ptr)));
            addReply(c,ele);
            addReply(c,shared.crlf);
            cardinality++;
        } else {
            dictAdd(dstset->ptr,ele,NULL);
            incrRefCount(ele);
            server.dirty++;
        }
    }
    dictReleaseIterator(di);

    if (!dstkey)
        lenobj->ptr = sdscatprintf(sdsempty(),"*%d\r\n",cardinality);
    else
        addReply(c,shared.ok);
    zfree(dv);
}

static void sinterCommand(redisClient *c) {
    sinterGenericCommand(c,c->argv+1,c->argc-1,NULL);
}

static void sinterstoreCommand(redisClient *c) {
    sinterGenericCommand(c,c->argv+2,c->argc-2,c->argv[1]);
}

static void flushdbCommand(redisClient *c) {
    dictEmpty(c->db->dict);
    dictEmpty(c->db->expires);
    server.dirty++;
    addReply(c,shared.ok);
    rdbSave(server.dbfilename);
}

static void flushallCommand(redisClient *c) {
    emptyDb();
    server.dirty++;
    addReply(c,shared.ok);
    rdbSave(server.dbfilename);
}

redisSortOperation *createSortOperation(int type, robj *pattern) {
    redisSortOperation *so = zmalloc(sizeof(*so));
    if (!so) oom("createSortOperation");
    so->type = type;
    so->pattern = pattern;
    return so;
}

/* Return the value associated to the key with a name obtained
 * substituting the first occurence of '*' in 'pattern' with 'subst' */
robj *lookupKeyByPattern(redisDb *db, robj *pattern, robj *subst) {
    char *p;
    sds spat, ssub;
    robj keyobj;
    int prefixlen, sublen, postfixlen;
    /* Expoit the internal sds representation to create a sds string allocated on the stack in order to make this function faster */
    struct {
        long len;
        long free;
        char buf[REDIS_SORTKEY_MAX+1];
    } keyname;

    spat = pattern->ptr;
    ssub = subst->ptr;
    if (sdslen(spat)+sdslen(ssub)-1 > REDIS_SORTKEY_MAX) return NULL;
    p = strchr(spat,'*');
    if (!p) return NULL;

    prefixlen = p-spat;
    sublen = sdslen(ssub);
    postfixlen = sdslen(spat)-(prefixlen+1);
    memcpy(keyname.buf,spat,prefixlen);
    memcpy(keyname.buf+prefixlen,ssub,sublen);
    memcpy(keyname.buf+prefixlen+sublen,p+1,postfixlen);
    keyname.buf[prefixlen+sublen+postfixlen] = '\0';
    keyname.len = prefixlen+sublen+postfixlen;

    keyobj.refcount = 1;
    keyobj.type = REDIS_STRING;
    keyobj.ptr = ((char*)&keyname)+(sizeof(long)*2);

    /* printf("lookup '%s' => %p\n", keyname.buf,de); */
    return lookupKeyRead(db,&keyobj);
}

/* sortCompare() is used by qsort in sortCommand(). Given that qsort_r with
 * the additional parameter is not standard but a BSD-specific we have to
 * pass sorting parameters via the global 'server' structure */
static int sortCompare(const void *s1, const void *s2) {
    const redisSortObject *so1 = s1, *so2 = s2;
    int cmp;

    if (!server.sort_alpha) {
        /* Numeric sorting. Here it's trivial as we precomputed scores */
        if (so1->u.score > so2->u.score) {
            cmp = 1;
        } else if (so1->u.score < so2->u.score) {
            cmp = -1;
        } else {
            cmp = 0;
        }
    } else {
        /* Alphanumeric sorting */
        if (server.sort_bypattern) {
            if (!so1->u.cmpobj || !so2->u.cmpobj) {
                /* At least one compare object is NULL */
                if (so1->u.cmpobj == so2->u.cmpobj)
                    cmp = 0;
                else if (so1->u.cmpobj == NULL)
                    cmp = -1;
                else
                    cmp = 1;
            } else {
                /* We have both the objects, use strcoll */
                cmp = strcoll(so1->u.cmpobj->ptr,so2->u.cmpobj->ptr);
            }
        } else {
            /* Compare elements directly */
            cmp = strcoll(so1->obj->ptr,so2->obj->ptr);
        }
    }
    return server.sort_desc ? -cmp : cmp;
}

/* The SORT command is the most complex command in Redis. Warning: this code
 * is optimized for speed and a bit less for readability */
static void sortCommand(redisClient *c) {
    list *operations;
    int outputlen = 0;
    int desc = 0, alpha = 0;
    int limit_start = 0, limit_count = -1, start, end;
    int j, dontsort = 0, vectorlen;
    int getop = 0; /* GET operation counter */
    robj *sortval, *sortby = NULL;
    redisSortObject *vector; /* Resulting vector to sort */

    /* Lookup the key to sort. It must be of the right types */
    sortval = lookupKeyRead(c->db,c->argv[1]);
    if (sortval == NULL) {
        addReply(c,shared.nokeyerr);
        return;
    }
    if (sortval->type != REDIS_SET && sortval->type != REDIS_LIST) {
        addReply(c,shared.wrongtypeerr);
        return;
    }

    /* Create a list of operations to perform for every sorted element.
     * Operations can be GET/DEL/INCR/DECR */
    operations = listCreate();
    listSetFreeMethod(operations,zfree);
    j = 2;

    /* Now we need to protect sortval incrementing its count, in the future
     * SORT may have options able to overwrite/delete keys during the sorting
     * and the sorted key itself may get destroied */
    incrRefCount(sortval);

    /* The SORT command has an SQL-alike syntax, parse it */
    while(j < c->argc) {
        int leftargs = c->argc-j-1;
        if (!strcasecmp(c->argv[j]->ptr,"asc")) {
            desc = 0;
        } else if (!strcasecmp(c->argv[j]->ptr,"desc")) {
            desc = 1;
        } else if (!strcasecmp(c->argv[j]->ptr,"alpha")) {
            alpha = 1;
        } else if (!strcasecmp(c->argv[j]->ptr,"limit") && leftargs >= 2) {
            limit_start = atoi(c->argv[j+1]->ptr);
            limit_count = atoi(c->argv[j+2]->ptr);
            j+=2;
        } else if (!strcasecmp(c->argv[j]->ptr,"by") && leftargs >= 1) {
            sortby = c->argv[j+1];
            /* If the BY pattern does not contain '*', i.e. it is constant,
             * we don't need to sort nor to lookup the weight keys. */
            if (strchr(c->argv[j+1]->ptr,'*') == NULL) dontsort = 1;
            j++;
        } else if (!strcasecmp(c->argv[j]->ptr,"get") && leftargs >= 1) {
            listAddNodeTail(operations,createSortOperation(
                REDIS_SORT_GET,c->argv[j+1]));
            getop++;
            j++;
        } else if (!strcasecmp(c->argv[j]->ptr,"del") && leftargs >= 1) {
            listAddNodeTail(operations,createSortOperation(
                REDIS_SORT_DEL,c->argv[j+1]));
            j++;
        } else if (!strcasecmp(c->argv[j]->ptr,"incr") && leftargs >= 1) {
            listAddNodeTail(operations,createSortOperation(
                REDIS_SORT_INCR,c->argv[j+1]));
            j++;
        } else if (!strcasecmp(c->argv[j]->ptr,"get") && leftargs >= 1) {
            listAddNodeTail(operations,createSortOperation(
                REDIS_SORT_DECR,c->argv[j+1]));
            j++;
        } else {
            decrRefCount(sortval);
            listRelease(operations);
            addReply(c,shared.syntaxerr);
            return;
        }
        j++;
    }

    /* Load the sorting vector with all the objects to sort */
    vectorlen = (sortval->type == REDIS_LIST) ?
        listLength((list*)sortval->ptr) :
        dictSize((dict*)sortval->ptr);
    vector = zmalloc(sizeof(redisSortObject)*vectorlen);
    if (!vector) oom("allocating objects vector for SORT");
    j = 0;
    if (sortval->type == REDIS_LIST) {
        list *list = sortval->ptr;
        listNode *ln = list->head;
        while(ln) {
            robj *ele = ln->value;
            vector[j].obj = ele;
            vector[j].u.score = 0;
            vector[j].u.cmpobj = NULL;
            ln = ln->next;
            j++;
        }
    } else {
        dict *set = sortval->ptr;
        dictIterator *di;
        dictEntry *setele;

        di = dictGetIterator(set);
        if (!di) oom("dictGetIterator");
        while((setele = dictNext(di)) != NULL) {
            vector[j].obj = dictGetEntryKey(setele);
            vector[j].u.score = 0;
            vector[j].u.cmpobj = NULL;
            j++;
        }
        dictReleaseIterator(di);
    }
    assert(j == vectorlen);

    /* Now it's time to load the right scores in the sorting vector */
    if (dontsort == 0) {
        for (j = 0; j < vectorlen; j++) {
            if (sortby) {
                robj *byval;

                byval = lookupKeyByPattern(c->db,sortby,vector[j].obj);
                if (!byval || byval->type != REDIS_STRING) continue;
                if (alpha) {
                    vector[j].u.cmpobj = byval;
                    incrRefCount(byval);
                } else {
                    vector[j].u.score = strtod(byval->ptr,NULL);
                }
            } else {
                if (!alpha) vector[j].u.score = strtod(vector[j].obj->ptr,NULL);
            }
        }
    }

    /* We are ready to sort the vector... perform a bit of sanity check
     * on the LIMIT option too. We'll use a partial version of quicksort. */
    start = (limit_start < 0) ? 0 : limit_start;
    end = (limit_count < 0) ? vectorlen-1 : start+limit_count-1;
    if (start >= vectorlen) {
        start = vectorlen-1;
        end = vectorlen-2;
    }
    if (end >= vectorlen) end = vectorlen-1;

    if (dontsort == 0) {
        server.sort_desc = desc;
        server.sort_alpha = alpha;
        server.sort_bypattern = sortby ? 1 : 0;
        qsort(vector,vectorlen,sizeof(redisSortObject),sortCompare);
    }

    /* Send command output to the output buffer, performing the specified
     * GET/DEL/INCR/DECR operations if any. */
    outputlen = getop ? getop*(end-start+1) : end-start+1;
    addReplySds(c,sdscatprintf(sdsempty(),"*%d\r\n",outputlen));
    for (j = start; j <= end; j++) {
        listNode *ln = operations->head;
        if (!getop) {
            addReplySds(c,sdscatprintf(sdsempty(),"$%d\r\n",
                sdslen(vector[j].obj->ptr)));
            addReply(c,vector[j].obj);
            addReply(c,shared.crlf);
        }
        while(ln) {
            redisSortOperation *sop = ln->value;
            robj *val = lookupKeyByPattern(c->db,sop->pattern,
                vector[j].obj);

            if (sop->type == REDIS_SORT_GET) {
                if (!val || val->type != REDIS_STRING) {
                    addReply(c,shared.nullbulk);
                } else {
                    addReplySds(c,sdscatprintf(sdsempty(),"$%d\r\n",
                        sdslen(val->ptr)));
                    addReply(c,val);
                    addReply(c,shared.crlf);
                }
            } else if (sop->type == REDIS_SORT_DEL) {
                /* TODO */
            }
            ln = ln->next;
        }
    }

    /* Cleanup */
    decrRefCount(sortval);
    listRelease(operations);
    for (j = 0; j < vectorlen; j++) {
        if (sortby && alpha && vector[j].u.cmpobj)
            decrRefCount(vector[j].u.cmpobj);
    }
    zfree(vector);
}

static void infoCommand(redisClient *c) {
    sds info;
    time_t uptime = time(NULL)-server.stat_starttime;
    
    info = sdscatprintf(sdsempty(),
        "redis_version:%s\r\n"
        "connected_clients:%d\r\n"
        "connected_slaves:%d\r\n"
        "used_memory:%zu\r\n"
        "changes_since_last_save:%lld\r\n"
        "last_save_time:%d\r\n"
        "total_connections_received:%lld\r\n"
        "total_commands_processed:%lld\r\n"
        "uptime_in_seconds:%d\r\n"
        "uptime_in_days:%d\r\n"
        ,REDIS_VERSION,
        listLength(server.clients)-listLength(server.slaves),
        listLength(server.slaves),
        server.usedmemory,
        server.dirty,
        server.lastsave,
        server.stat_numconnections,
        server.stat_numcommands,
        uptime,
        uptime/(3600*24)
    );
    addReplySds(c,sdscatprintf(sdsempty(),"$%d\r\n",sdslen(info)));
    addReplySds(c,info);
    addReply(c,shared.crlf);
}

static void monitorCommand(redisClient *c) {
    /* ignore MONITOR if aleady slave or in monitor mode */
    if (c->flags & REDIS_SLAVE) return;

    c->flags |= (REDIS_SLAVE|REDIS_MONITOR);
    c->slaveseldb = 0;
    if (!listAddNodeTail(server.monitors,c)) oom("listAddNodeTail");
    addReply(c,shared.ok);
}

/* ================================= Expire ================================= */
static int removeExpire(redisDb *db, robj *key) {
    if (dictDelete(db->expires,key) == DICT_OK) {
        return 1;
    } else {
        return 0;
    }
}

/*
 * 设置过期时间
 */
static int setExpire(redisDb *db, robj *key, time_t when) {
	// 当 dictAdd 正常操作， 就会执行 else 语句， 否则执行 return 0
	// dictAdd 向数据库的 expires 字典中添加过期时间， key 是一个 redis
	// 对象， 说明 redis 对象的过期时间
    if (dictAdd(db->expires,key,(void*)when) == DICT_ERR) {
        return 0;
    } else {
        incrRefCount(key);
        return 1;
    }
}

/* Return the expire time of the specified key, or -1 if no expire
 * is associated with this key (i.e. the key is non volatile) */
static time_t getExpire(redisDb *db, robj *key) {
    dictEntry *de;

    /* No expire? return ASAP */
    if (dictSize(db->expires) == 0 ||
       (de = dictFind(db->expires,key)) == NULL) return -1;

    return (time_t) dictGetEntryVal(de);
}

static int expireIfNeeded(redisDb *db, robj *key) {
    time_t when;
    dictEntry *de;

    /* No expire? return ASAP */
    if (dictSize(db->expires) == 0 ||
       (de = dictFind(db->expires,key)) == NULL) return 0;

    /* Lookup the expire */
    when = (time_t) dictGetEntryVal(de);
    if (time(NULL) <= when) return 0;

    /* Delete the key */
    dictDelete(db->expires,key);
    return dictDelete(db->dict,key) == DICT_OK;
}

static int deleteIfVolatile(redisDb *db, robj *key) {
    dictEntry *de;

    /* No expire? return ASAP */
    if (dictSize(db->expires) == 0 ||
       (de = dictFind(db->expires,key)) == NULL) return 0;

    /* Delete the key */
    server.dirty++;
    dictDelete(db->expires,key);
    return dictDelete(db->dict,key) == DICT_OK;
}

static void expireCommand(redisClient *c) {
    dictEntry *de;
    int seconds = atoi(c->argv[2]->ptr);

    de = dictFind(c->db->dict,c->argv[1]);
    if (de == NULL) {
        addReply(c,shared.czero);
        return;
    }
    if (seconds <= 0) {
        addReply(c, shared.czero);
        return;
    } else {
        time_t when = time(NULL)+seconds;
        if (setExpire(c->db,c->argv[1],when))
            addReply(c,shared.cone);
        else
            addReply(c,shared.czero);
        return;
    }
}

/* =============================== Replication  ============================= */

/* Send the whole output buffer syncronously to the slave. This a general operation in theory, but it is actually useful only for replication. */
static int flushClientOutput(redisClient *c) {
    int retval;
    time_t start = time(NULL);

    while(listLength(c->reply)) {
        if (time(NULL)-start > 5) return REDIS_ERR; /* 5 seconds timeout */
        retval = aeWait(c->fd,AE_WRITABLE,1000);
        if (retval == -1) {
            return REDIS_ERR;
        } else if (retval & AE_WRITABLE) {
            sendReplyToClient(NULL, c->fd, c, AE_WRITABLE);
        }
    }
    return REDIS_OK;
}

static int syncWrite(int fd, char *ptr, ssize_t size, int timeout) {
    ssize_t nwritten, ret = size;
    time_t start = time(NULL);

    timeout++;
    while(size) {
        if (aeWait(fd,AE_WRITABLE,1000) & AE_WRITABLE) {
            nwritten = write(fd,ptr,size);
            if (nwritten == -1) return -1;
            ptr += nwritten;
            size -= nwritten;
        }
        if ((time(NULL)-start) > timeout) {
            errno = ETIMEDOUT;
            return -1;
        }
    }
    return ret;
}

static int syncRead(int fd, char *ptr, ssize_t size, int timeout) {
    ssize_t nread, totread = 0;
    time_t start = time(NULL);

    timeout++;
    while(size) {
        if (aeWait(fd,AE_READABLE,1000) & AE_READABLE) {
            nread = read(fd,ptr,size);
            if (nread == -1) return -1;
            ptr += nread;
            size -= nread;
            totread += nread;
        }
        if ((time(NULL)-start) > timeout) {
            errno = ETIMEDOUT;
            return -1;
        }
    }
    return totread;
}

static int syncReadLine(int fd, char *ptr, ssize_t size, int timeout) {
    ssize_t nread = 0;

    size--;
    while(size) {
        char c;

        if (syncRead(fd,&c,1,timeout) == -1) return -1;
        if (c == '\n') {
            *ptr = '\0';
            if (nread && *(ptr-1) == '\r') *(ptr-1) = '\0';
            return nread;
        } else {
            *ptr++ = c;
            *ptr = '\0';
            nread++;
        }
    }
    return nread;
}

static void syncCommand(redisClient *c) {
    struct stat sb;
    int fd = -1, len;
    time_t start = time(NULL);
    char sizebuf[32];

    /* ignore SYNC if aleady slave or in monitor mode */
    if (c->flags & REDIS_SLAVE) return;

    redisLog(REDIS_NOTICE,"Slave ask for syncronization");
    if (flushClientOutput(c) == REDIS_ERR ||
        rdbSave(server.dbfilename) != REDIS_OK)
        goto closeconn;

    fd = open(server.dbfilename, O_RDONLY);
    if (fd == -1 || fstat(fd,&sb) == -1) goto closeconn;
    len = sb.st_size;

    snprintf(sizebuf,32,"$%d\r\n",len);
    if (syncWrite(c->fd,sizebuf,strlen(sizebuf),5) == -1) goto closeconn;
    while(len) {
        char buf[1024];
        int nread;

        if (time(NULL)-start > REDIS_MAX_SYNC_TIME) goto closeconn;
        nread = read(fd,buf,1024);
        if (nread == -1) goto closeconn;
        len -= nread;
        if (syncWrite(c->fd,buf,nread,5) == -1) goto closeconn;
    }
    if (syncWrite(c->fd,"\r\n",2,5) == -1) goto closeconn;
    close(fd);
    c->flags |= REDIS_SLAVE;
    c->slaveseldb = 0;
    if (!listAddNodeTail(server.slaves,c)) oom("listAddNodeTail");
    redisLog(REDIS_NOTICE,"Syncronization with slave succeeded");
    return;

closeconn:
    if (fd != -1) close(fd);
    c->flags |= REDIS_CLOSE;
    redisLog(REDIS_WARNING,"Syncronization with slave failed");
    return;
}

static int syncWithMaster(void) {
    char buf[1024], tmpfile[256];
    int dumpsize;
    int fd = anetTcpConnect(NULL,server.masterhost,server.masterport);
    int dfd;

    if (fd == -1) {
        redisLog(REDIS_WARNING,"Unable to connect to MASTER: %s",
            strerror(errno));
        return REDIS_ERR;
    }
    /* Issue the SYNC command */
    if (syncWrite(fd,"SYNC \r\n",7,5) == -1) {
        close(fd);
        redisLog(REDIS_WARNING,"I/O error writing to MASTER: %s",
            strerror(errno));
        return REDIS_ERR;
    }
    /* Read the bulk write count */
    if (syncReadLine(fd,buf,1024,5) == -1) {
        close(fd);
        redisLog(REDIS_WARNING,"I/O error reading bulk count from MASTER: %s",
            strerror(errno));
        return REDIS_ERR;
    }
    dumpsize = atoi(buf+1);
    redisLog(REDIS_NOTICE,"Receiving %d bytes data dump from MASTER",dumpsize);
    /* Read the bulk write data on a temp file */
    snprintf(tmpfile,256,"temp-%d.%ld.rdb",(int)time(NULL),(long int)random());
    dfd = open(tmpfile,O_CREAT|O_WRONLY,0644);
    if (dfd == -1) {
        close(fd);
        redisLog(REDIS_WARNING,"Opening the temp file needed for MASTER <-> SLAVE synchronization: %s",strerror(errno));
        return REDIS_ERR;
    }
    while(dumpsize) {
        int nread, nwritten;

        nread = read(fd,buf,(dumpsize < 1024)?dumpsize:1024);
        if (nread == -1) {
            redisLog(REDIS_WARNING,"I/O error trying to sync with MASTER: %s",
                strerror(errno));
            close(fd);
            close(dfd);
            return REDIS_ERR;
        }
        nwritten = write(dfd,buf,nread);
        if (nwritten == -1) {
            redisLog(REDIS_WARNING,"Write error writing to the DB dump file needed for MASTER <-> SLAVE synchrnonization: %s", strerror(errno));
            close(fd);
            close(dfd);
            return REDIS_ERR;
        }
        dumpsize -= nread;
    }
    close(dfd);
    if (rename(tmpfile,server.dbfilename) == -1) {
        redisLog(REDIS_WARNING,"Failed trying to rename the temp DB into dump.rdb in MASTER <-> SLAVE synchronization: %s", strerror(errno));
        unlink(tmpfile);
        close(fd);
        return REDIS_ERR;
    }
    emptyDb();
    if (rdbLoad(server.dbfilename) != REDIS_OK) {
        redisLog(REDIS_WARNING,"Failed trying to load the MASTER synchronization DB from disk");
        close(fd);
        return REDIS_ERR;
    }
    server.master = createClient(fd);
    server.master->flags |= REDIS_MASTER;
    server.replstate = REDIS_REPL_CONNECTED;
    return REDIS_OK;
}

/* =================================== Main! ================================ */

static void daemonize(void) {
    int fd;
    FILE *fp;

    if (fork() != 0) exit(0); /* parent exits */
    setsid(); /* create a new session */

    /* Every output goes to /dev/null. If Redis is daemonized but
     * the 'logfile' is set to 'stdout' in the configuration file
     * it will not log at all. */
    if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd > STDERR_FILENO) close(fd);
    }
    /* Try to write the pid file */
    fp = fopen(server.pidfile,"w");
    if (fp) {
        fprintf(fp,"%d\n",getpid());
        fclose(fp);
    }
}

int main(int argc, char **argv) {
    // 首先初始化 server 的配置
    initServerConfig();
    if (argc == 2) {
        // 如果是两个参数， 表明使用命令行中给的配置文件， 重置 server params 相关
        // 的数据后， 加载命令中指定的配置
        ResetServerSaveParams();
        loadServerConfig(argv[1]);
    } else if (argc > 2) {
        fprintf(stderr,"Usage: ./redis-server [/path/to/redis.conf]\n");
        exit(1);
    }
    // 开始初始化 server， 会将 server 的各个配置的值进行初始化， 需要注意的是 server
    // 在本文件中是一个全局变量
    initServer();
    // 如果 server.daemonize 非零， 则执行 daemonize() 函数以守护进程的方式启动 server
    if (server.daemonize) daemonize();
    // 启动 server 时记录日志
    redisLog(REDIS_NOTICE,"Server started, Redis version " REDIS_VERSION);
    // 正常加载数据库
    if (rdbLoad(server.dbfilename) == REDIS_OK)
        redisLog(REDIS_NOTICE,"DB loaded from disk");
    // 创建事件轮询
    if (aeCreateFileEvent(server.el, server.fd, AE_READABLE,
        acceptHandler, NULL, NULL) == AE_ERR) oom("creating file event");
    redisLog(REDIS_NOTICE,"The server is now ready to accept connections on port %d", server.port);
    // 事件循环， 执行完毕后删除事件轮询
    aeMain(server.el);
    // 删除事件轮询
    aeDeleteEventLoop(server.el);
    return 0;
}
