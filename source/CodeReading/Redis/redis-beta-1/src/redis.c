/* Redis - REmote DIctionary Server
 * Copyright (C) 2008 Salvatore Sanfilippo antirez at gmail dot com
 * All Rights Reserved. Under the GPL version 2. See the COPYING file. */

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

#include "ae.h"     /* Event driven programming library */
#include "sds.h"    /* Dynamic safe strings */
#include "anet.h"   /* Networking the easy way */
#include "dict.h"   /* Hash tables */
#include "adlist.h" /* Linked lists */

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

/* Hash table parameters */
#define REDIS_HT_MINFILL        10      /* Minimal hash table fill 10% */
#define REDIS_HT_MINSLOTS       16384   /* Never resize the HT under this */

/* Command types */
#define REDIS_CMD_BULK          1
#define REDIS_CMD_INLINE        0

/* Object types */
#define REDIS_STRING 0
#define REDIS_LIST 1
#define REDIS_SET 2
#define REDIS_SELECTDB 254
#define REDIS_EOF 255

/* List related stuff */
#define REDIS_HEAD 0
#define REDIS_TAIL 1

/* Log levels */
#define REDIS_DEBUG 0
#define REDIS_NOTICE 1
#define REDIS_WARNING 2

/* Anti-warning macro... */
#define REDIS_NOTUSED(V) ((void) V)

/*================================= Data types ============================== */

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

/* A redis object, that is a type able to hold a string / list / set */
typedef struct redisObject {
    int type;
    void *ptr;
    int refcount;
} robj;

struct saveparam {
    time_t seconds;
    int changes;
};

/* Global server state structure */
struct redisServer {
    int port;
    int fd;
    dict **dict;
    long long dirty;            /* changes to DB from the last save */
    list *clients;
    char neterr[ANET_ERR_LEN];
    aeEventLoop *el;
    int verbosity;
    int cronloops;
    int maxidletime;
    int dbnum;
    list *objfreelist;          /* A list of freed objects to avoid malloc() */
    int bgsaveinprogress;
    time_t lastsave;
    struct saveparam *saveparams;
    int saveparamslen;
    char *logfile;
};

typedef void redisCommandProc(redisClient *c);
struct redisCommand {
    char *name;
    redisCommandProc *proc;
    int arity;
    int type;
};

struct sharedObjectsStruct {
    robj *crlf, *ok, *err, *zerobulk, *nil, *zero, *one, *pong;
} shared;

/*================================ Prototypes =============================== */

static void freeStringObject(robj *o);
static void freeListObject(robj *o);
static void freeSetObject(robj *o);
static void decrRefCount(void *o);
static robj *createObject(int type, void *ptr);
static void freeClient(redisClient *c);
static int loadDb(char *filename);
static void addReply(redisClient *c, robj *obj);
static void addReplySds(redisClient *c, sds s);
static void incrRefCount(robj *o);
static int saveDbBackground(char *filename);

static void pingCommand(redisClient *c);
static void echoCommand(redisClient *c);
static void setCommand(redisClient *c);
static void setnxCommand(redisClient *c);
static void getCommand(redisClient *c);
static void delCommand(redisClient *c);
static void existsCommand(redisClient *c);
static void incrCommand(redisClient *c);
static void decrCommand(redisClient *c);
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

/*====================== Hash table type implementation  ==================== */

/* This is an hash table type that uses the SDS dynamic strings libary as
 * keys and radis objects as values (objects can hold SDS strings,
 * lists, sets). */

static unsigned int sdsDictHashFunction(const void *key) {
    return dictGenHashFunction(key, sdslen((sds)key));
}

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

static void sdsDictKeyDestructor(void *privdata, void *val)
{
    DICT_NOTUSED(privdata);

    sdsfree(val);
}

static void sdsDictValDestructor(void *privdata, void *val)
{
    DICT_NOTUSED(privdata);

    decrRefCount(val);
}

