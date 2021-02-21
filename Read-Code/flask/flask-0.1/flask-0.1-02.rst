##############################################################################
Python Web 模块之 Flask v0.1
##############################################################################

.. contents::

******************************************************************************
第 2 部分  源码阅读准备 
******************************************************************************

2.3 Flask 工作流程
==============================================================================

2.3.2 路由系统
------------------------------------------------------------------------------

2.3.2.2 URL 匹配
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

在上面的 Werkzeug 路由注册代码示例中 ， 我们创建了路由表 m ， 并使用 add() 方法添\
加了三个路由规则 。 现在 ， 来看看如何在 Werkzeug 中进行 URL 匹配 ， URL 匹配的示\
例如下所示 ： 

.. code-block:: bash

    >>> from werkzeug.routing import Map, Rule
    >>> m = Map()
    >>> rule1 = Rule('/', endpoint='index')
    >>> rule2 = Rule('/downloads/', endpoint='downloads/index')
    >>> rule3 = Rule('/downloads/<int:id>', endpoint='downloads/show')
    >>> m
    Map([[]])
    >>> m.add(rule1)
    >>> m.add(rule2)
    >>> m.add(rule3)
    >>> m
    Map([[<Rule '/' -> index>,
    <Rule '/downloads/' -> downloads/index>,
    <Rule '/downloads/<id>' -> downloads/show>]])
    >>> urls = m.bind('example.com')
    >>> urls.match('/', 'GET')
    ('index', {})
    >>> urls.match('/downloads/42')
    ('downloads/show', {'id': 42})
    >>> urls.match('/downloads')
    Traceback (most recent call last):
    File "<stdin>", line 1, in <module>
    File "C:\Anaconda3\envs\python27\lib\site-packages\werkzeug\routing.py", line 1261, in match
        url_quote(path_info.lstrip('/'), self.map.charset)
    werkzeug.routing.RequestRedirect: 301: Moved Permanently
    >>> urls.match('/missing')
    Traceback (most recent call last):
    File "<stdin>", line 1, in <module>
    File "C:\Anaconda3\envs\python27\lib\site-packages\werkzeug\routing.py", line 1302, in match
        raise NotFound()
    werkzeug.exceptions.NotFound: 404: Not Found
    >>>

Map.bind() 方法和 Map.bind_to_environ() 都会返回一个 MapAdapter 对象 ， 它负责匹\
配和构建 URL 。 MapAdapter 类的 match 方法用来判断传入的 URL 是否匹配 Map 对象中\
存储的路由规则 （存储在 self.map._rules 列表中） 。 上面的例子中分别展示了几种常见\
的匹配情况 。 匹配成功后会返回一个包含 URL 端点和 URL 变量的元组 。 

为了确保 URL 的唯一 ， Werkzeug 使用下面的规则来处理尾部斜线问题 ： 当你定义的 URL \
规则添加了尾部斜线时 ， 用户访问未加尾部斜线的 URL 时会被自动重定向到正确的 URL ； \
反过来 ， 如果定义的 URL 不包含尾部斜线 ， 用户访问的 URL 添加了尾部斜线则会返回 \
404 错误 。 MapAdapter 类的 build() 方法用于创建 URL ， 我们用来生成 URL 的 \
url_for() 函数内部就是通过 build() 方法实现的 。 下面是一个简单的例子 ： 

.. code-block:: bash

    接着上文 ：
    >>> urls.build('index', {})
    '/'
    >>> urls.build('downloads/show', {'id': 42})
    '/downloads/42'
    >>> urls.build('downloads/show', {'id': 42}, force_external=True)
    'http://example.com/downloads/42'
    >>>

关于 Werkzeug 的路由系统 ， 这里只是简单介绍 ， 具体可以查看 Werkzeug 的文档 \
（http://werkzeug.pocoo.org/docs/latest/routing/） 及相关代码 。 

在上一节 ， 注册路由后 ， 两个对应关系分别存储到 url_map 和 view_functions 中 ， \
前者存储了 URL 到端点的映射关系 ， 后者则存储了端点和视图函数的映射关系 。 下面我们\
会了解在客户端发送请求时 ， Flask 是如何根据请求的 URL 找到对应的视图函数的 。 在上\
一节分析 Flask 中的请求响应循环时 ， 我们曾说过 ， 请求的处理最终交给了 \
dispatch_request() 方法 ， 这个方法如代码清单所示 :

