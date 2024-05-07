###############################################################################
Python 标准库
###############################################################################

.. contents::

本次以 Python 3.7 为例。

下文来自于 `Python 官方文档`_ 。 

.. _`Python 官方文档` : https://docs.python.org/zh-cn/3.7/reference/index.html#reference-index

`Python 语言参考`_ 描述了 Python 语言的具体语法和语义 ， 这份库参考则介绍了与 \
Python 一同发行的标准库 。 它还描述了通常包含在 Python 发行版中的一些可选组件 。 

Python 标准库非常庞大 ， 所提供的组件涉及范围十分广泛 ， 正如以下内容目录所显示的 \
。 这个库包含了多个内置模块 (以 C 编写) ， Python 程序员必须依靠它们来实现系统级功\
能 ， 例如文件 I/O ， 此外还有大量以 Python 编写的模块 ， 提供了日常编程中许多问题\
的标准解决方案 。 其中有些模块经过专门设计 ， 通过将特定平台功能抽象化为平台中立的 \
API 来鼓励和加强 Python 程序的可移植性 。

Windows 版本的 Python 安装程序通常包含整个标准库 ， 往往还包含许多额外组件 。 对于\
类 Unix 操作系统 ， Python 通常会分成一系列的软件包 ， 因此可能需要使用操作系统所提\
供的包管理工具来获取部分或全部可选组件 。

在这个标准库以外还存在成千上万并且不断增加的其他组件 (从单独的程序 、 模块 、 软件包\
直到完整的应用开发框架) ， 访问 `Python 包索引`_ 即可获取这些第三方包 。 

.. _`Python 语言参考`: https://docs.python.org/zh-cn/3.7/reference/index.html#reference-index

.. _`Python 包索引`: https://pypi.org/

总的目录结构如下 : 

- 概述
    - 可用性注释

- 内置函数

- 内置常量
    - 由 site 模块添加的常量

- 内置类型
    - 逻辑值检测
    - 布尔运算 --- and, or, not
    - 比较运算
    - 数字类型 --- int, float, complex
    - 迭代器类型
    - 序列类型 --- list, tuple, range
    - 文本序列类型 --- str
    - 二进制序列类型 --- bytes, bytearray, memoryview
    - 集合类型 --- set, frozenset
    - 映射类型 --- dict
    - 上下文管理器类型
    - 其他内置类型
    - 特殊属性

- 内置异常
    - 基类
    - 具体异常
    - 警告
    - 异常层次结构

- 文本处理服务
    - string --- 常见的字符串操作
    - re --- 正则表达式操作
    - difflib --- 计算差异的辅助工具
    - textwrap --- 文本自动换行与填充
    - unicodedata --- Unicode 数据库
    - stringprep --- 因特网字符串预备
    - readline --- GNU readline 接口
    - rlcompleter --- GNU readline 的补全函数

- 二进制数据服务
    - struct --- 将字节串解读为打包的二进制数据
    - codecs --- 编解码器注册和相关基类

- 数据类型
    - datetime --- 基本的日期和时间类型
    - calendar --- 日历相关函数
    - collections --- 容器数据类型
    - collections.abc --- 容器的抽象基类
    - heapq --- 堆队列算法
    - bisect --- 数组二分查找算法
    - array --- 高效的数值数组
    - weakref --- 弱引用
    - types --- 动态类型创建和内置类型名称
    - copy --- 浅层 (shallow) 和深层 (deep) 复制操作
    - pprint --- 数据美化输出
    - reprlib --- 另一种 repr() 实现
    - enum --- 枚举类型支持

- 数字和数学模块
    - numbers --- 数字的抽象基类
    - math --- 数学函数
    - cmath ——关于复数的数学函数
    - decimal --- 十进制定点和浮点运算
    - fractions --- 分数
    - random --- 生成伪随机数
    - statistics --- 数学统计函数

- 函数式编程模块
    - itertools --- 为高效循环而创建迭代器的函数
    - functools --- 高阶函数和可调用对象上的操作
    - operator --- 标准运算符替代函数

- 文件和目录访问
    - pathlib --- 面向对象的文件系统路径
    - os.path --- 常见路径操作
    - fileinput --- 迭代来自多个输入流的行
    - stat --- 解析 stat() 结果
    - filecmp --- 文件及目录的比较
    - tempfile --- 生成临时文件和目录
    - glob --- Unix 风格路径名模式扩展
    - fnmatch --- Unix 文件名模式匹配
    - linecache --- 随机读写文本行
    - shutil --- 高阶文件操作
    - macpath --- Mac OS 9 路径操作函数

- 数据持久化
    - pickle —— Python 对象序列化
    - copyreg --- 注意 pickle 支持函数
    - shelve --- Python 对象持久化
    - marshal --- 内部 Python 对象序列化
    - dbm --- Unix "数据库" 接口
    - sqlite3 --- SQLite 数据库 DB-API 2.0 接口模块