dictType sdsDictType = {
    sdsDictHashFunction,       /* hash function */
    NULL,                      /* key dup */
    NULL,                      /* val dup */
    sdsDictKeyCompare,         /* key compare */
    sdsDictKeyDestructor,      /* key destructor */
    sdsDictValDestructor,      /* val destructor */
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
        if (now - c->lastinteraction > server.maxidletime) {
            redisLog(REDIS_DEBUG,"Closing idle client");
            freeClient(c);
        }
    }
    listReleaseIterator(li);
}

int serverCron(struct aeEventLoop *eventLoop, long long id, void *clientData) {
    int j, size, used, loops = server.cronloops++;
    REDIS_NOTUSED(eventLoop);
    REDIS_NOTUSED(id);
    REDIS_NOTUSED(clientData);

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

    /* Show information about connected clients */
    if (!(loops % 5)) redisLog(REDIS_DEBUG,"%d clients connected",listLength(server.clients));

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
                saveDbBackground("dump.rdb");
                break;
            }
         }
    }
    return 1000;
}

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

static void appendServerSaveParams(time_t seconds, int changes) {
    server.saveparams = realloc(server.saveparams,sizeof(struct saveparam)*(server.saveparamslen+1));
    if (server.saveparams == NULL) oom("appendServerSaveParams");
    server.saveparams[server.saveparamslen].seconds = seconds;
    server.saveparams[server.saveparamslen].changes = changes;
    server.saveparamslen++;
}

static void ResetServerSaveParams() {
    free(server.saveparams);
    server.saveparams = NULL;
    server.saveparamslen = 0;
}

static void initServerConfig() {
    server.dbnum = REDIS_DEFAULT_DBNUM;
    server.port = REDIS_SERVERPORT;
    server.verbosity = REDIS_DEBUG;
    server.maxidletime = REDIS_MAXIDLETIME;
    server.saveparams = NULL;
    server.logfile = NULL; /* NULL = log on standard output */
    ResetServerSaveParams();

    appendServerSaveParams(60*60,1);  /* save after 1 hour and 1 change */
    appendServerSaveParams(300,100);  /* save after 5 minutes and 100 changes */
    appendServerSaveParams(60,10000); /* save after 1 minute and 10000 changes */
}

static void initServer() {
    int j;

    signal(SIGHUP, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);

    server.clients = listCreate();
    server.objfreelist = listCreate();
    createSharedObjects();
    server.el = aeCreateEventLoop();
    server.dict = malloc(sizeof(dict*)*server.dbnum);
    if (!server.dict || !server.clients || !server.el || !server.objfreelist)
        oom("server initialization"); /* Fatal OOM */
    server.fd = anetTcpServer(server.neterr, server.port, NULL);
    if (server.fd == -1) {
        redisLog(REDIS_WARNING, "Opening TCP port: %s", server.neterr);
        exit(1);
    }
    for (j = 0; j < server.dbnum; j++) {
        server.dict[j] = dictCreate(&sdsDictType,NULL);
        if (!server.dict[j])
            oom("server initialization"); /* Fatal OOM */
    }
    server.cronloops = 0;
    server.bgsaveinprogress = 0;
    server.lastsave = time(NULL);
    server.dirty = 0;
    aeCreateTimeEvent(server.el, 1000, serverCron, NULL, NULL);
}

/* I agree, this is a very rudimental way to load a configuration...
   will improve later if the config gets more complex */