.. code-block:: python  

    [flask.py]

    class Flask(object):

        def dispatch_request(self):
            try:
                endpoint, values = self.match_request()
                return self.view_functions[endpoint](**values)
            except HTTPException, e:
                handler = self.error_handlers.get(e.code)
                if handler is None:
                    return e
                return handler(e)
            except Exception, e:
                handler = self.error_handlers.get(500)
                if self.debug or handler is None:
                    raise
                return handler(e)

从名字可以看出来 ， 这个方法负责请求调度 （request dispatching） 。 正是 \
dispatch_request() 方法实现了从请求的 URL 找到端点 ， 再从端点找到对应的视图函数并\
调用的过程 。 view_functions 在注册路由时 ， 由 Rule 类表示的 rule 对象由 \
route() 装饰器传入的参数创建 。 如上文中的描述 ： view_function 是 Flask 类中定义\
的一个字典 ， 它存储了端点和视图函数的映射关系 。 

而这里先调用 match_request() 方法得到处理的 endpoint 和 values ， 如下示例代码 \
， 如果我请求的是 http://localhost:5000/hello/1234 ， 则结果为 ： endpoint=\
'hello' ， values={'name':'1234'} ， 调试信息如下图 ： 

.. code-block:: python

    @app.route('/hello/<name>/test', methods=['POST', 'GET'])
    def hello1(name):
        if name == "Test":
            return 'Test'
        else:
            return 'hello'


    @app.route('/hello/<name>', methods=['POST', 'GET'])
    def hello2(name):
        if name == "Test":
            return 'Test'
        else:
            return 'hello'

.. image:: img/2-1.png

如果我请求的是 http://localhost:5000/hello/1234/test ， 则结果为 ： endpoint=\
'hello' ， values={'name':'1234'} ， 调试信息如下图 ：

.. image:: img/2-2.png

由此可见 endpoint 就是视图函数的名称 ， values 则是注册路由时 ， 路径的可变参数的名\
称与值组成的字典 。 

那么我们来仔细看一下 view_functions 对象 ， view_functions 在 Flask 对象初始化的\
时候是空字典 ： 

.. code-block:: python 

    class Flask(object):

        def __init__(self, package_name):
            ...
            self.view_functions = {}
            ...

第一次出现变化的是在添加路由的时候 ， 即在 Flask.route() 函数内部出现了首次变化 。 

.. code-block:: python  

    [flask.py]

    class Flask(object):

        def route(self, rule, **options):
            def decorator(f):
                self.add_url_rule(rule, f.__name__, **options)
                self.view_functions[f.__name__] = f
                return f
            return decorator

将试图函数装饰一下 ， 把视图函数本身对象复制给以视图函数名为 key ， 形式如下 ： 

:: 

    {'func_name': func(Object)}

因此在 dispatch_request 函数最后一行 \
``return self.view_functions[endpoint](**values)`` 中 ， \
self.view_functions[endpoint] 代表的是视图函数对象本身 ， 后面的 ``(**values)`` \
可以表示为 endpoint_obj(name=value) ， 即是执行视图函数 。 

虽然已经通过调试知道 match_request 函数的执行结果 ， 但还需要通过源码理解一番 。 

.. code-block:: python  

    [flask.py]

    class Flask(object):

        def match_request(self):
            rv = _request_ctx_stack.top.url_adapter.match()
            request.endpoint, request.view_args = rv
            return rv

通过上面的代码可以看到 ， 最终是调用了请求的 match() 方法来获取到 endpoint 和参数 \
， 而调用者 url_adapter = url_map.bind_to_environ(environ) ， 在 \
_RequestContext 类的初始化函数中可以看到 ： 

.. code-block:: python 

    class _RequestContext(object):

        def __init__(self, app, environ):
            self.app = app
            self.url_adapter = app.url_map.bind_to_environ(environ)
            self.request = app.request_class(environ)
            self.session = app.open_session(self.request)
            self.g = _RequestGlobals()
            self.flashes = None

