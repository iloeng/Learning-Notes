##############################################################################
Python Web 模块之 Flask v0.1
##############################################################################

.. contents::

******************************************************************************
第 3 部分  源码阅读之 App 代码阅读
******************************************************************************

3.1 App 代码
==============================================================================

3.1.4 Flask __call__
------------------------------------------------------------------------------

uml: Flask-__call__.puml

.. code-block:: python 

    def __call__(self, environ, start_response):
        """Shortcut for :attr:`wsgi_app`"""
        return self.wsgi_app(environ, start_response)

执行 __call__ 函数时 ， 直接返回了 wsgi_app 函数的执行结果 。 

3.1.4 Flask wsgi_app
------------------------------------------------------------------------------

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

3.1.4 Flask request_context
------------------------------------------------------------------------------

uml: Flask-request_context.puml

.. code-block:: python 

    def request_context(self, environ):
        return _RequestContext(self, environ)

直接返回 _RequestContext 类实例 ， 换句话说 request_context 就是 \
_RequestContext 类实例。 

3.1.4 _RequestContext
------------------------------------------------------------------------------

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

在上文中 ， 执行 with 的时候 ， 会执行 __enter__ 函数 ， 当然是在执行 __init__ 函\
数之后 ， 举个例子 ： 

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

因此 _RequestContext 类中也是这样的顺序 ， 初始化 6 个变量 ， 然后执行 \
_request_ctx_stack.push 函数 ， 将当前请求上下文推入到请求上下文堆栈中 ， 上下文结\
束后执行 _request_ctx_stack.pop ， 弹出当前请求上下文 。 

