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

.. _`requirements.txt`: requirements.txt

******************************************************************************
第 2 部分  源码阅读准备 
******************************************************************************

2.1 Flask 的设计理念
==============================================================================

2.1.1 "微"框架
------------------------------------------------------------------------------

在官方介绍中 ， Flask 被称为微框架 ， 这里的 "微" 并不意味着 Flask 功能简陋 ， 而\
是指其保留核心且易于扩展 。 有许多 Web 程序不需要后台管理 、 用户认证 、 权限管理 \
， 有些甚至不需要表单或数据库 ， 所以 Flask 并没有内置这类功能 ， 而是把这些功能都\
交给扩展或用户自己实现 。 正因为如此 ， 从只需要渲染模板的小项目 ， 到需要各种功能的\
大项目 ， Flask 几乎能够适应各种情况 。 Flask 的这一设计理念正印证了 《Zen of \
Python》 里的这一句 ： 

    "Simple is better than complex."

2.1.2 两个核心依赖
------------------------------------------------------------------------------

虽然 Flask 保持简单的核心 ， 但它主要依赖两个库 —— Werkzeug 和 Jinja 。 Python \
Web 框架都需要处理 WSGI 交互 ， 而 Werkzeug 本身就是一个非常优秀的 WSGI 工具库 ， \
几乎没有理由不使用它 ， Flask 与 Werkzeug 的联系非常紧密 。 从路由处理 ， 到请求解\
析 ， 再到响应的封装 ， 以及上下文和各种数据结构都离不开 Werkzeug ， 有些函数 （比\
如 redirect 、 abort） 甚至是直接从 Werkzeug 引入的 。 如果要深入了解 Flask 的实\
现原理 ， 必然躲不开 Werkzeug 。 

