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

