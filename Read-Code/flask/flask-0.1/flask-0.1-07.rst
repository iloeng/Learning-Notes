##############################################################################
Python Web 模块之 Flask v0.1
##############################################################################

..
    ============
    Part title..
    ============

    ***************
    Chapter title..
    ***************

    Section title..
    ===============

    Subsection title..
    ------------------

    Subsubsection title..
    ^^^^^^^^^^^^^^^^^^^^^

    Paragraph title..
    """""""""""""""""

    Paragraph title..
    *****************

.. contents::

******************************************************************************
第 3 部分  源码阅读之测试用例
******************************************************************************

3.1 BasicFunctionality
==============================================================================

3.1.2 Session
------------------------------------------------------------------------------

.. code-block:: python

    def test_session(self):
        app = flask.Flask(__name__)
        app.secret_key = 'testkey'
        @app.route('/set', methods=['POST'])
        def set():
            flask.session['value'] = flask.request.form['value']
            return 'value set'
        @app.route('/get')
        def get():
            return flask.session['value']

        c = app.test_client()
        assert c.post('/set', data={'value': '42'}).data == 'value set'
        assert c.get('/get').data == '42'

这个测试用例也很简单 ， set 函数用于设置当前请求中 session 中 'value' 的值 ， 设置\
为表单中 'value' 字段的值 ， 最终返回 'value set' ； get 函数就返回设置的 session \
中的 value 字段 。 

然后模拟一个客户端发送请求 ， 通过判断相应的值来确认功能是否正常 。 

3.1.3 Request Processing
------------------------------------------------------------------------------

.. code-block:: python 

    def test_request_processing(self):
        app = flask.Flask(__name__)
        evts = []
        @app.before_request
        def before_request():
            evts.append('before')
        @app.after_request
        def after_request(response):
            response.data += '|after'
            evts.append('after')
            return response
        @app.route('/')
        def index():
            assert 'before' in evts
            assert 'after' not in evts
            return 'request'
        assert 'after' not in evts
        rv = app.test_client().get('/').data
        assert 'after' in evts
        assert rv == 'request|after'

这个测试用例主要是测试响应前与响应后的功能 ， before_request 视图函数注册了 \
before_request 路由 ， 在执行视图函数之前会先行执行它 ； 同理 after_request 视图函\
数注册了 after_request 路由 ， 在视图函数执行完毕后在执行它 。

因此 before_request 函数中先向 evts 列表中添加了 'before' ， 然后在执行 index 视\
图函数 ， 这里视图函数首先判断 'before'  是否在 evts 中 ， 如果在就继续 ， 否则就失\
败 ； 然后判断 'after' 是否在 evts 中 ， 如果不在就继续 ， 否则失败 ； 最终返回 \
'request' ； 然后执行 after_request 函数 ， 它会在之前的相应数据中添加 '\|after' \
， 同时将 'after' 添加到 evts 中 。 整个步骤就是这样的 ， 在进行判断操作 。 

3.1.4 Error Handling
------------------------------------------------------------------------------

.. code-block:: python 

    def test_error_handling(self):
        app = flask.Flask(__name__)
        @app.errorhandler(404)
        def not_found(e):
            return 'not found', 404
        @app.errorhandler(500)
        def internal_server_error(e):
            return 'internal server error', 500
        @app.route('/')
        def index():
            flask.abort(404)
        @app.route('/error')
        def error():
            1/0
        c = app.test_client()
        rv = c.get('/')
        assert rv.status_code == 404
        assert rv.data == 'not found'
        rv = c.get('/error')
        assert rv.status_code == 500
        assert 'internal server error' in rv.data

这个测试用例是为了测试错误处理功能是否正常 。 

not_found 函数通过 errorhandler 注册了 404 代码的处理方法 ， 返回 \
``'not found', 404`` ； internal_server_error 注册了一个 500 代码的处理方法 ， \
返回 ``'internal server error', 500`` ； 访问 index 的时候 ， 直接以 404 异常中\
止 ； error 是以 Python 错误语句来导致 Python 内部错误 ， 可以被 \
internal_server_error 捕获 。 

因此这里也很好理解 ， 当请求 '/' 时会被 404 异常中止服务 ， 那么状态码应该为 404 \
， 执行结果为 'not found' 。 同理后面的步骤也是这样 。 

3.1.5 Response Creation
------------------------------------------------------------------------------

.. code-block:: python 

    def test_response_creation(self):
        app = flask.Flask(__name__)
        @app.route('/unicode')
        def from_unicode():
            return u'Hällo Wörld'
        @app.route('/string')
        def from_string():
            return u'Hällo Wörld'.encode('utf-8')
        @app.route('/args')
        def from_tuple():
            return 'Meh', 400, {'X-Foo': 'Testing'}, 'text/plain'
        c = app.test_client()
        assert c.get('/unicode').data == u'Hällo Wörld'.encode('utf-8')
        assert c.get('/string').data == u'Hällo Wörld'.encode('utf-8')
        rv = c.get('/args')
        assert rv.data == 'Meh'
        assert rv.headers['X-Foo'] == 'Testing'
        assert rv.status_code == 400
        assert rv.mimetype == 'text/plain'

这个 case 是测试请求响应的 ， 前面的判断都很好理解 ， 我有些疑惑的是 from_tuple 视\
图函数响应的时候会是 data ， headers ， status_code 和 mimetype 在返回值中 ， 应\
该是响应的时候经过了某些步骤的处理吧 。 

3.1.6 URL Generation
------------------------------------------------------------------------------

.. code-block:: python 

    def test_url_generation(self):
        app = flask.Flask(__name__)
        @app.route('/hello/<name>', methods=['POST'])
        def hello(): # 这里添加参数 name => def hello(name) 较好
            pass  # 这里改成 return "name" 较好
        with app.test_request_context():
            assert flask.url_for('hello', name='test x') == '/hello/test%20x'

这个 case 也比较简单 ， 注册一个路由之后 ， 在请求上下文中判断响应的链接是否正确 ， \
这里的 test_request_context 其实就是创建请求上下文 ， 其代码如下 ： 

.. code-block:: python 

    def test_request_context(self, *args, **kwargs):
        return self.request_context(create_environ(*args, **kwargs))

这里的 request_context 之前已经解析过 ， 就不再解析 ； url_for 函数是用来生成 URL \
链接的 ， 根据给定的参数生成链接 ， 其代码如下 ： 

.. code-block:: python 

    def url_for(endpoint, **values):
        """Generates a URL to the given endpoint with the method provided.

        :param endpoint: the endpoint of the URL (name of the function)
        :param values: the variable arguments of the URL rule
        """
        return _request_ctx_stack.top.url_adapter.build(endpoint, values)

由于 build 不是 Flask 的代码 ， 这里就不在解析 。

最终这个 case 通过判断生成链接是否符合预期来判断功能是否正常 。 

3.1.7 Static Files
------------------------------------------------------------------------------

.. code-block:: python 

    def test_static_files(self):
        app = flask.Flask(__name__)
        rv = app.test_client().get('/static/index.html')
        assert rv.status_code == 200
        assert rv.data.strip() == '<h1>Hello World!</h1>'
        with app.test_request_context():
            assert flask.url_for('static', filename='index.html') \
                == '/static/index.html'

这里的 index.html 文件内容就是 ``<h1>Hello World!</h1>`` ， 在这里并没有设置 \
static 文件目录 ， 这是因为 Flask 0.1 中已经设置了 static 目录为与 Flask 实例同级 \
， 因此没有设置 ， 同时是直接请求静态文件 ， 所以不需要视图函数 。

因此请求一个已知路径的静态文件是可以正常请求到的 ， 因此这里的 status_code 为正常的 \
200 ， 返回值也用 strip 函数预处理了一下 ， 最后又测试了一下 url_for 生成链接的功\
能 ， 这里就不在解析 。 

3.2 Context
==============================================================================

上文中已经解析完毕基础功能相关的测试用例 ， 这一节解析上下文相关的用例 。 

3.2.1 Static Files
------------------------------------------------------------------------------

.. code-block:: python 

    def test_context_binding(self):
        app = flask.Flask(__name__)
        @app.route('/')
        def index():
            return 'Hello %s!' % flask.request.args['name']
        @app.route('/meh')
        def meh():
            return flask.request.url

        with app.test_request_context('/?name=World'):
            assert index() == 'Hello World!'
        with app.test_request_context('/meh'):
            assert meh() == 'http://localhost/meh'

这个测试用例两个视图函数分别是 ： index 最终返回请求参数与 "Hello " 相连的字符串 \
； meh 最终返回当前请求的链接 。 

首先模拟一个请求上下文 ， 请求链接是 '/' ， 参数是 name=World ， 这里需要注意一下 \
， 在实际的链接中 ， "?" 之后的就是链接的请求参数 ， 所以其返回值为 Hello World ， \
类似于基本功能的测试用例里面创建一个 Client ， 然后请求 GET '/?name=World' ， 这里\
直接在请求上下文里面操作 ， 省去了请求的步骤 ； 下面的步骤同理 。 不过需要注意一下 \
， 如果自定义了 server host ， 链接中就不是 localhost 了 。

3.3 Templating
==============================================================================

上文中已经解析完毕基础功能及上下文相关的测试用例 ， 这一节解析模板相关的用例 。 

3.3.1 Context Processing
------------------------------------------------------------------------------

.. code-block:: python 

    def test_context_processing(self):
        app = flask.Flask(__name__)
        @app.context_processor
        def context_processor():
            return {'injected_value': 42}
        @app.route('/')
        def index():
            return flask.render_template('context_template.html', value=23)
        rv = app.test_client().get('/')
        assert rv.data == '<p>23|42'

测试用例开始之前 ， 使用 context_processor 注册了一个上下文处理器 ， 这个上下文处\
理器返回了一个字典 {'injected_value': 42} ； 同时主页使用 render_template 函数动\
态渲染了一个静态模板 ， 最终通过 get 请求主页后的值进行比对 ， 来判断测试功能是否正\
常 。

首先先看一下 context_processor 方法 ：

.. code-block:: python 

    [flask.py]

    def context_processor(self, f):
        """Registers a template context processor function."""
        self.template_context_processors.append(f)
        return f

就是把参数对象添加到模板处理器列表 template_context_processors 中 ， Flask 初始化\
的时候已经初始化为 \
``self.template_context_processors = [_default_template_ctx_processor]`` 这个 \
_default_template_ctx_processor 实际上就是一个 dict 对象 ：

.. code-block:: python 

    def _default_template_ctx_processor():
        """Default template context processor.  Injects `request`,
        `session` and `g`.
        """
        reqctx = _request_ctx_stack.top
        return dict(
            request=reqctx.request,   # 当前请求
            session=reqctx.session,   # 当前请求的 session
            g=reqctx.g
        )

接下来解析 render_template 函数 ：

.. code-block:: python 

    def render_template(template_name, **context):
        """Renders a template from the template folder with the given
        context.

        :param template_name: the name of the template to be rendered
        :param context: the variables that should be available in the
                        context of the template.
        """
        current_app.update_template_context(context)
        return current_app.jinja_env.get_template(template_name).render(context)

传入两个参数 ， 第一个是模板文件名称 ， 第二个就是参数字典 。 最终返回 jinja 渲染的\
文本 。 

当然在渲染之前 ， 会先执行模板上下文处理器 template_context_processors ：

.. code-block:: python 

    [flask.py]

    def update_template_context(self, context):
        """Update the template context with some commonly used variables.
        This injects request, session and g into the template context.

        :param context: the context as a dictionary that is updated in place
                        to add extra variables.
        """
        reqctx = _request_ctx_stack.top
        for func in self.template_context_processors:
            context.update(func())

将处理器全部执行一遍之后才会执行渲染步骤 ， 这个过程就是为了更新上下文的变量 。 

那这个测试用例就很明了了 ， 先执行 index 函数 ， 但是 index 函数中有渲染模板的功能 \
， 在模板渲染函数 render_template 中 ， 会首先执行模板上下文处理器 ， 因此会先行执\
行 context_processor 函数 ， 再渲染模板 ， 这个模板语句很简单 ： 

.. code-block:: html

    <p>{{ value }}|{{ injected_value }} 

经过渲染后 ， 分别将 value 和 injected_value 替换到模板文件中 ， 最终结果为 ： \
``<p>23|42`` ， 因此正常情况下应该是通过的 。

