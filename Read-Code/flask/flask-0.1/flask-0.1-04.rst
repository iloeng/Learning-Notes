##############################################################################
Python Web 模块之 Flask v0.1
##############################################################################

.. contents::

******************************************************************************
第 2 部分  源码阅读准备 
******************************************************************************

2.3 Flask 工作流程
==============================================================================

2.3.4 请求与响应对象
------------------------------------------------------------------------------

2.3.4.1 请求对象
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

承接上文 ， 溯源代码如下 : 

.. code-block:: python 

    [flask.py]

    class _RequestContext(object):

        def __init__(self, app, environ):
            self.request = app.request_class(environ)
            ...
    
    class Flask(object):

        request_class = Request
        ...

    class Request(RequestBase):
        """The request object used by default in flask.  Remembers the
        matched endpoint and view arguments.

        It is what ends up as :class:`~flask.request`.  If you want to replace
        the request object used you can subclass this and set
        :attr:`~flask.Flask.request_class` to your subclass.
        """

        def __init__(self, environ):
            RequestBase.__init__(self, environ)
            self.endpoint = None
            self.view_args = None
    
    from werkzeug import Request as RequestBase, Response as ResponseBase, \
         LocalStack, LocalProxy, create_environ, cached_property, \
         SharedDataMiddleware

    [flask/wrappers.py：Request]

    class Request(BaseRequest, AcceptMixin, ETagRequestMixin,
              UserAgentMixin, AuthorizationMixin,
              CommonRequestDescriptorsMixin):
        """Full featured request object implementing the following mixins:

        - :class:`AcceptMixin` for accept header parsing
        - :class:`ETagRequestMixin` for etag and cache control handling
        - :class:`UserAgentMixin` for user agent introspection
        - :class:`AuthorizationMixin` for http auth handling
        - :class:`CommonRequestDescriptorsMixin` for common headers
        """

Request 类继承 Werkzeug 提供的 Request 类 。 请求对象 request 的大部分属性都直接\
继承 Werkzeug 中 Request 类的属性 ， 比如 method 、 args 等 。 Flask 中的这个 \
Request 类主要添加了一些 Flask 特有的属性 ， 比如为了方便获取当前端点的 endpoint \
属性等 。 

Flask 允许自定义请求类 ， 通常情况下 ， 我们会子类化这个 Request 类 ， 并添加一些自\
定义的设置 ， 然后把这个自定义请求类赋值给程序实例的 request_class 属性 。 

2.3.4.2 响应对象
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

一般情况下 ， 在编写程序时我们并不需要直接与响应打交道 。 在 Flask 中的请求 - 响应\
循环中 ， 我们知道响应是在 wsgi_app() 方法中生成的 ， 它调用了 \
flask.Flask.make_response() 方法生成响应对象 ， 传入的 rv 参数是 \
dispatch_request() 的返回值 ， 也就是视图函数的返回值 。 

在前面介绍过 ， 视图函数可以返回多种类型的返回值 。 完整的合法返回值如表所示 。 

.. image:: img/2-5.png

这个 Flask.make_response() 方法主要的工作就是判断返回值是表中的哪一种类型 ， 最后\
根据类型做相应处理 ， 最后生成一个响应对象并返回它 。 响应对象为 Response 类的实例 \
， Response 类的定义如代码清单所示 。 

.. code-block:: python 

    [flask.py]

    class Response(ResponseBase):
        """The response object that is used by default in flask.  Works like the
        response object from Werkzeug but is set to have a HTML mimetype by
        default.  Quite often you don't have to create this object yourself because
        :meth:`~flask.Flask.make_response` will take care of that for you.

        If you want to replace the response object used you can subclass this and
        set :attr:`~flask.Flask.request_class` to your subclass.
        """
        default_mimetype = 'text/html'

和 Request 类相似 ， 这个响应对象继承 Werkzeug 中的 Response 类 。 这个类比 \
Request 类更简单 ， 只是设置了默认的 MIME 类型 。 

Flask 也允许你自定义响应类 ， 自定义的响应类通常会继承自内置的 Response 类 ， 然后\
赋值给 flask.Flask.response_class 属性 。 

2.3.5 Session 
------------------------------------------------------------------------------

在开始介绍 session 的实现之前 ， 有必要再重申一下措辞问题 。 我会使用下面的方式来表\
述三个与 session 相关的内容 ： Flask 提供了 "session 变量/对象" 来操作 "用户会话 \
(Session)" ， 它把用户会话保存在 "一块名/键为 session 的 cookie" 中 。 

在 Flask 中使用 session 非常简单 ， 只需要设置好密钥 ， 就可以在视图函数中操作 \
session 对象 ： 