self.url_adapter = app.url_map.bind_to_environ(environ) ， 也就是说实际获取 \
endpoint 与参数是通过调用 url_map.bind_to_environ(environ).match() 来获取的 。 \
通过前面的介绍我们已经知道 ， url_map 中存储的是 url 与 endpoint 之间的映射关系 \
， 这种映射关系是通过 @app.route() 进行指定的 。 而 environ 为单次请求信息 ， 内部\
包含请求的 url 。 可以理解为存储信息的对象 url_map 绑定特定的请求信息 environ ， \
然后进行匹配 match() ， 即可得到请求对应的 endpoint 和参数 value 。 也因此说明 \
match_request 在本地上下文中使用 ， 每次请求 url 创建请求上下午对象时都会执行该函\
数 。 

可以看到 endpoint 和 view_args 属性就在这个方法中创建 。 这个方法调用了 \
_request_ctx_stack.top.url_adapter.match() 来获取 endpoint 和 view_args 。 这\
里的 url_adapter 属性在 _RequestContext 的构造函数中定义 ， 其值为 \
app.url_map.bind_to_environ(environ) 

.. code-block:: python 

    [flask.py]

    class _RequestContext(object):
        """The request context contains all request relevant information.  It is
        created at the beginning of the request and pushed to the
        `_request_ctx_stack` and removed at the end of it.  It will create the
        URL adapter and request object for the WSGI environment provided.
        """

        def __init__(self, app, environ):
            self.app = app
            self.url_adapter = app.url_map.bind_to_environ(environ)
            self.request = app.request_class(environ)
            self.session = app.open_session(self.request)
            self.g = _RequestGlobals()
            self.flashes = None

        def __enter__(self):
            _request_ctx_stack.push(self)

        def __exit__(self, exc_type, exc_value, tb):
            # do not pop the request stack if we are in debug mode and an
            # exception happened.  This will allow the debugger to still
            # access the request object in the interactive shell.
            if tb is None or not self.app.debug:
                _request_ctx_stack.pop()

我们知道 url_map 属性是一个 Map 对象 ， 可以看出它最后调用了 bind() 或 \
bind_to_environ() 方法 ， 最终会返回一个 MapAdapter 类实例 。 

match_request() 方法通过调用 MapAdapter.match() 方法来匹配请求 URL ， 返回结果包\
含 endpoint 属性 ， 存储着匹配成功的端点值 。 

在 dispatch_request() 最后这一行代码中，通过在 view_functions 字典中根据端点作为\
键即可找到对应的视图函数对象 ， 并调用它 ： 

::
    
    return self.view_functions[endpoint](**values)

调用视图函数时传递的参数 ``**values`` 包含 URL 中解析出的变量值 ， 也就是 match() \
函数返回的第二个值 。 这时代码执行流程才终于走到视图函数中 。 

2.3.3 本地上下文
------------------------------------------------------------------------------

Flask 提供了两种上下文 ， 请求上下文和程序上下文 (新版本中) ， 这两种上下文分别包含 request \
、 session 和 current_app 、 g 这四个变量 ， 这些变量是实际对象的本地代理 \
(local proxy) ， 因此被称为本地上下文 (context locals) 。 这些代理对象定义在脚本\
中 ， 在 0.1 版本中只有本地上下文 。 

获取当前请求的信息是从 _request_ctx_stack.top 中获取出来的 ， 也就是说请求会被加入\
请求栈中 ， 栈顶的就是当前请求 。 可以看一下这个请求栈 _request_ctx_stack 的定义 ： 

.. code-block:: python 

    _request_ctx_stack = LocalStack()
    current_app = LocalProxy(lambda: _request_ctx_stack.top.app)
    request = LocalProxy(lambda: _request_ctx_stack.top.request)
    session = LocalProxy(lambda: _request_ctx_stack.top.session)
    g = LocalProxy(lambda: _request_ctx_stack.top.g)

我们在程序中从 flask 包直接导入的 request 和 session 就是定义在这里的全局对象 ， \
这两个对象是对实际的 request 变量和 session 变量的代理 。

通过请求栈 _request_ctx_stack 的定义可以看到 ， 确实是一个请求栈 ， 而且是一个多线\
程隔离的请求中 。 在这边我们简单理解 LocalStack 是一个多线程安全的栈 ， 提供 push \
, pop , top 的方法 。 而栈中元素必然就是单个请求了 ， 元素类型为 _RequestContext ： 

