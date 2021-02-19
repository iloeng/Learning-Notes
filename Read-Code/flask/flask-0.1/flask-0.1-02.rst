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
调用的过程 。 在注册路由时 ， 由 Rule 类表示的 rule 对象由 route() 装饰器传入的参\
数创建 。 而这里则直接从请求上下文对象 （_request_ctx_stack.top.request） 的 \
url_rule 属性获取 。 可以得知 ， URL 的匹配工作在请求上下文对象中实现 。 请求上下文\
对象 _RequestContext 在脚本中定义 ， 如代码清单所示 : (这段来自 Flask Web 开发实\
战 ： 入门 、 进阶与原理解析 （李辉著） ， 我会尽快理解这部分内容) 。

.. code-block:: python 

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

可以看到 ， 请求上下文对象的构造函数中调用了 match_request() 方法 ， 这会在创建请\
求上下文对象时调用 。 顾名思义 ， 这个方法用来匹配请求 （request matching） ， 如\
代码清单所示 : (这段来自 Flask Web 开发实战 ： 入门 、 进阶与原理解析 （李辉著） \
， 我会尽快理解这部分内容)

.. code-block:: python

    class Flask(object):
        def match_request(self):
            """Matches the current request against the URL map and also
            stores the endpoint and view arguments on the request object
            is successful, otherwise the exception is stored.
            """
            rv = _request_ctx_stack.top.url_adapter.match()
            request.endpoint, request.view_args = rv
            return rv

可以看到 url_rule 属性就在这个方法中创建 。 这个方法调用了 \
self.url_adapter.match(return_rule=True) 来获取 url_rule 和 view_args 。 这里\
的 url_adapter 属性在构造函数中定义 ， 其值为 \
app.create_url_adapter(self.request) 。 create_url_adapter() 方法的定义
如代码清单所示 : (这段来自 Flask Web 开发实战 ： 入门 、 进阶与原理解析 （李辉著） \
， 我会尽快理解这部分内容)

.. code-block:: python 

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

我们知道url_map属性是一个Map对象，可以看出它最后调用了
bind（）或bind_to_environ（）方法，最终会返回一个MapAdapter类实
例。
match_request（）方法通过调用MapAdapter.match（）方法来匹配
请求URL，设置return_rule=True可以在匹配成功后返回表示URL规则的
Rule类实例。这个Rule实例包含endpoint属性，存储着匹配成功的端点
值。
在dispatch_request（）最后这一行代码中，通过在view_functions字
典中根据端点作为键即可找到对应的视图函数对象，并调用它：

******************************************************************************
第 3 部分  源码阅读之测试用例
******************************************************************************

3.1 BasicFunctionality
==============================================================================

首先阅读基础功能方面的测试用例 ， 按照源码中的 Test 依次阅读 。 

3.1.1 Request Dispatching
------------------------------------------------------------------------------

第一个是请求转发功能 ， 详情看测试用例代码 。 

.. code-block:: python

    class BasicFunctionality(unittest.TestCase):

        def test_request_dispatching(self):
            app = flask.Flask(__name__)

            @app.route('/')
            def index():
                return flask.request.method
            
            @app.route('/more', methods=['GET', 'POST'])
            def more():
                return flask.request.method

            c = app.test_client()
            assert c.get('/').data == 'GET'
            rv = c.post('/')
            assert rv.status_code == 405
            assert sorted(rv.allow) == ['GET', 'HEAD']
            rv = c.head('/')
            assert rv.status_code == 200
            assert not rv.data # head truncates
            assert c.post('/more').data == 'POST'
            assert c.get('/more').data == 'GET'
            rv = c.delete('/more')
            assert rv.status_code == 405
            assert sorted(rv.allow) == ['GET', 'HEAD', 'POST']

首先初始化一个 Flask 对象 -> app ； 