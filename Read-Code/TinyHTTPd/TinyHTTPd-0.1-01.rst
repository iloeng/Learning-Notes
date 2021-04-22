##############################################################################
tiny-httpd-0.1.0 源码阅读
##############################################################################

.. contents::

******************************************************************************
第 1 部分  源码阅读环境 
******************************************************************************

虽然我一直使用的语言是 Python ， 但是我一直对 C 语言敬畏有加 ， 除了大学里面因为指\
针的缘故 ， 更是因为随着现在接触的越多 ， 就更加的钦佩底层相关的事物 。 因此不能放过\
这个语言 。

目前为止 ， 我在空余时间学习 C 语言 ， 而从何处开始学习是一个问题 ， 但是一直学习 C \
基础知识也不好 ， 会使人烦躁 ， 因此我结合源代码开始学习 ， 当然 C 的一些基础知识我\
是知道的 ， 那就从网络流传已久的 10 个 C 开源项目开始 ， 第一个就拿小型的 \
Tinyhttpd 项目开始 。 

我的源码阅读环境为 ： WSL2 + CLion on Windows 10 。

补充一下 ， 我的 WSL2 中的 GCC 版本为 10.2.1 20210110 ：

.. code-block:: c

    ┌──(home㉿Station)-[/mnt/c/Users/Darker]
    └─$ gcc -v
    Using built-in specs.
    COLLECT_GCC=gcc
    COLLECT_LTO_WRAPPER=/usr/lib/gcc/x86_64-linux-gnu/10/lto-wrapper
    OFFLOAD_TARGET_NAMES=nvptx-none:amdgcn-amdhsa:hsa
    OFFLOAD_TARGET_DEFAULT=1
    Target: x86_64-linux-gnu
    Configured with: ../src/configure -v --with-pkgversion='Debian 10.2.1-6' --with-bugurl=file:///usr/share/doc/gcc-10/README.Bugs --enable-languages=c,ada,c++,go,brig,d,fortran,objc,obj-c++,m2 --prefix=/usr --with-gcc-major-version-only --program-suffix=-10 --program-prefix=x86_64-linux-gnu- --enable-shared --enable-linker-build-id --libexecdir=/usr/lib --without-included-gettext --enable-threads=posix --libdir=/usr/lib --enable-nls --enable-bootstrap --enable-clocale=gnu --enable-libstdcxx-debug --enable-libstdcxx-time=yes --with-default-libstdcxx-abi=new --enable-gnu-unique-object --disable-vtable-verify --enable-plugin --enable-default-pie --with-system-zlib --enable-libphobos-checking=release --with-target-system-zlib=auto --enable-objc-gc=auto --enable-multiarch --disable-werror --with-arch-32=i686 --with-abi=m64 --with-multilib-list=m32,m64,mx32 --enable-multilib --with-tune=generic --enable-offload-targets=nvptx-none=/build/gcc-10-Km9U7s/gcc-10-10.2.1/debian/tmp-nvptx/usr,amdgcn-amdhsa=/build/gcc-10-Km9U7s/gcc-10-10.2.1/debian/tmp-gcn/usr,hsa --without-cuda-driver --enable-checking=release --build=x86_64-linux-gnu --host=x86_64-linux-gnu --target=x86_64-linux-gnu --with-build-config=bootstrap-lto-lean --enable-link-mutex
    Thread model: posix
    Supported LTO compression algorithms: zlib zstd
    gcc version 10.2.1 20210110 (Debian 10.2.1-6)

1.1 下载源码
==============================================================================

首先下载源代码 。 这个项目的路径为 ： \
https://sourceforge.net/projects/tinyhttpd ， 当然我在这个仓库里面附带了已经代码\
格式化的源代码 。 

1.2 编译源码
==============================================================================

当源代码下载之后 ， 我们需要编译源代码 ， 使其能正常运行在 WSL2 上 ， 并且没有错误产\
生 。

