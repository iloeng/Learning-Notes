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

