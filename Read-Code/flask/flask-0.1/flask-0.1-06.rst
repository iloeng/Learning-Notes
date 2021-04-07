##############################################################################
Python Web 模块之 Flask v0.1
##############################################################################

.. contents::

******************************************************************************
第 3 部分  源码阅读之 App 代码阅读
******************************************************************************

3.6 Flask __call__
==============================================================================

uml: Flask-__call__.puml

.. code-block:: python 

    def __call__(self, environ, start_response):
        """Shortcut for :attr:`wsgi_app`"""
        return self.wsgi_app(environ, start_response)

执行 __call__ 函数时 ， 直接返回了 wsgi_app 函数的执行结果 。 

3.7 Flask wsgi_app
==============================================================================

uml: Flask-wsgi_app.puml

.. code-block:: python 

    def wsgi_app(self, environ, start_response):
        """
        :param environ: a WSGI environment
        :param start_response: a callable accepting a status code,
                               a list of headers and an optional
                               exception context to start the response
        """
        with self.request_context(environ):
            rv = self.preprocess_request()
            if rv is None:
                rv = self.dispatch_request()
            response = self.make_response(rv)
            response = self.process_response(response)
            return response(environ, start_response)

首先打开一个请求上下文 ， 在这个上下文过程中 ， 先执行 preprocess_request 函数进行\
预处理请求的执行 ， 如果没有预处理请求 ， 则执行 dispatch_request 函数 ， 再然后执\
行 make_response 对预处理请求或分发的请求生成响应对象 ， 然后处理这个响应对象 ， 其\
结果作为返回值返回出去 。 

3.8 Flask request_context
==============================================================================

uml: Flask-request_context.puml

.. code-block:: python 

    def request_context(self, environ):
        return _RequestContext(self, environ)

接上文 ， 进入 request_context 后直接返回 _RequestContext 类实例 ， 换句话说 \
request_context 就是 _RequestContext 类实例。 

3.9 _RequestContext
==============================================================================

uml: Flask-_RequestContext.puml

.. code-block:: python 

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
            if tb is None or not self.app.debug:
                _request_ctx_stack.pop()

在上文中 ， 执行 with request_context 的时候 ， 会执行 _RequestContext 类的 \
__enter__ 函数 ， 当然是在执行 __init__ 函数之后 ， 举个例子可以看一下 with 的执\
行顺序 ： 

.. code-block:: python 

    class testwith:
        def __init__(self):
            print('__init__()')

        def __enter__(self):
            print('__enter__()')
            return '__enter__'
        
        def __exit__(self, type, value, trace):
            print('__exit__()')
        
    with testwith() as tt:
        print(tt)

    Result:
    >>>__init__()
    >>>__enter__()
    >>>__enter__
    >>>__exit__()

这个示例代码充分说明了执行过程是先执行初始化函数 ， 然后执行 __enter__ 函数 ， 上下\
文结束时执行 __exit__ 函数 。 

因此 _RequestContext 类中也是这样的顺序 ， 先初始化 6 个变量 ：

- self.app = app
- self.url_adapter = app.url_map.bind_to_environ(environ)
- self.request = app.request_class(environ)
- self.session = app.open_session(self.request)
- self.g = _RequestGlobals()
- self.flashes = None

初始化中的 app 参数就是 Flask 类实例 ， 因为 \
``return _RequestContext(self, environ)`` self 代表的就是 Flask 类实例 ； \
url_adapter 为当前 Flask app 的 url_map 绑定到 wsgi 环境中 ； request 为当前 \
Flask app 的 request_class ； session 为当前 Flask app 的 open_session ； g 为\
_RequestGlobals 类实例 ； flashes 为空 (None) 。

然后执行 _request_ctx_stack.push 函数 ， 将当前请求上下文推入到请求上下文堆栈中 \
， 上下文结束后执行 _request_ctx_stack.pop ， 弹出当前请求上下文 。 

3.10 Flask request_class
==============================================================================

uml: Flask-request_class.puml

.. code-block:: python 
    
    class Flask:

        request_class = Request

在 _RequestContext 中 ， bind_to_environ 函数属于 werkzeug 模块 ， 先放过 。 而 \
self.request 的值 Flask.request_class 中的 request_class 就是 Request 类实例 。 

3.11 Request
==============================================================================

uml: Flask-Request.puml

.. code-block:: python 

    class Request(RequestBase):
        """The request object used by default in flask.  Remembers the
        matched endpoint and view arguments.
        """

        def __init__(self, environ):
            RequestBase.__init__(self, environ)
            self.endpoint = None
            self.view_args = None

接上文 ， Request 类继承了 werkzeug.wrappers.Request 类 ， 然后记录了匹配的 \
endpoint 和 view_args 。 

3.12 open_session
==============================================================================

uml: Flask-open_session.puml