.. code-block:: python 

    [flask.py]

    class _RequestContext(object):

        def __init__(self, app, environ):
            self.app = app
            self.url_adapter = app.url_map.bind_to_environ(environ)
            self.request = app.request_class(environ)
            self.session = app.open_session(self.request)
            self.g = _RequestGlobals()
            self.flashes = None

        def __enter__(self):
            _request_ctx_stack.push(self)

        def __exit__(self, exc_type, exc_value, tb):
            # do not pop the request stack if we are in debug mode and an
            # exception happened.  This will allow the debugger to still
            # access the request object in the interactive shell.
            if tb is None or not self.app.debug:
                _request_ctx_stack.pop()

看到单个请求使用 app 和 environ 进行初始化 ， 其中 app 就是 Flask 实例 ， \
environ 为单次请求具体信息 。 其中就包含 url_adapter 属性 ， 前面已经介绍过 ， 就\
是通过 url_adapter.match() 进行匹配后获取到 endpoint 和 values 的 ， 从而获取到\
请求处理的视图函数的 ， 从而与前面的解释相互印证 。 那么现在还剩下一个问题 ， \
flask 是什么时候将 _RequestContext 加入到 _request_ctx_stack 中的呢 ？ 让我们回\
头看一下 wsgi_app() 方法 ， 使用 with 进行调用 ： 

.. code-block:: python

    class Flask(object):

        def wsgi_app(self, environ, start_response):
            with self.request_context(environ):
                rv = self.preprocess_request()
                if rv is None:
                    rv = self.dispatch_request()
                response = self.make_response(rv)
                response = self.process_response(response)
                return response(environ, start_response)

        def request_context(self, environ):
            return _RequestContext(self, environ)

可以看到调用了 request_context() 方法 ， 此方法创建了一个 _RequestContext 对象 \
， 然后使用 with 的调用方式 ， 会执行 _RequestContext 的 __enter__() 魔术方法 ， \
即会发现 _request_ctx_stack.push(self) ， 将创建的 _RequestContext 加入请求栈 \
_request_ctx_stack 中 ， 然后在执行处理结束的时候 ， 执行 __exit__() 方法 ， 将请\
求从请求栈中移除 。 至此 ， 一切豁然开朗 。 

2.3.3.1 本地线程与 Local 
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

如果每次只能发送一封电子邮件 （单线程） ， 那么在发送大量邮件时会花费很多时间 ， \
这时就需要使用多线程技术 。 处理 HTTP 请求的服务器也是这样 ， 当我们的程序需要面对\
大量用户同时发起的访问请求时 ， 我们显然不能一个个地处理 。 这时就需要使用多线程技\
术 ， Werkzeug 提供的开发服务器默认会开启多线程支持 。 

在处理请求时使用多线程后 ， 我们会面临一个问题 。 当我们直接导入 request 对象并在\
视图函数中使用时 ， 如何确保这时的 request 对象包含的请求信息就是我们需要的那一\
个 ？ 比如 A 用户和 B 用户在同一时间访问 hello 视图 ， 这时服务器分配了两个线程\
来处理这两个请求 ， 如何确保每个线程内的 request 对象都是各自对应 、 互不干扰的 \
？ 

解决办法就是引入本地线程 （Thread Local） 的概念 ， 在保存数据的同时记录下对应的\
线程 ID ， 获取数据时根据所在线程的 ID 即可获取到对应的数据 。 就像是超市里的存包\
柜 ， 每个柜子都有一个号码 ， 每个号码对应一份物品 。 

Flask 中的本地线程使用 Werkzeug 提供的 Local 类实现 ， 如代码清单 : 

