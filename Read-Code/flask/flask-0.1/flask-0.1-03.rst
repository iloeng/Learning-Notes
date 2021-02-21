##############################################################################
Python Web 模块之 Flask v0.1
##############################################################################

.. contents::

******************************************************************************
第 2 部分  源码阅读准备 
******************************************************************************

2.3 Flask 工作流程
==============================================================================

2.3.3 本地上下文
------------------------------------------------------------------------------

2.3.3.2 堆栈与 LocalStack 
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

承接上文 ， 其中 push() 方法和 pop() 方法分别用于向堆栈中推入和删除一个条目 。 具\
体的操作示例如下 ：

.. code-block:: python

    >>> class Stack:

            def __init__(self):
                self.items = []

            def push(self, item): # 推入条目
                self.items.append(item)

            def pop(self): # 移除并返回栈顶条目
                if self.is_empty:
                    return None
                return self.items.pop()

            @property
            def is_empty(self): # 判断是否为空
                return self.items == []

            @property
            def top(self): # 获取栈顶条目
                if self.is_empty:
                    return None
                return self.items[-1]

            
    >>> s = Stack()
    >>> s.push(42)
    >>> s.top
    42
    >>> s.push(24)
    >>> s.top
    24
    >>> s.pop()
    24
    >>> s.top
    42
    >>> 

Flask 中的上下文对象正是存储在这一类型的栈结构中 ， flask 这行代码创建了请求上下\
文堆栈 。 

.. code-block:: python 

    # context locals
    _request_ctx_stack = LocalStack()

从这里可以想到 ， 我们平时导入的 request 对象是保存在堆栈里的一个 \
RequestContext 实例 ， 导入的操作相当于获取堆栈的栈顶 （top） ， 它会返回栈顶的对\
象 （peek操作） ， 但并不删除它 。 

这两个堆栈对象使用 Werkzeug 提供的 LocalStack 类创建 ， 如代码清单所示 。 

.. code-block:: python 

    class LocalStack(object):

        def __init__(self):
            self._local = Local()
            self._lock = allocate_lock()

        def __release_local__(self):
            self._local.__release_local__()

        def __call__(self):
            def _lookup():
                rv = self.top
                if rv is None:
                    raise RuntimeError('object unbound')
                return rv
            return LocalProxy(_lookup)

        def push(self, obj):
            """Pushes a new item to the stack"""
            self._lock.acquire()
            try:
                rv = getattr(self._local, 'stack', None)
                if rv is None:
                    self._local.stack = rv = []
                rv.append(obj)
                return rv
            finally:
                self._lock.release()

        def pop(self):
            """Removes the topmost item from the stack, will return the
            old value or `None` if the stack was already empty.
            """
            self._lock.acquire()
            try:
                stack = getattr(self._local, 'stack', None)
                if stack is None:
                    return None
                elif len(stack) == 1:
                    release_local(self._local)
                    return stack[-1]
                else:
                    return stack.pop()
            finally:
                self._lock.release()

        @property
        def top(self):
            """The topmost item on the stack.  If the stack is empty,
            `None` is returned.
            """
            try:
                return self._local.stack[-1]
            except (AttributeError, IndexError):
                return None

简单来说 ， LocalStack 是基于 Local 实现的栈结构 （本地堆栈 ， 即实现了本地线程\
的堆栈） ， 和我们在前面编写的栈结构一样 ， 有 push() 、 pop() 方法以及获取栈顶\
的 top 属性 。 在构造函数中创建了 Local() 类的实例 _local 。 它把数据存储到 \
Local 中 ， 并将数据的字典名称设为 'stack' 。 注意这里和 Local 类一样也定义了 \
__call__ 方法 ， 当 LocalStack 实例被直接调用时 ， 会返回栈顶对象的代理 ， 即 \
LocalProxy 类实例 。 

这时会产生一个疑问 ， 为什么 Flask 使用 LocalStack 而不是直接使用 Local 存储上下\
文对象 。 主要的原因是为了支持多程序共存 。 将程序分离成多个程序很类似蓝本的模块化\
分离 ， 但它们并不是一回事 。 前面我们提到过 ， 使用 Werkzeug 提供的 \
DispatcherMiddleware 中间件就可以把多个程序组合成一个 WSGI 程序运行 。 

在上面的例子中 ， Werkzeug 会根据请求的 URL 来分发给对应的程序处理 。 在这种情况\
下 ， 就会有多个上下文对象存在 ， 使用栈结构就可以让多个程序上下文存在 ； 而活动\
的当前上下文总是可以在栈顶获得 ， 所以我们从 _request_ctx_stack.top 属性来获取当\
前的请求上下文对象 。 

2.3.3.3 代理与 LocalProxy 
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

代理 （Proxy） 是一种设计模式 ， 通过创建一个代理对象 。 我们可以使用这个代理对象\
来操作实际对象 。 从字面理解 ， 代理就是使用一个中间人来转发操作 。 代码清单是使\
用 Python 实现一个简单的代理类 。 