static void loadServerConfig(char *filename) {
    FILE *fp = fopen(filename,"r");
    char buf[REDIS_CONFIGLINE_MAX+1], *err = NULL;
    int linenum = 0;
    sds line = NULL;
    
    if (!fp) {
        redisLog(REDIS_WARNING,"Fatal error, can't open config file");
        exit(1);
    }
    while(fgets(buf,REDIS_CONFIGLINE_MAX+1,fp) != NULL) {
        sds *argv;
        int argc;

        linenum++;
        line = sdsnew(buf);
        line = sdstrim(line," \t\r\n");

        /* Skip comments and blank lines*/
        if (line[0] == '#' || line[0] == '\0') {
            sdsfree(line);
            continue;
        }

        /* Split into arguments */
        argv = sdssplitlen(line,sdslen(line)," ",1,&argc);

        /* Execute config directives */
        if (!strcmp(argv[0],"timeout") && argc == 2) {
            server.maxidletime = atoi(argv[1]);
            if (server.maxidletime < 1) {
                err = "Invalid timeout value"; goto loaderr;
            }
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

            server.logfile = strdup(argv[1]);
            if (!strcmp(server.logfile,"stdout")) server.logfile = NULL;
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
        } else {
            err = "Bad directive or wrong number of arguments"; goto loaderr;
        }
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
        sdsfree(c->argv[j]);
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
    free(c);
}

static void sendReplyToClient(aeEventLoop *el, int fd, void *privdata, int mask) {
    redisClient *c = privdata;
    int nwritten = 0, totwritten = 0, objlen;
    robj *o;
    REDIS_NOTUSED(el);
    REDIS_NOTUSED(mask);

    while(listLength(c->reply)) {
        o = listNodeValue(listFirst(c->reply));
        objlen = sdslen(o->ptr);

        if (objlen == 0) {
            listDelNode(c->reply,listFirst(c->reply));
            continue;
        }

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

    sdstolower(c->argv[0]);
    /* The QUIT command is handled as a special case. Normal command
     * procs are unable to close the client connection safely */
    if (!strcmp(c->argv[0],"quit")) {
        freeClient(c);
        return 0;
    }
    cmd = lookupCommand(c->argv[0]);
    if (!cmd) {
        addReplySds(c,sdsnew("-ERR unknown command\r\n"));
        resetClient(c);
        return 1;
    } else if (cmd->arity != c->argc) {
        addReplySds(c,sdsnew("-ERR wrong number of arguments\r\n"));
        resetClient(c);
        return 1;
    } else if (cmd->type == REDIS_CMD_BULK && c->bulklen == -1) {
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
    /* Exec the command */
    cmd->proc(c);
    resetClient(c);
    return 1;
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
            if (argv == NULL) oom("Splitting query in token");
            for (j = 0; j < argc && j < REDIS_MAX_ARGS; j++) {
                if (sdslen(argv[j])) {
                    c->argv[c->argc] = argv[j];
                    c->argc++;
                } else {
                    sdsfree(argv[j]);
                }
            }
            free(argv);
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
            c->argv[c->argc] = sdsnewlen(c->querybuf,c->bulklen-2);
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
    c->dict = server.dict[id];
    return REDIS_OK;
}

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
    if (createClient(cfd) == REDIS_ERR) {
        redisLog(REDIS_WARNING,"Error allocating resoures for the client");
        close(cfd); /* May be already closed, just ingore errors */
        return;
    }
}

/* ======================= Redis objects implementation ===================== */
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

static robj *createListObject(void) {
    list *l = listCreate();

    if (!l) oom("createListObject");
    listSetFreeMethod(l,decrRefCount);
    return createObject(REDIS_LIST,l);
}

static void freeStringObject(robj *o) {
    sdsfree(o->ptr);
}

static void freeListObject(robj *o) {
    listRelease((list*) o->ptr);
}

static void freeSetObject(robj *o) {
    /* TODO */
    o = o;
}

static void incrRefCount(robj *o) {
    o->refcount++;
}

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

/*============================ DB saving/loading ============================ */

/* Save the DB on disk. Return REDIS_ERR on error, REDIS_OK on success */
static int saveDb(char *filename) {
    dictIterator *di = NULL;
    dictEntry *de;
    uint32_t len;
    uint8_t type;
    FILE *fp;
    char tmpfile[256];
    int j;

    snprintf(tmpfile,256,"temp-%d.%ld.rdb",(int)time(NULL),(long int)random());
    fp = fopen(tmpfile,"w");
    if (!fp) {
        redisLog(REDIS_WARNING, "Failed saving the DB: %s", strerror(errno));
        return REDIS_ERR;
    }
    if (fwrite("REDIS0000",9,1,fp) == 0) goto werr;
    for (j = 0; j < server.dbnum; j++) {
        dict *dict = server.dict[j];
        if (dictGetHashTableUsed(dict) == 0) continue;
        di = dictGetIterator(dict);
        if (!di) {
            fclose(fp);
            return REDIS_ERR;
        }

        /* Write the SELECT DB opcode */
        type = REDIS_SELECTDB;
        len = htonl(j);
        if (fwrite(&type,1,1,fp) == 0) goto werr;
        if (fwrite(&len,4,1,fp) == 0) goto werr;

        /* Iterate this DB writing every entry */
        while((de = dictNext(di)) != NULL) {
            sds key = dictGetEntryKey(de);
            robj *o = dictGetEntryVal(de);

            type = o->type;
            len = htonl(sdslen(key));
            if (fwrite(&type,1,1,fp) == 0) goto werr;
            if (fwrite(&len,4,1,fp) == 0) goto werr;
            if (fwrite(key,sdslen(key),1,fp) == 0) goto werr;
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
        dictReleaseIterator(di);
    }
    /* EOF opcode */
    type = REDIS_EOF;
    if (fwrite(&type,1,1,fp) == 0) goto werr;
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
    redisLog(REDIS_WARNING,"Error saving DB on disk: %s", strerror(errno));
    if (di) dictReleaseIterator(di);
    return REDIS_ERR;
}

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

static int loadDb(char *filename) {
    FILE *fp;
    char buf[REDIS_LOADBUF_LEN];    /* Try to use this buffer instead of */
    char vbuf[REDIS_LOADBUF_LEN];   /* malloc() when the element is small */
    char *key = NULL, *val = NULL;
    uint32_t klen,vlen,dbid;
    uint8_t type;
    int retval;
    dict *dict = server.dict[0];

    fp = fopen(filename,"r");
    if (!fp) return REDIS_ERR;
    if (fread(buf,9,1,fp) == 0) goto eoferr;
    if (memcmp(buf,"REDIS0000",9) != 0) {
        fclose(fp);
        redisLog(REDIS_WARNING,"Wrong signature trying to load DB from file");
        return REDIS_ERR;
    }
    while(1) {
        robj *o;

        /* Read type. */
        if (fread(&type,1,1,fp) == 0) goto eoferr;
        if (type == REDIS_EOF) break;
        /* Handle SELECT DB opcode as a special case */
        if (type == REDIS_SELECTDB) {
            if (fread(&dbid,4,1,fp) == 0) goto eoferr;
            dbid = ntohl(dbid);
            if (dbid >= (unsigned)server.dbnum) {
                redisLog(REDIS_WARNING,"FATAL: Data file was created with a Redis server compiled to handle more than %d databases. Exiting\n", server.dbnum);
                exit(1);
            }
            dict = server.dict[dbid];
            continue;
        }
        /* Read key */
        if (fread(&klen,4,1,fp) == 0) goto eoferr;
        klen = ntohl(klen);
        if (klen <= REDIS_LOADBUF_LEN) {
            key = buf;
        } else {
            key = malloc(klen);
            if (!key) oom("Loading DB from file");
        }
        if (fread(key,klen,1,fp) == 0) goto eoferr;

        if (type == REDIS_STRING) {
            /* Read string value */
            if (fread(&vlen,4,1,fp) == 0) goto eoferr;
            vlen = ntohl(vlen);
            if (vlen <= REDIS_LOADBUF_LEN) {
                val = vbuf;
            } else {
                val = malloc(vlen);
                if (!val) oom("Loading DB from file");
            }
            if (fread(val,vlen,1,fp) == 0) goto eoferr;
            o = createObject(REDIS_STRING,sdsnewlen(val,vlen));
        } else if (type == REDIS_LIST) {
            /* Read list value */
            uint32_t listlen;
            if (fread(&listlen,4,1,fp) == 0) goto eoferr;
            listlen = ntohl(listlen);
            o = createListObject();
            /* Load every single element of the list */
            while(listlen--) {
                robj *ele;

                if (fread(&vlen,4,1,fp) == 0) goto eoferr;
                vlen = ntohl(vlen);
                if (vlen <= REDIS_LOADBUF_LEN) {
                    val = vbuf;
                } else {
                    val = malloc(vlen);
                    if (!val) oom("Loading DB from file");
                }
                if (fread(val,vlen,1,fp) == 0) goto eoferr;
                ele = createObject(REDIS_STRING,sdsnewlen(val,vlen));
                if (!listAddNodeTail((list*)o->ptr,ele))
                    oom("listAddNodeTail");
                /* free the temp buffer if needed */
                if (val != vbuf) free(val);
                val = NULL;
            }
        } else {
            assert(0 != 0);
        }
        /* Add the new object in the hash table */
        retval = dictAdd(dict,sdsnewlen(key,klen),o);
        if (retval == DICT_ERR) {
            redisLog(REDIS_WARNING,"Loading DB, duplicated key found! Unrecoverable error, exiting now.");
            exit(1);
        }
        /* Iteration cleanup */
        if (key != buf) free(key);
        if (val != vbuf) free(val);
        key = val = NULL;
    }
    fclose(fp);
    return REDIS_OK;

eoferr: /* unexpected end of file is handled here with a fatal exit */
    if (key != buf) free(key);
    if (val != vbuf) free(val);
    redisLog(REDIS_WARNING,"Short read loading DB. Unrecoverable error, exiting now.");
    exit(1);
    return REDIS_ERR; /* Just to avoid warning */
}

/*================================== Commands =============================== */

static void pingCommand(redisClient *c) {
    addReply(c,shared.pong);
}

static void echoCommand(redisClient *c) {
    addReplySds(c,sdscatprintf(sdsempty(),"%d\r\n",(int)sdslen(c->argv[1])));
    addReplySds(c,c->argv[1]);
    addReply(c,shared.crlf);
    c->argv[1] = NULL;
}

static void setGenericCommand(redisClient *c, int nx) {
    int retval;
    robj *o;

    o = createObject(REDIS_STRING,c->argv[2]);
    c->argv[2] = NULL;
    retval = dictAdd(c->dict,c->argv[1],o);
    if (retval == DICT_ERR) {
        if (!nx)
            dictReplace(c->dict,c->argv[1],o);
        else
            decrRefCount(o);
    } else {
        /* Now the key is in the hash entry, don't free it */
        c->argv[1] = NULL;
    }
    server.dirty++;
    addReply(c,shared.ok);
}

static void setCommand(redisClient *c) {
    return setGenericCommand(c,0);
}

static void setnxCommand(redisClient *c) {
    return setGenericCommand(c,1);
}

static void getCommand(redisClient *c) {
    dictEntry *de;
    
    de = dictFind(c->dict,c->argv[1]);
    if (de == NULL) {
        addReply(c,shared.nil);
    } else {
        robj *o = dictGetEntryVal(de);
        
        if (o->type != REDIS_STRING) {
            char *err = "GET against key not holding a string value";
            addReplySds(c,
                sdscatprintf(sdsempty(),"%d\r\n%s\r\n",-((int)strlen(err)),err));
        } else {
            addReplySds(c,sdscatprintf(sdsempty(),"%d\r\n",(int)sdslen(o->ptr)));
            addReply(c,o);
            addReply(c,shared.crlf);
        }
    }
}

static void delCommand(redisClient *c) {
    if (dictDelete(c->dict,c->argv[1]) == DICT_OK)
        server.dirty++;
    addReply(c,shared.ok);
}

static void existsCommand(redisClient *c) {
    dictEntry *de;
    
    de = dictFind(c->dict,c->argv[1]);
    if (de == NULL)
        addReply(c,shared.zero);
    else
        addReply(c,shared.one);
}

static void incrDecrCommand(redisClient *c, int incr) {
    dictEntry *de;
    sds newval;
    long long value;
    int retval;
    robj *o;
    
    de = dictFind(c->dict,c->argv[1]);
    if (de == NULL) {
        value = 0;
    } else {
        robj *o = dictGetEntryVal(de);
        
        if (o->type != REDIS_STRING) {
            value = 0;
        } else {
            char *eptr;

            value = strtoll(o->ptr, &eptr, 10);
        }
    }

    value += incr;
    newval = sdscatprintf(sdsempty(),"%lld",value);
    o = createObject(REDIS_STRING,newval);
    retval = dictAdd(c->dict,c->argv[1],o);
    if (retval == DICT_ERR) {
        dictReplace(c->dict,c->argv[1],o);
    } else {
        /* Now the key is in the hash entry, don't free it */
        c->argv[1] = NULL;
    }
    server.dirty++;
    addReply(c,o);
    addReply(c,shared.crlf);
}

static void incrCommand(redisClient *c) {
    return incrDecrCommand(c,1);
}

static void decrCommand(redisClient *c) {
    return incrDecrCommand(c,-1);
}

static void selectCommand(redisClient *c) {
    int id = atoi(c->argv[1]);
    
    if (selectDb(c,id) == REDIS_ERR) {
        addReplySds(c,"-ERR invalid DB index\r\n");
    } else {
        addReply(c,shared.ok);
    }
}

static void randomkeyCommand(redisClient *c) {
    dictEntry *de;
    
    de = dictGetRandomKey(c->dict);
    if (de == NULL) {
        addReply(c,shared.crlf);
    } else {
        addReply(c,dictGetEntryVal(de));
        addReply(c,shared.crlf);
    }
}

static void keysCommand(redisClient *c) {
    dictIterator *di;
    dictEntry *de;
    sds keys, reply;
    sds pattern = c->argv[1];
    int plen = sdslen(pattern);

    di = dictGetIterator(c->dict);
    keys = sdsempty();
    while((de = dictNext(di)) != NULL) {
        sds key = dictGetEntryKey(de);
        if ((pattern[0] == '*' && pattern[1] == '\0') ||
            stringmatchlen(pattern,plen,key,sdslen(key),0)) {
            keys = sdscatlen(keys,key,sdslen(key));
            keys = sdscatlen(keys," ",1);
        }
    }
    dictReleaseIterator(di);
    keys = sdstrim(keys," ");
    reply = sdscatprintf(sdsempty(),"%lu\r\n",sdslen(keys));
    reply = sdscatlen(reply,keys,sdslen(keys));
    reply = sdscatlen(reply,"\r\n",2);
    sdsfree(keys);
    addReplySds(c,reply);
}

static void dbsizeCommand(redisClient *c) {
    addReplySds(c,
        sdscatprintf(sdsempty(),"%lu\r\n",dictGetHashTableUsed(c->dict)));
}

static void lastsaveCommand(redisClient *c) {
    addReplySds(c,
        sdscatprintf(sdsempty(),"%lu\r\n",server.lastsave));
}

static void saveCommand(redisClient *c) {
    if (saveDb("dump.rdb") == REDIS_OK) {
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
    if (saveDbBackground("dump.rdb") == REDIS_OK) {
        addReply(c,shared.ok);
    } else {
        addReply(c,shared.err);
    }
}

static void shutdownCommand(redisClient *c) {
    redisLog(REDIS_WARNING,"User requested shutdown, saving DB...");
    if (saveDb("dump.rdb") == REDIS_OK) {
        redisLog(REDIS_WARNING,"Server exit now, bye bye...");
        exit(1);
    } else {
        redisLog(REDIS_WARNING,"Error trying to save the DB, can't exit"); 
        addReplySds(c,sdsnew("-ERR can't quit, problems saving the DB\r\n"));
    }
}

static void renameGenericCommand(redisClient *c, int nx) {
    dictEntry *de;
    robj *o;

    /* To use the same key as src and dst is probably an error */
    if (sdscmp(c->argv[1],c->argv[2]) == 0) {
        addReplySds(c,sdsnew("-ERR src and dest key are the same\r\n"));
        return;
    }

    de = dictFind(c->dict,c->argv[1]);
    if (de == NULL) {
        addReplySds(c,sdsnew("-ERR no such key\r\n"));
        return;
    }
    o = dictGetEntryVal(de);
    incrRefCount(o);
    if (dictAdd(c->dict,c->argv[2],o) == DICT_ERR) {
        if (nx) {
            decrRefCount(o);
            addReplySds(c,sdsnew("-ERR destination key exists\r\n"));
            return;
        }
        dictReplace(c->dict,c->argv[2],o);
    } else {
        c->argv[2] = NULL;
    }
    dictDelete(c->dict,c->argv[1]);
    server.dirty++;
    addReply(c,shared.ok);
}

static void renameCommand(redisClient *c) {
    renameGenericCommand(c,0);
}

static void renamenxCommand(redisClient *c) {
    renameGenericCommand(c,1);
}

static void moveCommand(redisClient *c) {
    dictEntry *de;
    sds *key;
    robj *o;
    dict *src, *dst;

    /* Obtain source and target DB pointers */
    src = c->dict;
    if (selectDb(c,atoi(c->argv[2])) == REDIS_ERR) {
        addReplySds(c,sdsnew("-ERR target DB out of range\r\n"));
        return;
    }
    dst = c->dict;
    c->dict = src;

    /* If the user is moving using as target the same
     * DB as the source DB it is probably an error. */
    if (src == dst) {
        addReplySds(c,sdsnew("-ERR source DB is the same as target DB\r\n"));
        return;
    }

    /* Check if the element exists and get a reference */
    de = dictFind(c->dict,c->argv[1]);
    if (!de) {
        addReplySds(c,sdsnew("-ERR no such key\r\n"));
        return;
    }

    /* Try to add the element to the target DB */
    key = dictGetEntryKey(de);
    o = dictGetEntryVal(de);
    if (dictAdd(dst,key,o) == DICT_ERR) {
        addReplySds(c,sdsnew("-ERR target DB already contains the moved key\r\n"));
        return;
    }

    /* OK! key moved, free the entry in the source DB */
    dictDeleteNoFree(src,c->argv[1]);
    server.dirty++;
    addReply(c,shared.ok);
}

static void pushGenericCommand(redisClient *c, int where) {
    robj *ele, *lobj;
    dictEntry *de;
    list *list;
    
    ele = createObject(REDIS_STRING,c->argv[2]);
    c->argv[2] = NULL;

    de = dictFind(c->dict,c->argv[1]);
    if (de == NULL) {
        lobj = createListObject();
        list = lobj->ptr;
        if (where == REDIS_HEAD) {
            if (!listAddNodeHead(list,ele)) oom("listAddNodeHead");
        } else {
            if (!listAddNodeTail(list,ele)) oom("listAddNodeTail");
        }
        dictAdd(c->dict,c->argv[1],lobj);

        /* Now the key is in the hash entry, don't free it */
        c->argv[1] = NULL;
    } else {
        lobj = dictGetEntryVal(de);
        if (lobj->type != REDIS_LIST) {
            decrRefCount(ele);
            addReplySds(c,sdsnew("-ERR push against existing key not holding a list\r\n"));
            return;
        }
        list = lobj->ptr;
        if (where == REDIS_HEAD) {
            if (!listAddNodeHead(list,ele)) oom("listAddNodeHead");
        } else {
            if (!listAddNodeTail(list,ele)) oom("listAddNodeTail");
        }
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
    dictEntry *de;
    list *l;
    
    de = dictFind(c->dict,c->argv[1]);
    if (de == NULL) {
        addReply(c,shared.zero);
        return;
    } else {
        robj *o = dictGetEntryVal(de);
        if (o->type != REDIS_LIST) {
            addReplySds(c,sdsnew("-1\r\n"));
        } else {
            l = o->ptr;
            addReplySds(c,sdscatprintf(sdsempty(),"%d\r\n",listLength(l)));
        }
    }
}

static void lindexCommand(redisClient *c) {
    dictEntry *de;
    int index = atoi(c->argv[2]);
    
    de = dictFind(c->dict,c->argv[1]);
    if (de == NULL) {
        addReply(c,shared.nil);
    } else {
        robj *o = dictGetEntryVal(de);
        
        if (o->type != REDIS_LIST) {
            char *err = "LINDEX against key not holding a list value";
            addReplySds(c,
                sdscatprintf(sdsempty(),"%d\r\n%s\r\n",-((int)strlen(err)),err));
        } else {
            list *list = o->ptr;
            listNode *ln;
            
            ln = listIndex(list, index);
            if (ln == NULL) {
                addReply(c,shared.nil);
            } else {
                robj *ele = listNodeValue(ln);
                addReplySds(c,sdscatprintf(sdsempty(),"%d\r\n",(int)sdslen(ele->ptr)));
                addReply(c,ele);
                addReply(c,shared.crlf);
            }
        }
    }
}

static void popGenericCommand(redisClient *c, int where) {
    dictEntry *de;
    
    de = dictFind(c->dict,c->argv[1]);
    if (de == NULL) {
        addReply(c,shared.nil);
    } else {
        robj *o = dictGetEntryVal(de);
        
        if (o->type != REDIS_LIST) {
            char *err = "POP against key not holding a list value";
            addReplySds(c,
                sdscatprintf(sdsempty(),"%d\r\n%s\r\n",-((int)strlen(err)),err));
        } else {
            list *list = o->ptr;
            listNode *ln;

            if (where == REDIS_HEAD)
                ln = listFirst(list);
            else
                ln = listLast(list);

            if (ln == NULL) {
                addReply(c,shared.nil);
            } else {
                robj *ele = listNodeValue(ln);
                addReplySds(c,sdscatprintf(sdsempty(),"%d\r\n",(int)sdslen(ele->ptr)));
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
    dictEntry *de;
    int start = atoi(c->argv[2]);
    int end = atoi(c->argv[3]);
    
    de = dictFind(c->dict,c->argv[1]);
    if (de == NULL) {
        addReply(c,shared.nil);
    } else {
        robj *o = dictGetEntryVal(de);
        
        if (o->type != REDIS_LIST) {
            char *err = "LRANGE against key not holding a list value";
            addReplySds(c,
                sdscatprintf(sdsempty(),"%d\r\n%s\r\n",-((int)strlen(err)),err));
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
                addReply(c,shared.zero);
                return;
            }
            if (end >= llen) end = llen-1;
            rangelen = (end-start)+1;

            /* Return the result in form of a multi-bulk reply */
            ln = listIndex(list, start);
            addReplySds(c,sdscatprintf(sdsempty(),"%d\r\n",rangelen));
            for (j = 0; j < rangelen; j++) {
                ele = listNodeValue(ln);
                addReplySds(c,sdscatprintf(sdsempty(),"%d\r\n",(int)sdslen(ele->ptr)));
                addReply(c,ele);
                addReply(c,shared.crlf);
                ln = ln->next;
            }
        }
    }
}

static void ltrimCommand(redisClient *c) {
    dictEntry *de;
    int start = atoi(c->argv[2]);
    int end = atoi(c->argv[3]);
    
    de = dictFind(c->dict,c->argv[1]);
    if (de == NULL) {
        addReplySds(c,sdsnew("-ERR no such key\r\n"));
    } else {
        robj *o = dictGetEntryVal(de);
        
        if (o->type != REDIS_LIST) {
            addReplySds(c, sdsnew("-ERR LTRIM against key not holding a list value"));
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
        }
    }
}

/* =================================== Main! ================================ */

int main(int argc, char **argv) {
    initServerConfig();
    initServer();
    if (argc == 2) {
        ResetServerSaveParams();
        loadServerConfig(argv[1]);
        redisLog(REDIS_NOTICE,"Configuration loaded");
    } else if (argc > 2) {
        fprintf(stderr,"Usage: ./redis-server [/path/to/redis.conf]\n");
        exit(1);
    }
    redisLog(REDIS_NOTICE,"Server started");
    if (loadDb("dump.rdb") == REDIS_OK)
        redisLog(REDIS_NOTICE,"DB loaded from disk");
    if (aeCreateFileEvent(server.el, server.fd, AE_READABLE,
        acceptHandler, NULL, NULL) == AE_ERR) oom("creating file event");
    redisLog(REDIS_NOTICE,"The server is now ready to accept connections");
    aeMain(server.el);
    aeDeleteEventLoop(server.el);
    return 0;
}