.. code-block:: python 

    def open_session(self, request):
        key = self.secret_key
        if key is not None:
            return SecureCookie.load_cookie(request, self.session_cookie_name,
                                            secret_key=key)

在 _RequestContext 类中继续 ， self.session 的值 open_session 函数的 request 参\
数就是当前请求对象 ， 因为 app.open_session(self.request) 。 self.request 是一\
个 Request 类实例 ， 当 self.secret_key 不为空时 ， 返回 SecureCookie 类 。

3.13 _RequestGlobals
==============================================================================

接着上文 ， _RequestContext 中 g 变量是 _RequestGlobals 类实例 ， 代码如下 ： 

.. code-block:: python 

    class _RequestGlobals(object):
        pass

因此 g 变量为空 。 

OK ， 到这里 _RequestContext 类解析完毕 ， 也就是说 request_context 解析完毕 ， \
接下来返回到 wsgi_app 函数中 ， 进入请求上下文当中 ， 解析 preprocess_request 方法

3.14 Flask preprocess_request
==============================================================================

preprocess_request 的源代码如下所示 ， ``self.before_request_funcs`` 是一个列表 \
， 默认情况下是空值 ， 其值为可调用对象 ， 通过 before_request 函数进行操作 。 

.. code-block:: python 

    def preprocess_request(self):
        for func in self.before_request_funcs:
            rv = func()
            if rv is not None:
                return rv

由于一般情况下是空值 ， 所以该函数没有返回值 ， 但是当 before_request_funcs 有值的\
时候 ， 会返回其值的返回值 ， 换句话说 ， before_request_funcs 中是一个个函数 ， \
返回的是函数的执行结果 。 

3.15 Flask before_request
==============================================================================

.. code-block:: python 

    def before_request(self, f):
        """Registers a function to run before each request."""
        self.before_request_funcs.append(f)
        return f

直接看一下这个函数 ， 它用来注册在每个请求执行之前的函数 ， 也就是说在执行一个视图函\
数之前 ， 先执行 before_request_funcs 列表中的函数 ， 调用这个函数之后 ， 会将参数\
对象追加到 before_request_funcs 列表中 ， 最后返回这个参数对象 。 

3.16 Flask dispatch_request
==============================================================================

继续 wsgi_app 中的解析 ， 由于 preprocess_request 为空 ， 判断条件为 False ， 因\
此执行 dispatch_request 函数 ， 该函数代码如下 ：

.. code-block:: python 

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

其实这个函数在前文中有过解析 ， 这里在详细解析一下 。 首先执行 try 内部的步骤 ， 执\
行 match_request 函数获得 endpoint 和 values ， 这里的 endpoint 其实就是视图函数\
名称 ， values 就是视图函数的参数 ， 然后从 view_functions (视图函数关联字典) 中获\
取到视图函数对象 ， 再将参数传递过去 ， 最终返回视图函数的执行结果 。 

如果出现 HTTPException ， 则执行错误事件处理函数 ， error_handlers 是一个字典 ， \
通过 errorhandler 函数注册错误事件处理函数 ， 从 error_handlers 字典中获取到错误事\
件处理对象之后 ， 执行这个对象并返回出去结果 。

如果是其他的 Exception ， 直接按照错误代码 500 进行处理 。 

3.17 Flask match_request
==============================================================================

.. code-block:: python 

    def match_request(self):
        """Matches the current request against the URL map and also
        stores the endpoint and view arguments on the request object
        is successful, otherwise the exception is stored.
        """
        rv = _request_ctx_stack.top.url_adapter.match()
        request.endpoint, request.view_args = rv
        return rv

接着 dispatch_request 函数中的步骤 ， match_request 函数的功能就如函数注释 ， 将\
当前请求与 URL 映射进行匹配 ， 匹配成功就存储 endpoint 和视图函数的参数 ， 否则就存\
储异常 。 最终返回匹配结果 。 

3.18 Flask errorhandler
==============================================================================

.. code-block:: python 

    def errorhandler(self, code):

        def decorator(f):
            self.error_handlers[code] = f
            return f
        return decorator

接着 dispatch_request 函数中的步骤 ， 如果出现异常 ， 就会从异常处理列表中查找异常\
处理方法 ， error_handlers 是一个字典 ， 通过 errorhandler 函数注册错误事件处理函\
数 ， 类似于 route 注册路由 ， errorhandler 会注册某些错误代码的处理方法 ， 假如错\
误代码是 404 ：

.. code-block:: python 

    @app.errorhandler(404)
    def page_not_found():
        return 'This page does not exist', 404

其注册后的结果 errorhandler = {'404': page_not_found} ， 之后会通过异常代码查找异\
常处理方法 ， 如果出现了 404 异常代码 ， 然后就查到 page_not_found 方法 ， 然后就执\
行它 。

到此 dispatch_request 函数解析完毕 。 


