##############################################################################
tiny-httpd-0.1.0 源码阅读
##############################################################################

.. contents::

******************************************************************************
第 2 部分  开始源码阅读
******************************************************************************

2.4 accept_request 函数
==============================================================================

在继续向下解析 accept_request 函数之前 ， 在这里先了解一下 HTTP 请求相关的知识 ： 

.. image:: img/2-1.png 

上图是一个 HTTP 请求结构图 ， 客户端发送一个 HTTP 请求到服务器的请求消息包括以下格\
式 ： 请求行 （request line） 、 请求头部 （header） 、 空行和请求数据四个部分组\
成 ， 请求方法就是 GET 、 POST 等方法 。 

上述是客户端请求消息的过程 ， 接下来是服务器响应消息 ： 

.. image:: img/2-2.png 

HTTP 响应也由四个部分组成 ， 分别是 ： 状态行 、 消息报头 、 空行和响应正文 。 此报\
文来自于 TinyHTTPd 服务器请求数据 color=red 时的响应报文 。 其客户端请求如下 ： 

.. image:: img/2-3.png 

可以看到这个请求报文与 HTTP 请求结构图一致 ， 有很多请求头应该是 Chrome 自动添加的 \
。 现在可以回到代码中了 。

.. code-block:: C  

    [void accept_request(int client)]
    while (!ISspace(buf[j]) && (i < sizeof(method) - 1)) {
            method[i] = buf[j];
            i++;
            j++;
    }

既然知道了 HTTP 请求结构图 ， 那么这里的逻辑就很清晰了 ， 就是从请求报文的第一行获取\
请求方法 ， 循环判断 buf 中的数据 ， 一旦为空值 ， 就表示拿到了请求方法 ， 因为第一\
行的第一个空值就是在请求方法之后 。 那这个的 i 和 j 就为 3 ， 注意我这里是请求首页 \
， method 为 GET 。 使用 GDB 调试结果如下 ： 

.. image:: img/2-4.png 

然后在 method 后添加一个空值 '\0' ， 作为字符串的结束标志 。 

.. code-block:: C 

    [void accept_request(int client)]
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
            unimplemented(client);
            return;
    }

``int strcasecmp(const char *s1, const char *s2);`` 函数的功能是忽略大小写比较字\
符串 ， 若参数 s1 和 s2 字符串相等返回 0 ， s1 大于 s2 则返回大于 0 的值 ， s1 小\
于 s2 则返回小于 0 的值 。 

只有 method 与 "GET" 或 "POST" 相等时 ， 才会跳过这个判断 ， 因为相等时返回值为 0 \
， 在与另一个返回值进行与运算 (&&) 的时候值仍然为 0 ， 条件为假 ， 内部的步骤才不会\
继续执行 。 否则就会执行 unimplemented() 函数 ， 并返回空值 。 unimplemented 函数\
这里先不解析 。 等下文解析 。 

.. code-block:: C 

    [void accept_request(int client)]
    if (strcasecmp(method, "POST") == 0)
        cgi = 1;

执行到此处 ， 判断是否是 "POST" 方法 ， 如果是 strcasecmp 函数返回 0 ， 同时将 cgi \
置为 1 ， 方便后面执行 CGI 程序 。 

.. code-block:: C 

    [void accept_request(int client)]
    i = 0;
    while (ISspace(buf[j]) && (j < sizeof(buf)))
        j++;
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf))) {
        url[i] = buf[j];
        i++;
        j++;
    }
    url[i] = '\0';

这里则是从请求报文中提取 URL 。 首先判断 buf[j] 是否是空白字符 ， ISspace 判断某个\
字符是空白字符的时候 ， 返回非 0 数字 ， 否则返回 0 。

.. code-block:: shell

    (gdb) p buf
    $4 = "GET / HTTP/1.1\n", '\000' <repeats 25 times>, "\363\365|\377\377\177\000\000\001", '\000' <repeats 15 times>,

这一段代码开始的时候 ， 将 i 的值重新置为 0 了 ， 而 j 的值仍然是读取 method 之后的\
值 3 ， 因此 buf[3] = " " ， ISspace 会返回一个非 0 数字 ， 因此执行到此分支 ， j \
的值自增 1 为 4 。 

到下面的 while 循环 ， 与之前获取 method 一致 ， 从非空字符读取到空白字符就是 URL \
的值 ， 因此 url[0] = '/' ， 此时 i = 1 ， j = 5 ， 最后 url[1] = '\0' ， 表示 \
url 字符串到此结束 。 

开始进行下一段代码分析 。 

.. code-block:: C 

    [void accept_request(int client)]
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




