这个代码包可以在 Linux 上正常编译 ， 不需要按照代码注释中所说的步骤 。 可以直接执行 ：

.. code-block:: shell

    gcc -g -W -Wall -pthread -o httpd httpd.c

会有一些警告信息 ， 目前我们不用管它 。 然后执行 httpd ， 将这个 httpd 服务器运行起\
来 ， 查看是否有错误 。

1.3 环境整理
==============================================================================

编译后的产物在我的实际电脑上运行后出现如下错误 ：

1. 浏览器打开主页面空白

2. perl 脚本无法执行

首先第一条问题的解决办法为 ： 去除 index.html 的可执行权限 ， 确保两个 CGI 脚本具有\
可执行权限 。

.. code-block:: shell

    chmod 600 index.html 
    chmod a+x check.cgi 
    chmox a+x color.cgi 

第二个问题如果类似于 \
``Can't locate CGI.pm in @INC (you may need to install the CGI module)`` ， \
则说明 Perl 需要安装 CGI 模块 。 

首先执行 ``perl -e shell -MCPAN`` 进入 CPAN shell ， 然后执行在 CPAN 中执行 \
``install CGI`` 就可以安装 CGI 模块了 ， 需要等待 ， 耗时较久 。

.. code-block:: shell

    cpan[1]> install CGI 

同时需要修改两个 CGI 的头部 ， 使其指向真实的 perl 路径 ： 

.. code-block:: shell

    #!/usr/local/bin/perl -Tw
    改为
    #!/usr/bin/perl -Tw

到这里 ， 我这里的环境能正常执行了 。 

******************************************************************************
第 2 部分  开始源码阅读
******************************************************************************

由于是 C 语言项目 ， 直接从 main 函数开始看起 。 

2.1 main 函数
==============================================================================

main 函数的源代码如下 ： 

.. code-block:: c 

    int main(void) {
        int server_sock = -1;
        u_short port = 0;
        int client_sock = -1;
        struct sockaddr_in client_name;
        int client_name_len = sizeof(client_name);
        pthread_t newthread;

        server_sock = startup(&port);
        printf("httpd running on port %d\n", port);

        while (1) {
            client_sock = accept(server_sock,
                                (struct sockaddr *) &client_name,
                                &client_name_len);
            if (client_sock == -1)
                error_die("accept");
            /* accept_request(client_sock); */
            if (pthread_create(&newthread, NULL, accept_request, client_sock) != 0)
                perror("pthread_create");
        }

        close(server_sock);

        return (0);
    }

首先初始化变量 server_sock 和 client_sock 均为 -1 ， 初始化端口号 port 为无符号短\
整型 ， 值为 0 。 client_name 是一个 sockaddr_in 结构体 ， 用于网络通信 ， 其结构\
体如下 ： 

.. code-block:: C

    [/usr/include/netinet/in.h]
    typedef uint16_t in_port_t;
    struct sockaddr_in
    {
        __SOCKADDR_COMMON (sin_);   // 此处简化为  sa_family_t sin_family;
        in_port_t sin_port;			/* Port number.  */
        struct in_addr sin_addr;		/* Internet address.  */

        /* Pad to size of `struct sockaddr`.  */
        unsigned char sin_zero[sizeof (struct sockaddr)
                - __SOCKADDR_COMMON_SIZE
                - sizeof (in_port_t)
                - sizeof (struct in_addr)];  // 这一大串计算完毕后是 8 ， unsigned char sin_zero[8]
    };
    typedef uint32_t in_addr_t;
    struct in_addr
    {
        in_addr_t s_addr;
    };

    [/usr/include/x86_64-linux-gnu/bits/stdint-uintn.h]
    typedef __uint16_t uint16_t;
    typedef __uint32_t uint32_t;

    [/usr/include/x86_64-linux-gnu/bits/types.h]
    typedef unsigned short int __uint16_t;
    typedef unsigned int __uint32_t;

    [/usr/include/x86_64-linux-gnu/bits/sockaddr.h]
    typedef unsigned short int sa_family_t;
    #define	__SOCKADDR_COMMON(sa_prefix) \
        sa_family_t sa_prefix##family

    [/usr/include/x86_64-linux-gnu/bits/socket.h]
    struct sockaddr
    {
        __SOCKADDR_COMMON (sa_);	/* Common data: address family and length.  */
        // 此处相当于 sa_family_t sa_family
        char sa_data[14];		/* Address data.  */
    };
        
