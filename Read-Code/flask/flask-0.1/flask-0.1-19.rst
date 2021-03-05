##############################################################################
Python Web 模块之 Flask v0.1
##############################################################################

.. contents::

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