引入 Jinja2 主要是因为大多数 Web 程序都需要渲染模板 ， 与 Jinja2 集成可以减少大量\
的工作 。 除此之外 ， Flask 扩展常常需要处理模板 ， 而集成 Jinja2 方便了扩展的开发 \
。 不过 ， Flask 并不限制你选择其他模板引擎 ， 比如 Mako \
(http://www.makotemplates.org/) 、 Genshi(http://genshi.edgewall.org/)等 。 

2.1.3 显式程序对象
------------------------------------------------------------------------------

在一些 Python Web 框架中 ， 一个视图函数可能类似这样 ： 

.. code-block:: python 

    from example_framework import route

    @route('/')
    def index():
        return 'Hello World!'

而在 Flask 中，则需要这样 ： 

.. code-block:: python 

    from flask import Flask
    app = Flask(__name__)

    @app.route('/')
    def index():
        return 'Hello World!'

应该看到其中的区别了 ， Flask 中存在一个显式的程序对象 ， 我们需要在全局空间中创建\
它 。 这样设计主要有下面几个原因 ： 

- 前一种方式(隐式程序对象)在同一时间内只能有一个实例存在 ， 而显式的程序对象允许多个\
  程序实例存在 。 
- 允许你通过子类化 Flask 类来改变程序行为 。 
- Flask 需要通过传入的包名称来定位资源(模板和静态文件) 。
- 允许通过工厂函数来创建程序实例 ， 可以在不同的地方传入不同的配置来创建不同的程序实\
  例。
- 允许通过蓝本来模块化程序。

另外 ， 这个设计也印证了 《Zen of Python》 里的这一条 : "Explicit is better \
than implicit." 

2.1.4 本地上下文
------------------------------------------------------------------------------

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

2.1.5 丰富的自定义支持
------------------------------------------------------------------------------

Flask 的灵活不仅体现在易于扩展 ， 不限制项目结构 ， 也体现在其内部的高度可定制化 。 \
比如 ， 我们可以子类化用于创建程序实例的 Flask 类 ， 来改变特定的行为 ： 

.. code-block:: python 

    from flask import Flask
    class MyFlask(Flask)
        pass
    app = MyFlask(__name__)
    ...

除了 Flask 类 ， 还可以自定义请求类和响应类 。 最常用的方式是子类化 Flask 内置的请\
求类和响应类 ， 然后改变一些默认的属性 。 Flask 内部在使用这些类时并不直接写死 ， \
而是使用了定义在 Flask 属性上的中间变量 ， 比如请求类存储在 Flask.request_class \
中 。 如果要使用自己的请求类 ， 那么只需要把请求类赋值给这个属性即可 ： 

.. code-block:: python 

    from flask import Flask, Request
    class MyRequest(Request):
        pass
    app = Flask(__name__)
    app.request_class = MyRequest

同样 ， Flask 允许你使用自定义的响应类 。 在其内部 ， 创建响应对象的 \
make_response() 并不是直接实例化 Response 类 ， 而是实例化被存储在 \
Flask.response_class 属性上的类 ， 默认为 Response 类 。 如果你要自定义响应类 ， \
创建后只需赋值给程序实例的 response_class 属性即可 。 

2.2 Flask 与 WSGI
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

2.2.1 WSGI 程序
------------------------------------------------------------------------------

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
代时将调用这个方法） ， 它返回 yield 语句 。 如果想以类的 **实例** 作为 WSGI 程序 \
， 那么这个类必须实现 __call__ 方法 。

在上面创建的两个简单的 WSGI 程序 ， 你应该感觉很熟悉吧 ！ 事实上 ， 这两个程序的实\
际功能和书开始介绍的 Flask 程序 hello 完全相同 。 

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

2.2.2 WSGI 服务器
------------------------------------------------------------------------------

程序编写好了 ， 现在需要一个 WSGI 服务器来运行它 。 作为 WSGI 服务器的实现示例 ， \
Python 提供了一个 wsgiref 库 ， 可以在开发时使用 。 以 hello() 函数为例 ， 在函数\
定义的下面添加如下代码 。  

.. code-block:: python

    from wsgiref.simple_server import make_server

    def hello(environ, start_response):
        ...

    server = make_server('localhost', 5000, hello)
    server.serve_forever()

这里使用 make_server(host, port, application) 方法创建了一个本地服务器 ， 分别传\
入主机地址 、 端口和可调用对象 （即 WSGI 程序） 作为参数 。 最后使用 \
serve_forever() 方法运行它 。 

WSGI 服务器启动后 ， 它会监听本地机的对应端口 （我们设置的 5000） 。 当接收到请求\
时 ， 它会把请求报文解析为一个 environ 字典 ， 然后调用 WSGI 程序提供的可调用对象 \
， 传递这个字典作为参数 ， 同时传递的另一个参数是一个 start_response 函数 。 目前对\
于 start_response 函数有些不太理解 。 

在命令行使用 Python 解释器执行 hello.py ， 这会启动我们创建的 WSGI 服务器 ： 

.. code-block:: bash

    python hello.py

然后像以前一样在浏览器中访问 http://localhost:5000 时 ， 这个 WSGI 服务器接收到这\
个请求 ， 接着调用 hello() 函数 ， 并传递 environ 和 start_response 参数 ， 最后\
把 hello() 函数的返回值处理为 HTTP 响应返回给客户端 。 这一系列工作完成后 ， 我们就\
会在浏览器看到一行 "Hello，Web！" 。

下面是这个程序的变式 ， 通过从 environ 字典获取请求 URL 来修改响应的内容 。 

.. code-block:: python 

    def hello(environ, start_response):
        status = '200 OK'
        response_headers = [('Content-type', 'text/html')]
        start_response(status, response_headers)
        name = environ['PATH_INFO'][1:] or 'web'
        return [b'<h1>Hello, %s!</h1>' % name]

从 environ 字典里获取路径中根地址后的字符作为名字 ： environ['PATH_INFO'][1：] \
， 然后插入到响应的字符串里 。 这时在浏览器中访问 localhost:5000/Grey ， 则会看到\
浏览器显示一行 "Hello,Grey！" 。 

到此 ， 大概了解了 wsgi 的相关信息 ， 如下是我的总结 ： 

- 函数式 ： 接收两个参数 ， 并返回一个 list
- 类形式 ： 如果以类实例作为 WSGI 程序 ， 类必须实现 __call__ 方法

wsgi 也大致了解了一下 ， 继续往下学习 。 

2.2.3 中间件
------------------------------------------------------------------------------

WSGI 允许使用中间件 (Middleware) 包装 (wrap) 程序 ， 为程序在被调用前添加额外的设\
置和功能 。 当请求发送来后 ， 会先调用包装在可调用对象外层的中间件 。 这个特性经常被\
用来解耦程序的功能 ， 这样可以将不同功能分开维护 ， 达到分层的目的 ， 同时也根据需要\
嵌套 。 如下代码是一个简单的例子 。 

.. code-block:: python 

    from wsgiref.simple_server import make_server
    
    def hello(environ, start_response):
        status = '200 OK'
        response_headers = [('Content-type', 'text/html')]
        start_response(status, response_headers)
        return [b'<h1>Hello, web!</h1>']
    
    class MyMiddleware(object):

        def __init__(self, app):
            self.app = app
    
        def __call__(self, environ, start_response):
            def custom_start_response(status, headers, exc_info=None):
                headers.append(('A-CUSTOM-HEADER', 'Nothing'))
                return start_response(status, headers)
            return self.app(environ, custom_start_response)
    
    wrapped_app = MyMiddleware(hello)
    server = make_server('localhost', 5000, wrapped_app)
    server.serve_forever()

中间件接收可调用对象作为参数 。 这个可调用对象也可以是被其他中间件包装的可调用对象 \
。 中间件可以层层叠加 ， 形成一个 "中间件堆栈" ， 最后才会调用到实际的可调用对象 。 

使用类定义的中间件必须实现 __call__ 方法 ， 接收 environ 和 start_response 对象作\
为参数 ， 最后调用传入的可调用对象 ， 并传递这两个参数 。 这个 MyMiddleware 中间件\
其实并没有做什么 ， 只是向首部添加了一个无意义的自定义字段 。 最后传入可调用对象 \
hello 函数来实例化这个中间件 ， 获得包装后的程序实例 wrapped_app 。 

因为 Flask 中实际的 WSGI 可调用对象是 Flask.wsgi_app() 方法 ， 因此 ， 如果我们自\
己实现了中间件 ， 那么最佳的方式是嵌套在这个 wsgi_app 对象上 ， 比如 ： 

.. code-block:: python 

    class MyMiddleware(object):
        pass

    app = Flask(__name__)
    app.wsgi_app = MyMiddleware(app.wsgi_app)

作为 WSGI 工具集 ， Werkzeug 内置了许多方便的中间件 ， 可以用来为程序添加额外的功\
能 。 比如 ， 能够为程序添加性能分析器的 \
werkzeug.contrib.profiler.ProfilerMiddleware 中间件 ， 这个中间件可以在处理请求\
时进行性能分析 ， 作用和 Flask-DebugToolbar 提供的分析器基本相同 ； 另外 ， 支持多\
应用调度的 werkzeug.wsgi.DispatcherMiddleware 中间件则可以让你将多个 WSGI 程序作\
为一个 "程序集" 同时运行 ， 你需要传入多个程序实例 ， 并为这些程序设置对应的 URL 前\
缀或子域名来分发请求 。 

2.3 Flask 工作流程与机制
==============================================================================

本节深入到 Flask 的源码来了解请求 、 响应 、 路由处理等功能是如何实现的 。 首先 ， \
我们会对 Flask 应用启动流程和请求响应循环进行分析 。 

2.3.1 Flask 中的请求相应循环
------------------------------------------------------------------------------

对于 Flask 的工作流程 ， 最好的了解方法是从启动程序的脚本开始 ， 跟着程序调用的脚步\
一步步深入代码的内部 。 在本节 ， 我们会了解请求 - 响应循环在 Flask 中是如何处理的 \
： 从程序开始运行 ， 第一个请求进入 ， 再到返回生成的响应 。 

为了方便进行单步调试 ， 在这里先创建一个简单的 Flask 程序 :

.. code-block:: python

    from flask import Flask
    app = Flask(__name__)

    @app.route('/')
    def hello():
        return 'Hello, Flask!' # 在这一行设置断点

首先在 hello 程序的 index 视图中渲染模板这一行设置断点 ， 然后 PyCharm 中运行调试 。

2.3.1.1 程序启动
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

目前有两种方法启动开发服务器 ， 一种是在命令行中使用 flask run 命令 （会调用 \
flask.cli.run_command() 函数） ， 另一种是使用在新版本中被弃用的 \
flask.Flask.run() 方法 。 不论是 run_command() 函数 ， 还是新版本中用于运行程序\
的 run() 函数 ， 它们都在最后调用了 werkzeug.serving 模块中的 run_simple() 函数 \
， 其代码如下 ：

未完待续 ...

下一篇文章 ： `下一篇`_ 

.. _`下一篇`: flask-0.1-02.rst