整理之后的结构体如下 ， 原始的有点儿不太易于阅读 ：

.. code-block:: C

    [/usr/include/netinet/in.h]
    struct sockaddr_in
    {
        unsigned short sin_family;
        unsigned short sin_port;			/* Port number.  */
        unsigned int sin_addr;		/* Internet address.  */

        /* Pad to size of `struct sockaddr`.  */
        unsigned char sin_zero[8];  
    };

    [/usr/include/x86_64-linux-gnu/bits/socket.h]
    struct sockaddr
    {
        unsigned short sa_family;	/* Common data: address family and length.  */
        char sa_data[14];		/* Address data.  */
    };

在 sockaddr 结构体中 ， sa_family 是通信类型 ， 最常用的值是 "AF_INET" ， \
sa_data 14 字节 ， 包含套接字中的目标地址和端口信息 ， 其缺点就是把目标地址和端口信\
息混在一起了 ； 而 sockaddr_in 结构体解决了 sockaddr 的缺陷 ， 它把 port 和 addr \
分开存储在两个变量中 。 client_name 就是一个 sockaddr_in 类型的变量 。 

client_name_len 是 client_name 所占用的字节数 。 newthread 是 pthread_t 类型的\
数据 ； server_sock 被赋值为 startup 函数值 ， 然后打印出 server 监听的端口号 ； \
然后进入一个死循环 ， 在没有异常的情况下 ， 使这个服务一直运行 ；

在这个死循环中 ， client_sock 被赋值为 accept 函数值 ， 当 client_sock == -1 时 \
， 说明运行出错了 ， 退出当前子程序 。 

如果 pthread_create 创建线程出错 ， 即函数返回值不等于 0 ， perror 打印出系统错误\
信息 。 

最终关闭服务器 server_sock ， 并返回 0 。 

2.2 startup 函数
==============================================================================

在 main 函数中只是简单的一笔带过 startup 函数 ， 在这一小节 ， 详细分析一下 ：

.. code-block:: c

    typedef unsigned short int __u_short;
    typedef __u_short u_short;
    int startup(u_short *port) {
        int httpd = 0;
        struct sockaddr_in name;

        httpd = socket(PF_INET, SOCK_STREAM, 0);
        if (httpd == -1)
            error_die("socket");
        memset(&name, 0, sizeof(name));
        name.sin_family = AF_INET;
        name.sin_port = htons(*port);
        name.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(httpd, (struct sockaddr *) &name, sizeof(name)) < 0)
            error_die("bind");
        if (*port == 0) /* if dynamically allocating a port */
        {
            int namelen = sizeof(name);
            if (getsockname(httpd, (struct sockaddr *) &name, &namelen) == -1)
                error_die("getsockname");
            *port = ntohs(name.sin_port);
        }
        if (listen(httpd, 5) < 0)
            error_die("listen");
        return (httpd);
    }

startup 函数是一个指向 port (端口) 的无符号 short 指针 。 从上文中知道这个 port \
初始为 0 。

进入函数内部 ， httpd 初始化为值为 0 的 int 型数据 ； name 是 sockaddr_in 结构数\
据 ； 

然后 httpd 被赋值为 socket 函数值 。 socket 函数用于创建套接字 ：

