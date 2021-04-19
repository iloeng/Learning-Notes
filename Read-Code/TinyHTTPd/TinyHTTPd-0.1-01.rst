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