- 数据压缩和存档
    - zlib --- 与 gzip 兼容的压缩
    - gzip --- 对 gzip 格式的支持
    - bz2 --- 对 bzip2 压缩算法的支持
    - lzma --- 用 LZMA 算法压缩
    - zipfile --- 使用ZIP存档
    - tarfile --- 读写tar归档文件

- 文件格式
    - csv --- CSV 文件读写
    - configparser --- 配置文件解析器
    - netrc --- netrc 文件处理
    - xdrlib --- 编码与解码 XDR 数据
    - plistlib --- 生成与解析 Mac OS X .plist 文件

- 加密服务
    - hashlib --- 安全哈希与消息摘要
    - hmac --- 基于密钥的消息验证
    - secrets --- 生成安全随机数字用于管理密码

- 通用操作系统服务
    - os --- 操作系统接口模块
    - io --- 处理流的核心工具
    - time --- 时间的访问和转换
    - argparse --- 命令行选项、参数和子命令解析器
    - getopt --- C 风格的命令行选项解析器
    - 模块 logging --- Python 的日志记录工具
    - logging.config --- 日志记录配置
    - logging.handlers --- 日志处理
    - getpass --- 便携式密码输入工具
    - curses --- 终端字符单元显示的处理
    - curses.textpad --- 用于 curses 程序的文本输入控件
    - curses.ascii --- 用于 ASCII 字符的工具
    - curses.panel --- curses 的 panel 栈扩展
    - platform --- 获取底层平台的标识数据
    - errno --- 标准 errno 系统符号
    - ctypes --- Python 的外部函数库

- 并发执行
    - threading --- 基于线程的并行
    - multiprocessing --- 基于进程的并行
    - concurrent 包
    - concurrent.futures --- 启动并行任务
    - subprocess --- 子进程管理
    - sched --- 事件调度器
    - queue --- 一个同步的队列类
    - _thread --- 底层多线程 API
    - _dummy_thread --- _thread 的替代模块
    - dummy_threading --- 可直接替代 threading 模块。

- contextvars 上下文变量
    - 上下文变量
    - 手动上下文管理
    - asyncio 支持

- 网络和进程间通信
    - asyncio --- 异步 I/O
    - socket --- 底层网络接口
    - ssl --- 套接字对象的TLS/SSL封装
    - select --- Waiting for I/O 完成
    - selectors --- 高级 I/O 复用库
    - asyncore --- 异步socket处理器
    - asynchat --- 异步 socket 指令/响应 处理器
    - signal --- 设置异步事件处理程序
    - mmap --- 内存映射文件支持

- 互联网数据处理
    - email --- 电子邮件与 MIME 处理包
    - json --- JSON 编码和解码器
    - mailcap --- Mailcap 文件处理
    - mailbox --- Manipulate mailboxes in various formats
    - mimetypes --- Map filenames to MIME types
    - base64 --- Base16, Base32, Base64, Base85 数据编码
    - binhex --- 对binhex4文件进行编码和解码
    - binascii --- 二进制和 ASCII 码互转
    - quopri --- 编码与解码经过 MIME 转码的可打印数据
    - uu --- 对 uuencode 文件进行编码与解码

- 结构化标记处理工具
    - html --- 超文本标记语言支持
    - html.parser --- 简单的 HTML 和 XHTML 解析器
    - html.entities --- HTML 一般实体的定义
    - XML处理模块
    - xml.etree.ElementTree --- ElementTree XML API
    - xml.dom --- The Document Object Model API
    - xml.dom.minidom --- Minimal DOM implementation
    - xml.dom.pulldom --- Support for building partial DOM trees
    - xml.sax --- Support for SAX2 parsers
    - xml.sax.handler --- Base classes for SAX handlers
    - xml.sax.saxutils --- SAX 工具集
    - xml.sax.xmlreader --- Interface for XML parsers
    - xml.parsers.expat --- Fast XML parsing using Expat

- 互联网协议和支持
    - webbrowser --- 方便的Web浏览器控制器
    - cgi --- Common Gateway Interface support
    - cgitb --- 用于 CGI 脚本的回溯管理器
    - wsgiref --- WSGI Utilities and Reference Implementation
    - urllib --- URL 处理模块
    - urllib.request --- 用于打开 URL 的可扩展库
    - urllib.response --- urllib 使用的 Response 类
    - urllib.parse --- Parse URLs into components
    - urllib.error --- urllib.request 引发的异常类
    - urllib.robotparser --- robots.txt 语法分析程序
    - http --- HTTP 模块
    - http.client --- HTTP 协议客户端
    - ftplib --- FTP 协议客户端
    - poplib --- POP3 protocol client
    - imaplib --- IMAP4 protocol client
    - nntplib --- NNTP protocol client
    - smtplib ---SMTP协议客户端
    - smtpd --- SMTP 服务器
    - telnetlib --- Telnet client
    - uuid --- UUID objects according to RFC 4122
    - socketserver --- A framework for network servers
    - http.server --- HTTP 服务器
    - http.cookies --- HTTP状态管理
    - http.cookiejar —— HTTP 客户端的 Cookie 处理
    - xmlrpc --- XMLRPC 服务端与客户端模块
    - xmlrpc.client --- XML-RPC client access
    - xmlrpc.server --- Basic XML-RPC servers
    - ipaddress --- IPv4/IPv6 操作库