.. code-block:: c

    /* Create a new socket of type TYPE in domain DOMAIN, using
    protocol PROTOCOL.  If PROTOCOL is zero, one is chosen automatically.
    Returns a file descriptor for the new socket, or -1 for errors.  */
    extern int socket (int __domain, int __type, int __protocol) __THROW;

这里的 __domain 指明通信域 ， 如 PF_UNIX (unix 域) ， PF_INET (IPv4) ， \
PF_INET6 (IPv6) 等 。

type 为数据传输方式 / 套接字类型 ， 常用的有 SOCK_STREAM （流格式套接字 / 面向连接\
的套接字） 和 SOCK_DGRAM （数据报套接字 / 无连接的套接字） 。 SOCK_STREAM 是数据\
流 ， 一般是 TCP/IP 协议的编程 ， SOCK_DGRAM 是数据包 ， 是 UDP 协议网络编程 。 

protocol 表示传输协议 ， 常用的有 IPPROTO_TCP 和 IPPTOTO_UDP ， 分别表示 TCP 传\
输协议和 UDP 传输协议 。 使用 0 则根据前两个参数使用默认的协议 。 

一般情况下有了 __domain 和 type 两个参数就可以创建套接字了 ， 操作系统会自动\
推演出协议类型 ， 除非遇到这样的情况 ： 有两种不同的协议支持同一种地址类型和数据传输\
类型 。 如果我们不指明使用哪种协议 ， 操作系统是没办法自动推演的 。 

socket 函数正常时 ， 返回新套接字的文件描述符 ； 否则返回 -1 。 因此代码中用 httpd \
与 -1 进行比较 ， 判断套接字是否建立正常 。 

之后使用 memset 函数将以 name 为起始地址的内存中的值设置为 0 ， 内存块的大小为 \
name 结构体的大小 。 之后设置相应的结构体中的值 ， sin_family 设为 AF_INET ， \
AF_INET 实际上是 PF_INET ， 代表的是 IPv4 ； sin_port 设置为 ``htons(*port)`` \
， htons 的作用是将一个无符号短整型数值转换为网络字节序 ， 即大端模式 \
(big-endian) ， 返回值是 TCP/IP 网络字节顺序 ， 这个函数的参数是 16 位无符号整数 \
， 刚好是两个字节 ， 一个字节只能存储 8 位 2 进制数 ， 而计算机的端口数量是 65536 \
个 ， 也就是 2^16 ， 两个字节 。 大端模式的符号位的判定固定为第一个字节 ， 容易判断\
正负 ； sin_addr.s_addr 设置为 ``htonl(INADDR_ANY)`` ， htonl 函数用于将主机数转\
换成无符号长整型的网络字节顺序 。 本函数将一个 32 位数从主机字节顺序转换成网络字节顺\
序 。 这里 htonl 参数设置为 INADDR_ANY 表示不管连接哪个服务器 IP 都能连接上 ， 不\
管服务器上有多少块网卡 ， 有多少个 IP ， 只要是向其中一个 IP 和指定的端口发送消息 \
， 服务器就能接收到消息 。 

.. code-block:: C 

    /* Address to accept any incoming messages.  */
    #define	INADDR_ANY		((in_addr_t) 0x00000000)

然后执行到 bind 函数 ， bind 函数能够将套接字文件描述符 、 端口号和 IP 绑定到一起 \
， 对于 TCP 服务器来说绑定的就是服务器自己的 IP 和端口 。

.. code-block:: C 

    /* Give the socket FD the local address ADDR (which is LEN bytes long).  */
    extern int bind (int __fd, __CONST_SOCKADDR_ARG __addr, socklen_t __len) __THROW;

    # define __CONST_SOCKADDR_ARG	const struct sockaddr *

函数的参数 __fd 表示的是 socket 函数创建的通信文件描述符 ； __addr 表示 \
``struct sockaddr`` 的地址 ， 用于设定要绑定的 IP 和端口 ； __len 表示所指定的结\
构体变量的大小 ； 

