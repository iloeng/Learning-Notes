##############################################################################
tiny-httpd-0.1.0 源码阅读
##############################################################################

.. contents::

******************************************************************************
第 2 部分  开始源码阅读
******************************************************************************

2.11 execute_cgi 函数
==============================================================================

这个函数开始的时候分别初始化了 buf ， 防止第一个字符就是 '\n' ， 因为没有初始化的\
时候 ， 内存中可能直接就分配为 '\n' ， 这样会干扰后面的判断 。 

.. code-block:: C  

    [execute_cgi]
    if (strcasecmp(method, "GET") == 0)
        while ((numchars > 0) && strcmp("\n", buf)) /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));

与之前的过程相似 ， 当 method 是 GET 时 ， 只有 numchars 为 0 时才会退出这个 \
while 循环 ， 因为 buf 不可能与 "\n" 相等 ， 即 ``strcmp("\n", buf)`` 不可能为 \
0 ， 因此只能 numchars 为 0 来退出循环 。 而 numchars 只有请求头读完之后才会出现\
空行 ， 这个时候的 numchars 才为 0 ； 因此这个 if 分支就是读取请求头 。 

.. code-block:: C 

    [execute_cgi]
    else /* POST */
    {
        numchars = get_line(client, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf)) {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16]));
            numchars = get_line(client, buf, sizeof(buf));
        }
        if (content_length == -1) {
            bad_request(client);
            return;
        }
    }

当不是 GET 请求的时候 ， 也就是请求是 POST 请求的时候 (这个程序只有 GET 和 POST \
功能) ， 读取一行请求报文 ， 当 while 循环正常执行的时候 ， 说明 POST 请求是正常\
的 。

.. code-block:: 

    POST http://127.0.0.1:7582/color.cgi HTTP/1.1
    Host: 127.0.0.1:7582
    Connection: keep-alive
    Content-Length: 9
    Pragma: no-cache
    Cache-Control: no-cache
    Upgrade-Insecure-Requests: 1
    Origin: http://127.0.0.1:7582
    Content-Type: application/x-www-form-urlencoded
    User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/90.0.4430.72 Safari/537.36
    Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9
    Referer: http://127.0.0.1:7582/
    Accept-Encoding: gzip, deflate, br
    Accept-Language: zh-CN,zh;q=0.9

    color=red

这个是一个完整的 POST 请求报文 ， 包括报文头部和数据 ， 他们通过空格进行分割 。 \
当然这里面有些数据是浏览器自动添加的 。 初始化 buf[15] 为结束符 "\0" ， 因为 \
``Content-Length:`` 的长度为 15 ， 刚好可以填充到 buf[1-14] 空间内 ， 以此来截\
取 ``Content-Length:`` 请求头的长度 。 

第 16 个字符原本是空格 ， 被替换为结束符 "\0" ， 但是第 17 个字符开始就是内容的长\
度 ， ``&(buf[16]`` 就是获取第 17 个字符的起始地址 。 atoi (表示 ascii to \
integer) 函数是把字符串转换成整型数的一个函数 ， 这里就是将第 17 个字符开始的字符\
串转换为数字 。 在这里获取到了请求正文的长度 。 

因此这个 while 循环就是为了获取请求正文的长度 ， 如果长度小于 0 ， 说明请求有问\
题 ， 执行 bad_request 函数 ， 并返回空值 。 

.. code-block:: C 

    [execute_cgi]
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    if (pipe(cgi_output) < 0) {
        cannot_execute(client);
        return;
    }
    if (pipe(cgi_input) < 0) {
        cannot_execute(client);
        return;
    }

当之前的步骤都没有问题的时候 ， server 开始做出回应 ， server 发送 200 相关的字符\
串 ， cgi_output 和 cgi_output 没有初始值 ， pipe 函数用于创建管道 ， 管道是一种\
把两个进程之间的标准输入和标准输出连接起来的机制 ， 成功 ， 返回 0 ， 否则返回 -1 \
。 参数数组包含 pipe 使用的两个文件的描述符 。 fd[0] : 读管道 ， fd[1] : 写管道 \
。 当 cgi_output 和 cgi_input 都是小于 0 时 ， 均执行 cannot_execute 并返回空值 。

.. code-block:: c

    [execute_cgi]
    if ((pid = fork()) < 0) {
        cannot_execute(client);
        return;
    }

fork 用于创建一个新进程 ， 称为子进程 ， 它与进程 （称为系统调用 fork 的进程） 同\
时运行 ， 此进程称为父进程 。 创建新的子进程后 ， 两个进程将执行 fork() 系统调用之\
后的下一条指令 。 子进程使用相同的 pc （程序计数器） ， 相同的 CPU 寄存器 ， 在父\
进程中使用的相同打开文件 。 执行成功的话 ， 父进程返回子进程的 PID ， 子进程返回 \
0 ， 否则返回 -1 。 因此在此处 ， 如果 PID 小于 0 ， 说明 fork 并没有执行成功 。 \
因此执行 cannot_execute 函数 。 

当 PID = 0 时 ， 说明是子进程 。 

.. code-block:: c

    [execute_cgi]
    if (pid == 0) /* child: CGI script */
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        dup2(cgi_output[1], 1);
        dup2(cgi_input[0], 0);
        close(cgi_output[0]);
        close(cgi_input[1]);
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);
        if (strcasecmp(method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        } else { /* POST */
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        execl(path, path, NULL);
        exit(0);
    } else { /* parent */
        close(cgi_output[1]);
        close(cgi_input[0]);
        if (strcasecmp(method, "POST") == 0)
            for (i = 0; i < content_length; i++) {
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        while (read(cgi_output[0], &c, 1) > 0)
            send(client, &c, 1, 0);

        close(cgi_output[0]);
        close(cgi_input[1]);
        waitpid(pid, &status, 0);
    }

这一块的代码有些晕 ， 对 Linux 进程间通信不太了解 。 后面再找时间学习一下 ， 这个\
函数就先到此结束 。 

2.12 bad_request 和 cannot_execute 函数
==============================================================================

.. code-block:: C 

    void bad_request(int client) {
        char buf[1024];

        sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
        send(client, buf, sizeof(buf), 0);
        sprintf(buf, "Content-type: text/html\r\n");
        send(client, buf, sizeof(buf), 0);
        sprintf(buf, "\r\n");
        send(client, buf, sizeof(buf), 0);
        sprintf(buf, "<P>Your browser sent a bad request, ");
        send(client, buf, sizeof(buf), 0);
        sprintf(buf, "such as a POST without a Content-Length.\r\n");
        send(client, buf, sizeof(buf), 0);
    }

    void cannot_execute(int client) {
        char buf[1024];

        sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
        send(client, buf, strlen(buf), 0);
        sprintf(buf, "Content-type: text/html\r\n");
        send(client, buf, strlen(buf), 0);
        sprintf(buf, "\r\n");
        send(client, buf, strlen(buf), 0);
        sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
        send(client, buf, strlen(buf), 0);
    }

这两个函数的步骤很相似 ， 格式化字符串然后发送给已连接的 socket 链接 。 

TinyHTTPd 阅读算是基本完成了 ， 但是仍然有一部分没有完整解析 ， 因为对 Linux 进程\
间通信有些陌生 ， 等后面学习一下再进行补充 。 