.. code-block:: python 

    class Proxy(object):

        def __init__(self, obj):
            object.__setattr__(self, '_obj', obj)
    
        def __getattr__(self, name):
            return getattr(self._obj, name)
    
        def __setattr__(self, name, value):
            self._obj[name] = value
    
        def __delattr__(self, name):
            del self._obj[name]

通过定义 __getattr__() 方法 、 __setattr__() 方法和 __delattr__() 方法 ， 它会\
把相关的获取 、 设置和删除操作转发给实例化代理类时传入的对象 。 下面的操作演示了\
这个代理类的使用方法 。 

.. code-block:: python

    >>> class Foo(object):

        def __init__(self, x):
            self.x = x

        def bar(self, y):
            self.x = y

            
    >>> foo = Foo('Peter')
    >>> p = Proxy(foo)
    >>> p.x
    'Peter'
    >>> p
    <__main__.Proxy object at 0x000002A81C6787C0>
    >>> p._obj
    <__main__.Foo object at 0x000002A81C678A00>
    >>> p.bar('Grey')
    >>> p.x
    'Grey'
    >>> foo.x
    'Grey'
    >>> 

Flask 使用 Werkzeug 提供的 LocalProxy 类来实现代理 ， 这是一个基于 Local 的本地\
代理 。 Local 类实例和 LocalStack 实例被调用时都会使用 LocalProxy 包装成一个代\
理 。 因此 ， 下面的代码中的堆栈对象都是代理 。

.. code-block:: python 

    _request_ctx_stack = LocalStack() # 请求上下文堆栈

如果要直接使用 LocalProxy 类实现代理 ， 需要在实例化时传入一个可调用对象 ， 比如\
传入的 lambda: _request_ctx_stack.top.request ： 

.. code-block:: python 

    request = LocalProxy(lambda: _request_ctx_stack.top.request)

LocalProxy 的定义如代码清单所示 : 

.. code-block:: python 

    class LocalProxy(object):

        __slots__ = ('__local', '__dict__', '__name__')

        def __init__(self, local, name=None):
            object.__setattr__(self, '_LocalProxy__local', local)
            object.__setattr__(self, '__name__', name)

        def _get_current_object(self):
            if not hasattr(self.__local, '__release_local__'):
                return self.__local()
            try:
                return getattr(self.__local, self.__name__)
            except AttributeError:
                raise RuntimeError('no object bound to %s' % self.__name__)

        @property
        def __dict__(self):
            try:
                return self._get_current_object().__dict__
            except RuntimeError:
                return AttributeError('__dict__')

        def __repr__(self):
            try:
                obj = self._get_current_object()
            except RuntimeError:
                return '<%s unbound>' % self.__class__.__name__
            return repr(obj)

        def __nonzero__(self):
            try:
                return bool(self._get_current_object())
            except RuntimeError:
                return False

        def __unicode__(self):
            try:
                return unicode(self._get_current_object())
            except RuntimeError:
                return repr(self)

        def __dir__(self):
            try:
                return dir(self._get_current_object())
            except RuntimeError:
                return []

        def __getattr__(self, name):
            if name == '__members__':
                return dir(self._get_current_object())
            return getattr(self._get_current_object(), name)

        def __setitem__(self, key, value):
            self._get_current_object()[key] = value

        def __delitem__(self, key):
            del self._get_current_object()[key]

        def __setslice__(self, i, j, seq):
            self._get_current_object()[i:j] = seq

        def __delslice__(self, i, j):
            del self._get_current_object()[i:j]

        __setattr__ = lambda x, n, v: setattr(x._get_current_object(), n, v)
        __delattr__ = lambda x, n: delattr(x._get_current_object(), n)
        __str__ = lambda x: str(x._get_current_object())
        __lt__ = lambda x, o: x._get_current_object() < o
        __le__ = lambda x, o: x._get_current_object() <= o
        __eq__ = lambda x, o: x._get_current_object() == o
        __ne__ = lambda x, o: x._get_current_object() != o
        __gt__ = lambda x, o: x._get_current_object() > o
        __ge__ = lambda x, o: x._get_current_object() >= o
        __cmp__ = lambda x, o: cmp(x._get_current_object(), o)
        __hash__ = lambda x: hash(x._get_current_object())
        __call__ = lambda x, *a, **kw: x._get_current_object()(*a, **kw)
        __len__ = lambda x: len(x._get_current_object())
        __getitem__ = lambda x, i: x._get_current_object()[i]
        __iter__ = lambda x: iter(x._get_current_object())
        __contains__ = lambda x, i: i in x._get_current_object()
        __getslice__ = lambda x, i, j: x._get_current_object()[i:j]
        __add__ = lambda x, o: x._get_current_object() + o
        __sub__ = lambda x, o: x._get_current_object() - o
        __mul__ = lambda x, o: x._get_current_object() * o
        __floordiv__ = lambda x, o: x._get_current_object() // o
        __mod__ = lambda x, o: x._get_current_object() % o
        __divmod__ = lambda x, o: x._get_current_object().__divmod__(o)
        __pow__ = lambda x, o: x._get_current_object() ** o
        __lshift__ = lambda x, o: x._get_current_object() << o
        __rshift__ = lambda x, o: x._get_current_object() >> o
        __and__ = lambda x, o: x._get_current_object() & o
        __xor__ = lambda x, o: x._get_current_object() ^ o
        __or__ = lambda x, o: x._get_current_object() | o
        __div__ = lambda x, o: x._get_current_object().__div__(o)
        __truediv__ = lambda x, o: x._get_current_object().__truediv__(o)
        __neg__ = lambda x: -(x._get_current_object())
        __pos__ = lambda x: +(x._get_current_object())
        __abs__ = lambda x: abs(x._get_current_object())
        __invert__ = lambda x: ~(x._get_current_object())
        __complex__ = lambda x: complex(x._get_current_object())
        __int__ = lambda x: int(x._get_current_object())
        __long__ = lambda x: long(x._get_current_object())
        __float__ = lambda x: float(x._get_current_object())
        __oct__ = lambda x: oct(x._get_current_object())
        __hex__ = lambda x: hex(x._get_current_object())
        __index__ = lambda x: x._get_current_object().__index__()
        __coerce__ = lambda x, o: x.__coerce__(x, o)
        __enter__ = lambda x: x.__enter__()
        __exit__ = lambda x, *a, **kw: x.__exit__(*a, **kw)