.. code-block:: python  

    from flask import Flask, session
    app = Flask(__name__)
    app.secret_key = 'secret string'

    @app.route('/')
    def hello():
        session['answer'] = 42
        return '<h1>Hello, Flask!</h1>'

当第一次介绍 session 时我们曾说它 "可以记住请求间的值" ， 很多人会对这句话感到困惑 \
。 就这个例子来说 ， 当用户访问 hello 视图时 ， 会把数字 42 存储到 session 对象里 \
， 以 answer 作为键 。 假如我再定义一个 bingo 视图 ， 当用户访问 bingo 视图时 ， \
我们可以在 bingo 视图里再次从 session 通过 answer 键获取这个数字 。 这一存一取背后\
的逻辑是这样的 ：

向 session 中存储值时 ， 会生成加密的 cookie 加入响应 。 这时用户的浏览器接收到响应\
会将 cookie 存储起来 。 当用户再次发起请求时 ， 浏览器会自动在请求报文中加入这个 \
cookie 值 。 Flask 接收到请求会把 session cookie 的值解析到 session 对象里 。 这\
时我们就可以再次从 session 中读取内容 。 

在向session中存数字的这行代码设置断点：

:: 

    session['answer'] = 42

2.3.5.1 操作 session
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

在前面学习过 ， session 变量在 flask 中的定义 ：

.. code-block:: python 

    session = LocalProxy(lambda: _request_ctx_stack.top.session)

从上面的代码中可以看到 Flask 从请求上下文堆栈的栈顶 (_request_ctx_stack.top) 获取\
请求上下文 ， 可以看出 session 是请求上下文对象 (即 _RequestContext) 的一个属性 \
， 这也就意味着 ， session 变量是在生成请求上下文的时候创建的 ， 后面我们会详细了解\
它的生成过程 。 

继续步进代码后 ， 会执行 LocalProxy 类的 __setitem__() 方法 ， 它会把设置操作转发\
给真实的 session 对象 ： 

.. code-block:: python 

    class LocalProxy(object):
        ...
        def __setitem__(self, key, value):
            self._get_current_object()[key] = value

.. image:: img/2-6.png

这时在调试工具栏右侧的变量列表中可以看到已经被代理的 session 对象实际上是 \
werkzeug.contrib.securecookie 模块中的 SecureCookie 类的实例 。 

查看步骤 ： 

1. 鼠标选择 'hello' , 在 variable 中添加 watch

.. image:: img/2-7.png

2. 添加 'session'

.. image:: img/2-8.png

在 Werkzeug 中进行一系列查询工作后 ， 最终执行了 SecureCookie 类中的 \
load_cookie() 方法 。

.. code-block:: python 

    [werkzeug/contrib/securecookie.py]

    class SecureCookie(ModificationTrackingDict):

        @classmethod
        def load_cookie(cls, request, key='session', secret_key=None):
            """Loads a :class:`SecureCookie` from a cookie in request.  If the
            cookie is not set, a new :class:`SecureCookie` instanced is
            returned.

            :param request: a request object that has a `cookies` attribute
                            which is a dict of all cookie values.
            :param key: the name of the cookie.
            :param secret_key: the secret key used to unquote the cookie.
                            Always provide the value even though it has
                            no default!
            """
            data = request.cookies.get(key)
            if not data:
                return cls(secret_key=secret_key)
            return cls.unserialize(data, secret_key)

        @classmethod
        def unserialize(cls, string, secret_key):
            """Load the secure cookie from a serialized string.

            :param string: the cookie value to unserialize.
            :param secret_key: the secret key used to serialize the cookie.
            :return: a new :class:`SecureCookie`.
            """
            if isinstance(string, unicode):
                string = string.encode('utf-8', 'ignore')
            try:
                base64_hash, data = string.split('?', 1)
            except (ValueError, IndexError):
                items = ()
            else:
                items = {}
                mac = hmac(secret_key, None, cls.hash_method)
                for item in data.split('&'):
                    mac.update('|' + item)
                    if not '=' in item:
                        items = None
                        break
                    key, value = item.split('=', 1)
                    # try to make the key a string
                    key = url_unquote_plus(key)
                    try:
                        key = str(key)
                    except UnicodeError:
                        pass
                    items[key] = value

                # no parsing error and the mac looks okay, we can now
                # sercurely unpickle our cookie.
                try:
                    client_hash = base64_hash.decode('base64')
                except Exception:
                    items = client_hash = None
                if items is not None and client_hash == mac.digest():
                    try:
                        for key, value in items.iteritems():
                            items[key] = cls.unquote(value)
                    except UnquoteError:
                        items = ()
                    else:
                        if '_expires' in items:
                            if time() > items['_expires']:
                                items = ()
                            else:
                                del items['_expires']
                else:
                    items = ()
            return cls(items, secret_key, False)

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