- 多媒体服务
    - audioop --- Manipulate raw audio data
    - aifc --- Read and write AIFF and AIFC files
    - sunau --- 读写 Sun AU 文件
    - wave --- 读写WAV格式文件
    - chunk --- 读取 IFF 分块数据
    - colorsys --- 颜色系统间的转换
    - imghdr --- 推测图像类型
    - sndhdr --- 推测声音文件的类型
    - ossaudiodev --- Access to OSS-compatible audio devices

- 国际化
    - gettext --- 多语种国际化服务
    - locale --- 国际化服务

- 程序框架
    - turtle --- 海龟绘图
    - cmd --- 支持面向行的命令解释器
    - shlex --- Simple lexical analysis
    - Tk图形用户界面(GUI)
    - tkinter --- Tcl/Tk的Python接口
    - tkinter.ttk --- Tk主题小部件
    - tkinter.tix --- Extension widgets for Tk
    - tkinter.scrolledtext --- 滚动文字控件
    - IDLE
    - 其他图形用户界面（GUI）包

- 开发工具
    - typing --- 类型标注支持
    - pydoc --- 文档生成器和在线帮助系统
    - doctest --- 测试交互性的Python示例
    - unittest --- 单元测试框架
    - unittest.mock --- 模拟对象库
    - unittest.mock 上手指南
    - 2to3 - 自动将 Python 2 代码转为 Python 3 代码
    - test --- Python回归测试包
    - test.support --- Utilities for the Python test suite
    - test.support.script_helper --- Utilities for the Python execution tests

- 调试和分析
    - bdb --- Debugger framework
    - faulthandler --- Dump the Python traceback
    - pdb --- Python的调试器
    - Python 分析器
    - timeit --- 测量小代码片段的执行时间
    - trace --- 跟踪Python语句执行
    - tracemalloc --- 跟踪内存分配

- 软件打包和分发
    - distutils --- 构建和安装 Python 模块
    - ensurepip --- Bootstrapping the pip installer
    - venv --- 创建虚拟环境
    - zipapp --- Manage executable Python zip archives

- Python运行时服务
    - sys --- 系统相关的参数和函数
    - sysconfig --- Provide access to Python's configuration information
    - builtins --- 内建对象
    - __main__ --- 顶层脚本环境
    - warnings --- Warning control
    - dataclasses --- 数据类
    - contextlib --- 为 with语句上下文提供的工具
    - abc --- 抽象基类
    - atexit --- 退出处理器
    - traceback --- 打印或检索堆栈回溯
    - __future__ --- Future 语句定义
    - gc --- 垃圾回收器接口
    - inspect --- 检查对象
    - site —— 指定 Site 的配置钩子

- 自定义 Python 解释器
    - code --- 解释器基础类
    - codeop --- 编译Python代码

导入模块
    - zipimport --- 从 Zip 存档中导入模块
    - pkgutil --- 包扩展模块工具
    - modulefinder --- 查找脚本使用的模块
    - runpy --- Locating and executing Python modules
    - importlib --- import 的实现

- Python 语言服务
    - parser --- 访问 Python 解析树
    - ast --- 抽象语法树
    - symtable --- Access to the compiler's symbol tables
    - symbol --- 与 Python 解析树一起使用的常量
    - token --- 与Python解析树一起使用的常量
    - keyword --- 检验Python关键字
    - tokenize -- 对 Python 代码使用的标记解析器
    - tabnanny --- 模糊缩进检测
    - pyclbr --- Python 模块浏览器支持
    - py_compile --- 编译 Python 源文件
    - compileall --- Byte-compile Python libraries
    - dis --- Python 字节码反汇编器
    - pickletools --- pickle 开发者工具集

- 杂项服务
    - formatter --- 通用格式化输出

- Windows系统相关模块
    - msilib --- Read and write Microsoft Installer files
    - msvcrt --- 来自 MS VC++ 运行时的有用例程
    - winreg --- Windows 注册表访问
    - winsound --- Sound-playing interface for Windows

- Unix 专有服务
    - posix --- 最常见的 POSIX 系统调用
    - pwd --- 用户密码数据库
    - spwd --- The shadow password database
    - grp --- 组数据库
    - crypt --- Function to check Unix passwords
    - termios --- POSIX 风格的 tty 控制
    - tty --- 终端控制功能
    - pty --- 伪终端工具
    - fcntl --- The fcntl and ioctl system calls
    - pipes --- 终端管道接口
    - resource --- Resource usage information
    - nis --- Sun 的 NIS (黄页) 接口
    - Unix syslog 库例程

- 被取代的模块
    - optparse --- 解析器的命令行选项
    - imp --- Access the import internals

- 未创建文档的模块
    - 平台特定模块

从文本处理服务开始看起 。 内置的库一般都是由 C 编写的模块 。 

本篇文章是概览

未完待续 ...