在 bind 步骤处 ， 如果正常绑定 ， 则返回值为 0 ， 否则返回 -1 ， 表示不成功 。 不成\
功时打印出失败信息 ， 并退出程序 。 在这里绑定了表示动态端口的 0 ， 实际上会自动找到\
一个可用的端口 ， 而 ``*port`` 的值仍为 0 。

然后判断端口号 ``*port`` 的值是不是 0 ， 为 0 说明需要动态分配端口号 。 然后通过 \
getsockname 函数获取套接字的名字 。 因此如果 getsockname 执行失败返回 -1 说明获\
取 socket 绑定的地址信息失败 ， 打印出信息并退出程序 。 如果正常获取到信息 ， 将当\
前绑定的端口信息转换为主机字节顺序的数字 ， 并赋值给 ``*port`` 。 

使用的是 ntohs 函数 ， 作用是将一个 16 位数由网络字节顺序转换为主机字节顺序 。

之后使用 listen 函数监听套接字上的连接请求 。 第一个参数就是套接字文件描述符 ， 第\
二个参数指定了内核为此套接字排队的最大连接个数 。 listen 成功时返回 0 ， 错误时返\
回 -1 。 错误就打印错误信息 "listen" ， 表明是在这一步出错的 。 最后返回了 socket \
id 。 

2.3 error_die 函数
==============================================================================

error_die 函数的功能很容易理解 ， 其代码如下 ： 

.. code-block:: C 

    void error_die(const char *sc) {
        perror(sc);
        exit(1);
    }

error_die 函数调用了两个函数 ： perror 和 exit 。 

perror(s) 用来将上一个函数发生错误的原因输出到标准设备 (stderr) 。 参数 s 所指的字\
符串会先打印出 ， 后面再加上错误原因字符串 。 此错误原因依照全局变量 errno 的值来决\
定要输出的字符串 。 

打印出发送错误的原因之后 ， 再用 exit 函数退出当前程序 。 

到此返回到 main 函数中 。 同时 port 变量也被赋值为真实的端口数 ， 并被打印出来 。 

2.4 accept_request 函数
==============================================================================

在解析 accept_request 函数之前 ， 需要先解析一下 mian 函数的死循环的一些步骤 。 

client_sock 是 accept 函数的结果 ， accept 函数会提取出所监听套接字的等待连接队列\
中第一个连接请求 ， 创建一个新的套接字 ， 并返回指向该套接字的文件描述符 。 新建立的\
套接字不在监听状态 ， 原来所监听的套接字也不受该系统调用的影响 。 

也就是说 accept 函数会从 server_sock 套接字中提取第一个连接的请求 ， 创建一个新的\
套接字 ， 并返回指向该套接字的文件描述符 ， 即 client_sock 。 以我的理解 ， 就是一\
个客户端请求 。 执行成功时返回文件描述符 ， 失败返回 -1 。

然后使用 pthread_create 创建处理这个客户端请求的进程 。 

.. code-block:: C 

    /* Create a new thread, starting with execution of START-ROUTINE
    getting passed ARG.  Creation attributed come from ATTR.  The new
    handle is stored in *NEWTHREAD.  */
    extern int pthread_create (pthread_t *__restrict __newthread,
                const pthread_attr_t *__restrict __attr,
                void *(*__start_routine) (void *),
                void *__restrict __arg) __THROWNL __nonnull ((1, 3));

__start_routine 是一个函数指针 ， 在 main 函数中 ， __start_routine 指向的是 \
accept_request 函数 ， 即新建的进程就是用来执行这个函数的 。 

pthread_create 执行成功就返回 0 ， 失败返回错误代码 。 

那么进入这一节的重点 ： 

