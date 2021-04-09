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

