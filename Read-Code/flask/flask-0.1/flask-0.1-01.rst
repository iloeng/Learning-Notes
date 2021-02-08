##############################################################################
Python Web 模块之 Flask v0.1
##############################################################################

.. contents::

******************************************************************************
第 1 部分  源码阅读环境 
******************************************************************************

1.1 下载源码
==============================================================================

首先从 github 上 clone 源码仓库 ： 

.. code-block:: bash

    git clone https://github.com/pallets/flask.git
    git checkout 0.1

1.2 Python 环境
==============================================================================

我一般使用 Anaconda 进行 Python 虚拟环境管理 ， 因此使用 Anaconda 创建一个 \
Python 2.7 环境 ， 然后激活该环境 ， 并在命令行中执行 ：

.. code-block:: bash

    pip install -i https://pypi.douban.com/simple -r requirements.txt 

这个 `requirements.txt`_ 文件中是 flask 0.1 版本依赖的两个模块 。 

.. _`requirements.txt`: /requirements.txt

1.3 本地上下文
==============================================================================

在多线程环境下 ， 要想让所有视图函数都获取请求对象 。 最直接的方法就是在调用视图函数\
时将所有需要的数据作为参数传递进去 ， 但这样一来程序逻辑就变得冗余且不易于维护 。 另\
一种方法是将这些数据设为全局变量 ， 但是如果直接将请求对象设为全局变量 ， 那么必然会\
在不同的线程中导致混乱 （非线程安全） 。 本地线程 （thread locals） 的出现解决了这\
些问题 。

本地线程就是一个全局对象 ， 你可以使用一种特定线程且线程安全的方式来存储和获取数据 \
。 也就是说 ， 同一个变量在不同的线程内拥有各自的值 ， 互不干扰 。 实现原理其实很简\
单 ， 就是根据线程的ID来存取数据 。 Flask 没有使用标准库的 threading.local() ， \
而是使用了 Werkzeug 自己实现的本地线程对象 werkzeug.local.Local() ， 后者增加了\
对 Greenlet 的优先支持 。 

Flask 使用本地线程来让上下文代理对象全局可访问 ， 比如 request 、 session 、 \
current_app 、 g ， 这些对象被称为本地上下文对象 （context locals） 。 因此 ， \
在不基于线程 、 greenlet 或单进程实现的并发服务器上 ， 这些代理对象将无法正常工作 \
， 但好在仅有少部分服务器不被支持 。 Flask 的设计初衷是为了让传统 Web 程序的开发更\
加简单和迅速 ， 而不是用来开发大型程序或异步服务器的 。 但是 Flask 的可扩展性却提供\
了无限的可能性 ， 除了使用扩展 ， 我们还可以子类化 Flask 类 ， 或是为程序添加中间\
件 。

在 Flask 中存在三种状态 ， 分别是程序设置状态 （application setup state） 、 程序\
运行状态 （application runtime state） 和请求运行状态 （request runtime state） 。

选自 《Flask Web开发实战：入门、进阶与原理解析（李辉著 ）》 ， 按照该书中的第 16 章\
的步骤 ， 先了解一下本地上下文的数据结构 。 

在 Flask 0.1 代码中 ， 本地上下文信息如下 ： 

.. code-block:: python 

    # context locals
    _request_ctx_stack = LocalStack()
    current_app = LocalProxy(lambda: _request_ctx_stack.top.app)
    request = LocalProxy(lambda: _request_ctx_stack.top.request)
    session = LocalProxy(lambda: _request_ctx_stack.top.session)
    g = LocalProxy(lambda: _request_ctx_stack.top.g)

我有些不解的是 LocalProxy 里面的匿名函数 ， 需要查一下资料 。