.. code-block:: C 

    /**********************************************************************/
    /* A request has caused a call to accept() on the server port to
    * return.  Process the request appropriately.
    * Parameters: the socket connected to the client */
    /**********************************************************************/
    void accept_request(int client) {
        char buf[1024];
        int numchars;
        char method[255];
        char url[255];
        char path[512];
        size_t i, j;
        struct stat st;
        int cgi = 0; /* becomes true if server decides this is a CGI
                        * program */
        char *query_string = NULL;

        numchars = get_line(client, buf, sizeof(buf));
        i = 0;
        j = 0;
        while (!ISspace(buf[j]) && (i < sizeof(method) - 1)) {
            method[i] = buf[j];
            i++;
            j++;
        }
        method[i] = '\0';

        if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
            unimplemented(client);
            return;
        }

        if (strcasecmp(method, "POST") == 0)
            cgi = 1;

        i = 0;
        while (ISspace(buf[j]) && (j < sizeof(buf)))
            j++;
        while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf))) {
            url[i] = buf[j];
            i++;
            j++;
        }
        url[i] = '\0';

        if (strcasecmp(method, "GET") == 0) {
            query_string = url;
            while ((*query_string != '?') && (*query_string != '\0'))
                query_string++;
            if (*query_string == '?') {
                cgi = 1;
                *query_string = '\0';
                query_string++;
            }
        }

        sprintf(path, "htdocs%s", url);
        if (path[strlen(path) - 1] == '/')
            strcat(path, "index.html");
        if (stat(path, &st) == -1) {
            while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
                numchars = get_line(client, buf, sizeof(buf));
            not_found(client);
        } else {
            if ((st.st_mode & S_IFMT) == S_IFDIR)
                strcat(path, "/index.html");
            if ((st.st_mode & S_IXUSR) ||
                (st.st_mode & S_IXGRP) ||
                (st.st_mode & S_IXOTH))
                cgi = 1;
            if (!cgi)
                serve_file(client, path);
            else
                execute_cgi(client, path, method, query_string);
        }

        close(client);
    }

这个函数的代码有些长 ， 慢慢解析 。 

buf[1024] 是一个 1 KB 大小的内存空间 ； numchars 是字符计数 ； method[255] 、 \
url[255] 和 path[512] 分别表示请求方法 、 URL 链接以及路径 ， 长度分别是 255 、 \
255 和 512 ； i , j 是 ``size_t`` 类型 ， 目前作用未知 ； st 是一个 stat 结构体 \
， struct stat 这个结构体是用来描述一个 linux 文件系统中的文件属性的结构 ； cgi \
是 CGI 程序的标识 ， 如果是 CGI 程序 ， 它的值会置为 1 ， 初始为 0 (假值) ； \
``*query_string`` 初始为空 (NULL) 。 

然后进入正常的步骤处理中 。 首先使用 get_line 函数获取一个 HTTP 请求中的第一行数据 \
， 正常情况下 ， 它读取了这个请求报文的第一行 ， 并将其存入到 buf[1024] 中 ， 最后\
返回这一行有多少个 bytes ； 否则返回 null 。 

然后初始化 i 和 j 为 0 。 开始循环读取 buf[1024] 这个 1KB 空间中存储的数据 ， 当然\
只读取了 254 字节的数据 ， 因为有 ``i < sizeof(method) - 1`` 约束条件 。 读取前 \
254 非空白字符的数据赋值到 method 数组里面 。 因为它使用了 ISspace 宏 ： 

.. code-block:: C 

    #define ISspace(x) isspace((int)(x))

    isspace(int c) 检查所传的字符是否是空白字符。
    ' '     (0x20)    space (SPC) 空格符
    '\t'    (0x09)    horizontal tab (TAB) 水平制表符    
    '\n'    (0x0a)    newline (LF) 换行符
    '\v'    (0x0b)    vertical tab (VT) 垂直制表符
    '\f'    (0x0c)    feed (FF) 换页符
    '\r'    (0x0d)    carriage return (CR) 回车符

未完待续 ...

下一篇文章 ： `下一篇`_ 

.. _`下一篇`: TinyHTTPd-0.1-02.rst
