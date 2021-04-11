##############################################################################
Python Web 模块之 Flask v0.1
##############################################################################

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