.. code-block:: python 

    >>> from flask import Flask, current_app, g, request, session
    >>> app = Flask(__name__)
    >>> current_app, g, request, session
    (<LocalProxy unbound>,
    <LocalProxy unbound>,
    <LocalProxy unbound>,
    <LocalProxy unbound>)

    上述代码为书中的代码 ， 我用 0.1 版的代码无法使用 ， 实际为 ：

    >>> from flask import Flask, current_app, g, request, session
    >>> app = Flask(__name__)
    >>> current_app, g, request, session
    (Traceback (most recent call last):
    File "<stdin>", line 1, in <module>
    File "D:\Anaconda3\envs\source\lib\site-packages\werkzeug\local.py", line 321, in __repr__
        obj = self._get_current_object()
    File "D:\Anaconda3\envs\source\lib\site-packages\werkzeug\local.py", line 306, in _get_current_object
        return self.__local()
    File "flask.py", line 660, in <lambda>
        current_app = LocalProxy(lambda: _request_ctx_stack.top.app)
    AttributeError: 'NoneType' object has no attribute 'app'

而我在实际中并没有成功以 0.1 版的代码进入到三种状态 ， 因此我只以我的实际情况进行记\
录 。 如下 ：

.. code-block:: python 

    >>> from flask import Flask, current_app, g, request, session, _request_ctx_stack
    >>> app = Flask(__name__)
    >>> ctx = app.test_request_context()
    >>> ctx.__enter__()
    >>> ctx
    <flask._RequestContext object at 0x0000000002C08470>
    >>> current_app
    <flask.Flask object at 0x0000000002C19358>
    >>> request
    <Request 'http://localhost/' [GET]>
    >>> session
    None
    >>> g
    <flask._RequestGlobals object at 0x000000000378E128>
    >>> _request_ctx_stack     # 本地上下文堆栈
    <werkzeug.local.LocalStack object at 0x0000000003779048>
    >>> _request_ctx_stack._local.__storage__   # 
    {18532: {'stack': [<flask._RequestContext object at 0x0000000002C08470>]}}
    >>>
    >>> _request_ctx_stack.top
    <flask._RequestContext object at 0x0000000002C08470>
    >>> _request_ctx_stack.top.__dict__
    {'g': <flask._RequestGlobals object at 0x000000000378E128>, 'url_adapter': <werkzeug.routing.MapAdapter object at 0x000000000377EB70>, 'app': <flask.Flask object at 0x0000000002C19358>, 'request': <Request 'http://localhost/' [GET]>, 'session': None, 'flashes': None}

从上述代码交互中可以看到 'g' 就是全局变量 ， app 是当前的 Flask 对象 ， request \
是当前的链接 ， session 为空 。 由于这部分与 wsgi 的 werkzeug 相关 ， 只能先放下 \
。 大概了解了 _request_ctx_stack ， current_app ， request ， session 和 g 的数\
据结构 ， 那么就接着阅读源代码 。 当然有个前提是先了解一下 wsgi 。

1.4 WSGI 相关信息
==============================================================================

Flask 的核心扩展 Werkzeug 是一个 WSGI 工具库 。 WSGI 指 Python Web Server \
Gateway Interface ， 它是为了让 Web 服务器与 Python 程序能够进行数据交流而定义的\
一套接口标准 / 规范 。 

WSGI 的具体定义在 PEP 333 （https://www.python.org/dev/peps/pep-0333/） 中可以\
看到 。 WSGI 的新版本在 PEP 3333 中发布 ， 新版本主要增加了 Python 3 支持 \
（https://www.python.org/dev/peps/pep-3333/） 。 

客户端和服务器端进行沟通遵循了 HTTP 协议 ， 可以说 HTTP 就是它们之间沟通的语言 。 \
从 HTTP 请求到我们的 Web 程序之间 ， 还有另外一个转换过程 —— 从 HTTP 报文到 WSGI \
规定的数据格式 。 WSGI 则可以视为 WSGI 服务器和我们的 Web 程序进行沟通的语言 。 \
WSGI 是开发 Python Web 程序的标准 ， 所有的 Python Web 框架都需要按照 WSGI 的规范\
来编写程序 。 

根据 WSGI 的规定 ， Web 程序 （或被称为 WSGI 程序） 必须是一个可调用对象 \
（callable object） 。 这个可调用对象接收两个参数 ：
    
- environ ： 包含了请求的所有信息的字典 。 
- start_response ： 需要在可调用对象中调用的函数 ， 用来发起响应 ， 参数是状态码 \
  、 响应头部等 。 

WSGI 服务器会在调用这个可调用对象时传入这两个参数 。 另外 ， 这个可调用对象还要返回\
一个可迭代 （iterable） 的对象 。 这个可调用对象可以是函数 、 方法 、 类或是实现了 \
__call__ 方法的类实例 ， 下面我们分别借助简单的实例来了解最主要的两种实现 ： 函数和\
类 。 

使用 Python 函数或 class 实现的 WSGI 程序 ：

.. code-block:: python

    from wsgiref.simple_server import make_server


    def hello(environ, start_response):
        status = '200 OK'
        response_headers = [('Content-type', 'text/html')]
        start_response(status, response_headers)
        name = environ['PATH_INFO'][1:] or 'web'
        return [b'<h1>Hello, %s!</h1>' % name]


    class AppClass:

        def __init__(self, environ, start_response):
            self.environ = environ
            self.start = start_response

        def __iter__(self):
            status = '200 OK'
            response_headers = [('Content-type', 'text/html')]
            self.start(status, response_headers)
            yield b'<h1>Hello, Web!</h1>'


    # server = make_server('localhost', 5000, hello)
    server = make_server('localhost', 5000, AppClass)
    server.serve_forever()

这里的 hello() 函数就是我们的可调用对象 ， 也就是我们的 Web 程序 。 hello() 的末尾\
返回一行问候字符串 ， 注意这是一个列表 。 

根据 WSGI 的定义 ， 请求和响应的主体应该为字节串 (bytestrings) ， 即 Python 2 中\
的 str 类型 。 在 Python 3 中字符串默认为 unicode 类型 ， 因此需要在字符串前添加 \
b 前缀 ， 将字符串声明为 bytes 类型 。 这里为了兼容两者 ， 统一添加了 b 前缀 。 

类形式的可调用对象如代码中的 AppClass ， 注意 ， 类中实现了 __iter__ 方法 （类被迭\
代时将调用这个方法） ， 它返回 yield 语句 。 如果想以类的实例作为 WSGI 程序 ， 那么\
这个类必须实现 __call__ 方法 。

在上面我们创建了两个简单的 WSGI 程序 ， 你应该感觉很熟悉吧 ！ 事实上 ， 这两个程序\
的实际功能和书开始介绍的 Flask 程序 hello 完全相同 。 

Flask 也是 Python Web 框架 ， 自然也要遵循 WSGI 规范 ， 所以 Flask 中也会实现类似\
的 WSGI 程序 ， 只不过对请求和响应的处理要丰富完善得多 。 在 Flask 中 ， 这个可调用\
对象就是我们的程序实例 app ， 我们创建 app 实例时调用的 Flask 类就是另一种可调用对\
象形式 —— 实现了 __call__ 方法的类 ： 

.. code-block:: python 

    class Flask(_PackageBoundObject):
        ...
        def wsgi_app(self, environ, start_response):
            with self.request_context(environ):
            rv = self.preprocess_request()
            if rv is None:
                rv = self.dispatch_request()
            response = self.make_response(rv)
            response = self.process_response(response)
            return response(environ, start_response)

        def __call__(self, environ, start_response):
        """Shortcut for :attr:`wsgi_app`."""
            return self.wsgi_app(environ, start_response)

这个 __call__ 方法内部调用了 wsgi_app() 方法 ， 请求进入和响应的返回就发生在这里 \
， WSGI 服务器通过调用这个方法来传入请求数据 ， 获取返回的响应 ， 后面会详细介绍 。 

程序编写好了 ， 现在需要一个 WSGI 服务器来运行它 。 作为 WSGI 服务器的实现示例 ， \
Python 提供了一个 wsgiref 库 ， 可以在开发时使用 。 以 hello() 函数为例 ， 在函数\
定义的下面添加上述代码 。  

这里使用 make_server(host, port, application) 方法创建了一个本地服务器 ， 分别传\
入主机地址 、 端口和可调用对象 （即 WSGI 程序） 作为参数 。 最后使用 \
serve_forever() 方法运行它 。 WSGI 服务器启动后 ， 它会监听本地机的对应端口 （我们\
设置的 5000） 。 当接收到请求时 ， 它会把请求报文解析为一个 environ 字典 ， 然后调\
用 WSGI 程序提供的可调用对象 ， 传递这个字典作为参数 ， 同时传递的另一个参数是一个 \
start_response 函数 。 目前对于 start_response 函数有些不太理解 。 

在命令行使用 Python 解释器执行 hello.py ， 这会启动我们创建的 WSGI 服务器 ： 

.. code-block:: bash

    python hello.py

然后像以前一样在浏览器中访问 http://localhost:5000 时 ， 这个 WSGI 服务器接收到这\
个请求 ， 接着调用 hello() 函数 ， 并传递 environ 和 start_response 参数 ， 最后\
把 hello() 函数的返回值处理为 HTTP 响应返回给客户端 。 这一系列工作完成后 ， 我们就\
会在浏览器看到一行 "Hello，Web！" 。

到此 ， 大概了解了 wsgi 的相关信息 ， 如下是我的总结 ： 

- 函数式 ： 接收两个参数 ， 并返回一个 list
- 类形式 ： 必须实现 __call__ 方法

wsgi 也大致了解了一下 ， 继续阅读源代码 。 