.. code-block:: python

    [wekzeug/local.py]

    try:
        from greenlet import getcurrent as get_current_greenlet
    except ImportError: # pragma: no cover
        try:
            from py.magic import greenlet
            get_current_greenlet = greenlet.getcurrent
            del greenlet
        except:
            # catch all, py.* fails with so many different errors.
            get_current_greenlet = int

    class Local(object):
        __slots__ = ('__storage__', '__lock__')

        def __init__(self):
            object.__setattr__(self, '__storage__', {})
            object.__setattr__(self, '__lock__', allocate_lock())

        def __iter__(self):
            return self.__storage__.iteritems()

        def __call__(self, proxy):
            """Create a proxy for a name."""
            return LocalProxy(self, proxy)

        def __release_local__(self):
            self.__storage__.pop(get_ident(), None)

        def __getattr__(self, name):
            self.__lock__.acquire()
            try:
                try:
                    return self.__storage__[get_ident()][name]
                except KeyError:
                    raise AttributeError(name)
            finally:
                self.__lock__.release()

        def __setattr__(self, name, value):
            self.__lock__.acquire()
            try:
                ident = get_ident()
                storage = self.__storage__
                if ident in storage:
                    storage[ident][name] = value
                else:
                    storage[ident] = {name: value}
            finally:
                self.__lock__.release()

        def __delattr__(self, name):
            self.__lock__.acquire()
            try:
                try:
                    del self.__storage__[get_ident()][name]
                except KeyError:
                    raise AttributeError(name)
            finally:
                self.__lock__.release()

Local 中构造函数定义了两个属性 ， 分别是 __storage__ 属性和 __ident_func__ 属\
性 。 __storage__ 是一个嵌套的字典 ， 外层的字典使用线程 ID 作为键来匹配内部的字\
典 ， 内部的字典的值即真实对象 。 它使用 \
self.__storage__[self.__ident_func__()][name] 来获取数据 ， 一个典型的 Local \
实例中的 __storage__ 属性可能会是这样 ： 

.. code-block::

    { 线程ID: { 名称: 实际数据}}

在存储数据时也会存入对应的线程 ID 。 这里的线程 ID 使用 __ident_func__ 属性定义\
的 get_ident() 方法获取 。 这就是为什么全局使用的上下文对象不会在多个线程中产生混\
乱 。 

这里会优先使用 Greenlet 提供的协程 ID ， 如果 Greenlet 不可用再使用 thread 模块\
获取线程 ID 。 类中定义了一些魔法方法来改变默认行为 。 比如 ， 当类实例被调用时会\
创建一个 LocalProxy 对象 ， 我们在后面会详细了解 。 除此之外 ， 类中还定义了用来\
释放线程/协程的 __release_local__() 方法 ， 它会清空当前线程/协程的数据 。 

在 Python 类中 ， 前后双下划线的方法常被称为魔法方法 （Magic Methods） 。 它们是 \
Python内置的特殊方法 ， 我们可以通过重写这些方法来改变类的行为 。 比如 ， 我们熟悉\
的 __init__() 方法 （构造函数） 会在类被实例化时调用 ， 类中的 __repr__() 方法会\
在类实例被打印时调用 。 Local 类中定义的 __getattr__() 、 __setattr__() 、 \
__delattr__() 方法分别会在类属性被访问 、 设置 、 删除时调用 ； __iter__() 会在\
类实例被迭代时调用 ； __call__() 会在类实例被调用时调用 。 完整的列表可以在 \
Python 文档 （https://docs.python.org/3/reference/datamodel.html） 看到 。 

2.3.3.2 堆栈与 LocalStack 
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

堆栈或栈是一种常见的数据结构 ， 它的主要特点就是后进先出 （LIFO，Last In First \
Out） ， 指针在栈顶 （top） 位置 ， 如图 16-9 所示 。 堆栈涉及的主要操作有 push \
（推入） 、 pop （取出） 和 peek （获取栈顶条目） 。 其他附加的操作还有获取条目数\
量 ， 判断堆栈是否为空等 。 使用 Python 列表 （list） 实现的一个典型的堆栈结构如\
代码清单所示 。 

.. image:: img/2-3.png

.. code-block:: python 

    [stack.py]

    class Stack:

        def __init__(self):
            self.items = []

        def push(self, item): # 推入条目
            self.items.append(item)

        def pop(self): # 移除并返回栈顶条目
            if self.is_empty:
                return None
            return self.items.pop()

        @property
        def is_empty(self): # 判断是否为空
            return self.items == []

        @property
        def top(self): # 获取栈顶条目
            if self.is_empty:
                return None
            return self.items[-1]

未完待续 ...

上一篇文章 ： `上一篇`_

下一篇文章 ： `下一篇`_ 

.. _`上一篇`: flask-0.1-01.rst
.. _`下一篇`: flask-0.1-03.rst