在 Python 类中 ， __foo 形式的属性会被替换为 _classname__foo 的形式 ， 这种开头\
加双下划线的属性在 Python 中表示类私有属性 （私有程度强于单下划线） 。 这也是为什\
么在 LocalProxy 类的构造函数设置了一个 _LocalProxy__local 属性 ， 而在其他方法中\
却可以简写为 __local 。 

这个代理的实现和我们在上面介绍的简单例子很相似 ， 不过这个代理中定义了更多的魔法方\
法 ， 大约有 50 多个 。 而且它还定义了一个 _get_current_object() 方法 ， 可以用\
来获取被代理的真实对象 。 这也是我们在本书第二部分 ， 获取被 current_user 代理的\
当前用户对象的方法 。 

那么 ，为什么 Flask 需要使用代理 ？ 总体来说 ， 在这里使用代理对象是因为这些代理\
可以在线程间共享 ， 让我们可以以动态的方式获取被代理的实际对象 。 具体来说 ， 我们\
在上节介绍过 Flask 的三种状态 ， 当上下文没被推送时 ， 响应的全局代理对象处于未绑\
定状态 。 而如果这里不使用代理 ， 那么在导入这些全局对象时就会尝试获取上下文 ， 然\
而这时堆栈是空的 ， 所以获取到的全局对象只能是 None 。 当请求进入并调用视图函数时 \
， 虽然这时堆栈里已经推入了上下文 ， 但这里导入的全局对象仍然是 None 。 总而言之 \
， 上下文的推送和移除是动态进行的 ， 而使用代理可以让我们拥有动态获取上下文对象的\
能力 。 

另外 ， 一个动态的全局对象 ， 也让多个程序实例并存有了可能 。 这样在不同的程序上下\
文环境中 ， current_app 总是能对应正确的程序实例 。 

2.3.3.4 请求上下文
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

在 Flask 中 ， 请求上下文由 _RequestContext 类表示 。 当请求进入时 ， 被作为 \
WSGI 程序调用的 Flask 类实例 （即我们的程序实例 app） 会在 wsgi_app() 方法中调\
用 Flask.request_context() 方法 。 这个方法会实例化 _RequestContext 类作为请求\
上下文对象 ， 接着 wsgi_app() 调用它的 push() 方法来将它推入请求上下文堆栈 。 \
_RequestContext 类的定义如代码清单所示 。 

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

构造函数中创建了 request 和 session 属性 ， request 对象使用 \
app.request_class(environ) 创建 ， 传入了包含请求信息的 environ 字典 。 而 \
session 在构造函数中只是 None ， 它会在 push() 方法中被调用 ， 即在请求上下文被\
推入请求上下文堆栈时创建 。 

和我们前面介绍的栈结构相似 ， push() 方法用于把请求上下文对象推入请求上下文堆栈 \
(_request_ctx_stack) ， 而 pop() 方法用来移出堆栈 。

另外 ， pop() 方法中还调用了 do_teardown_request() 方法 ， 这个方法会执行所有使\
用 teardown_request 钩子注册的函数 。 

魔法方法 __enter__() 和 __exit__() 分别在进入和退出 with 语句时调用 ， 这里用来\
在 with 语句调用前后分别推入和移出请求上下文 ， 具体见 PEP 343 \
（https://www.python.org/dev/peps/pep-0343/） 。 

请求上下文在 Flask 类的 wsgi_app 方法的开头创建 ， 在这个方法的最后没有直接调用 \
pop() 方法 ， 而是调用了 auto_pop() 方法来移除 。 也就是说 ， 请求上下文的生命周\
期开始于请求进入调用 wsgi_app() 时 ， 结束于响应生成后 。 

auto_pop() 方法在 _RequestContext 类中定义 ，如代码清单所示 。 

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