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
_RequestContext 实例 ， 导入的操作相当于获取堆栈的栈顶 （top） ， 它会返回栈顶的对\
象 （peek操作） ， 但并不删除它 。 

这个堆栈对象使用 Werkzeug 提供的 LocalStack 类创建 ， 如代码清单所示 。 

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

简单来说 ， LocalStack 是基于 Local 实现的栈结构 （本地堆栈 ， 即实现了本地线程的\
堆栈） ， 和我们在前面编写的栈结构一样 ， 有 push() 、 pop() 方法以及获取栈顶的 \
top 属性 。 在构造函数中创建了 Local() 类的实例 _local 。 它把数据存储到 Local \
中 ， 并将数据的字典名称设为 'stack' 。 注意这里和 Local 类一样也定义了 __call__ \
方法 ， 当 LocalStack 实例被直接调用时 ， 会返回栈顶对象的代理 ， 即 LocalProxy \
类实例 。 

这时会产生一个疑问 ， 为什么 Flask 使用 LocalStack 而不是直接使用 Local 存储上下\
文对象 。 主要的原因是为了支持多程序共存 。 将程序分离成多个程序很类似蓝本的模块化\
分离 ， 但它们并不是一回事 。 前面我们提到过 ， 使用 Werkzeug 提供的 \
DispatcherMiddleware 中间件就可以把多个程序组合成一个 WSGI 程序运行 。 

在上面的例子中 ， Werkzeug 会根据请求的 URL 来分发给对应的程序处理 。 在这种情况下 \
， 就会有多个上下文对象存在 ， 使用栈结构就可以让多个程序上下文存在 ； 而活动的当前\
上下文总是可以在栈顶获得 ， 所以我们从 _request_ctx_stack.top 属性来获取当前的请求\
上下文对象 。 

2.3.3.3 代理与 LocalProxy 
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

代理 （Proxy） 是一种设计模式 ， 通过创建一个代理对象 。 我们可以使用这个代理对象来\
操作实际对象 。 从字面理解 ， 代理就是使用一个中间人来转发操作 。 代码清单是使用 \
Python 实现一个简单的代理类 。 

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

通过定义 __getattr__() 方法 、 __setattr__() 方法和 __delattr__() 方法 ， 它会把\
相关的获取 、 设置和删除操作转发给实例化代理类时传入的对象 。 下面的操作演示了这个代\
理类的使用方法 。 

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

Flask 使用 Werkzeug 提供的 LocalProxy 类来实现代理 ， 这是一个基于 Local 的本地代\
理 。 Local 类实例和 LocalStack 实例被调用时都会使用 LocalProxy 包装成一个代理 \
。 因此 ， 下面的代码中的堆栈对象都是代理 。

.. code-block:: python 

    _request_ctx_stack = LocalStack() # 请求上下文堆栈

如果要直接使用 LocalProxy 类实现代理 ， 需要在实例化时传入一个可调用对象 ， 比如传\
入的 lambda: _request_ctx_stack.top.request ： 

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

在 Python 类中 ， __foo 形式的属性会被替换为 _classname__foo 的形式 ， 这种开头加\
双下划线的属性在 Python 中表示类私有属性 （私有程度强于单下划线） 。 这也是为什么在 \
LocalProxy 类的构造函数设置了一个 _LocalProxy__local 属性 ， 而在其他方法中却可以\
简写为 __local 。 

这个代理的实现和我们在上面介绍的简单例子很相似 ， 不过这个代理中定义了更多的魔法方法 \
， 大约有 50 多个 。 而且它还定义了一个 _get_current_object() 方法 ， 可以用来获取\
被代理的真实对象 。 这也是我们在本书第二部分 ， 获取被 current_user 代理的当前用户\
对象的方法 。 

那么 ，为什么 Flask 需要使用代理 ？ 总体来说 ， 在这里使用代理对象是因为这些代理可以\
在线程间共享 ， 让我们可以以动态的方式获取被代理的实际对象 。 具体来说 ， 我们在上节\
介绍过 Flask 的三种状态 ， 当上下文没被推送时 ， 响应的全局代理对象处于未绑定状态 \
。 而如果这里不使用代理 ， 那么在导入这些全局对象时就会尝试获取上下文 ， 然而这时堆\
栈是空的 ， 所以获取到的全局对象只能是 None 。 当请求进入并调用视图函数时 ， 虽然这\
时堆栈里已经推入了上下文 ， 但这里导入的全局对象仍然是 None 。 总而言之 ， 上下文的\
推送和移除是动态进行的 ， 而使用代理可以让我们拥有动态获取上下文对象的能力 。 

另外 ， 一个动态的全局对象 ， 也让多个程序实例并存有了可能 。 这样在不同的程序上下文\
环境中 ， current_app 总是能对应正确的程序实例 。 

2.3.3.4 请求上下文
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

在 Flask 中 ， 请求上下文由 _RequestContext 类表示 。 当请求进入时 ， 被作为 \
WSGI 程序调用的 Flask 类实例 （即我们的程序实例 app） 会在 wsgi_app() 方法中调用 \
Flask.request_context() 方法 。 这个方法会实例化 _RequestContext 类作为请求上下\
文对象 ， 接着 wsgi_app() 调用它的 push() 方法来将它推入请求上下文堆栈 。 \
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

构造函数 __init 中创建了 request 和 session 属性 ， request 对象使用 \
app.request_class(environ) 创建 ， 传入了包含请求信息的 environ 字典 。 而 \
session 在构造函数中是 app.open_session(self.request) 。

.. code-block:: python 

    class Flask(object):
        def open_session(self, request):
            """Creates or opens a new session.  Default implementation stores all
            session data in a signed cookie.  This requires that the
            :attr:`secret_key` is set.

            :param request: an instance of :attr:`request_class`.
            """
            key = self.secret_key
            if key is not None:
                return SecureCookie.load_cookie(request, self.session_cookie_name,
                                                secret_key=key)

当设置 secret_key 后 ， self.session 值为 load_cookie 的执行结果 ， 否则为空 。 \
它会在 push() 方法中被调用 ， 即在请求上下文被推入请求上下文堆栈时创建 。  

魔法方法 __enter__() 和 __exit__() 分别在进入和退出 with 语句时调用 ， 这里用来\
在 with 语句调用前后分别推入和移出请求上下文 ， 具体见 PEP 343 \
（https://www.python.org/dev/peps/pep-0343/） 。 

请求上下文在 Flask 类的 wsgi_app 方法的开头创建 ， 在这个方法的最后调用 pop() 方法\
来移除 。 也就是说 ， 请求上下文的生命周期开始于请求进入调用 wsgi_app() 时 ， 结束\
于响应生成后 。 

__exit__ 这个方法里添加了一个 if 判断 ， 用来确保没有异常发生时才调用 pop() 方法移\
除上下文 。 异常发生时需要保持上下文以便进行相关操作 ， 比如在页面的交互式调试器中执\
行操作或是测试 。 

2.3.3.5 程序上下文
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

在 v0.1 版本中并没有声明程序上下文的类定义 (以后的版本中出现了) ， 也就是说不存在程\
序上下文的类 。 但是在代码中有两个全局变量可以认为是程序上下文变量 。 

.. image:: img/2-4.png
    :align: center
    :alt: http://myndtt.github.io/images/55.png
    :name: 《Flask Web开发实战：入门、进阶与原理解析》
    :target: none

也是在请求上下文中进行了初始化 ， current_app 变量指向的 app 属性和 g 变量指向的 \
g 属性 ， 你也许会困惑代理对象 current_app 和 request 命名的不一致 ， 这是因为如果\
将当前程序的代理对象命名为 app 会和程序实例的名称相冲突 。 你可以把 request 理解成 \
current request （当前请求） 。 有两种方式创建程序上下文 ， 一种是自动创建 ， 当请\
求进入时 ， 程序上下文会随着请求上下文一起被创建 。 在 _RequestContext 类中 ， 程序\
上下文和请求上下文一起初始化然后推入 ， 在请求上下文移除之后移除 。 

用来构建 URL 的 url_for() 函数会使用请求上下文对象提供的 url_adapter 。

g 使用保存在 _request_ctx_stack.top.g 属性的 _RequestGlobals() 类表示 ， 是一个\
普通的类字典对象 。 可以把它看作 “增加了本地线程支持的全局变量” 。 有一个常见的疑问\
是 ， 为什么说每次请求都会重设 g ？ 这是因为 g 保存在程序上下文中 ， 而程序上下文的\
生命周期是伴随着请求上下文产生和销毁的 。 每个请求都会创建新的请求上下文堆栈 ， 同样\
也会创建新的程序上下文堆栈 ， 所以 g 会在每个新请求中被重设 。 

程序上下文和请求上下文的联系非常紧密 （在代码中就可以看出） 。 阅读 0.1 版本的代码 \
， 你会发现在 flask.py 底部 ， 全局对象创建时只存在一个请求上下文堆栈 。 四个全局对\
象都从请求上下文中获取 。 可以说程序上下文是请求上下文的衍生物 。 这样做的原因主要是\
为了更加灵活 。 程序中确实存在着两种明显的状态 ， 分离开可以让上下文的结构更加清晰合\
理 。 这也方便了测试等不需要请求存在的使用场景 ， 这时只需要单独推送程序上下文 ， 而\
且这个分离催生出了 Flask 的程序运行状态 。 

2.3.3.6 总结
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Flask 中的上下文由表示请求上下文的 _RequestContext 类实例和表示程序上下文的 \
current_app 和 g 组成 。 请求上下文对象存储在请求上下文堆栈 (_request_ctx_stack) \
中 ， 程序上下文对象存储在请求上下文堆栈 (_request_ctx_stack) 中 。 request 、 g \
、 session 和 current_app 都是保存在 _RequestContext 中的变量 。 当然 ， \
request 、 session 、 current_app 、 g 变量所指向的实际对象都有相应的类 ： 

- request —— Request
- session —— SecureCookieSession
- current_app —— Flask
- g —— _RequestGlobals

当第一个请求发来的时候 ： 

1. 需要保存请求相关的信息 —— 有了请求上下文 。 
#. 为了更好地分离程序的状态 ， 应用起来更加灵活 —— 有了程序上下文 。 
#. 为了让上下文对象可以在全局动态访问 ， 而不用显式地传入视图函数 ， 同时确保线程安\
   全 —— 有了 Local （本地线程） 。 
#. 为了支持多个程序 —— 有了 LocalStack （本地堆栈） 。
#. 为了支持动态获取上下文对象 —— 有了 LocalProxy （本地代理） 。
#. ……
#. 为了让这一切愉快的工作在一起 —— 有了Flask 。 

2.3.4 请求与响应对象
------------------------------------------------------------------------------

2.3.4.1 请求对象
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

一个请求从客户端发出 ， 假如忽略掉更深的细节 ， 它大致经过了这些变化 ： 从 HTTP 请求\
报文 ， 到符合 WSGI 规定的 Python 字典 ， 再到 Werkzeug 中的 \
werkzeug.wrappers.Request 对象 ， 最后再到 Flask 中我们熟悉的请求对象 request 。 

前面说过 ， 从 flask 中导入的 request 是代理 ， 被代理的实际对象是请求上下文 \
_RequestContext 对象的 request 属性 ， 这个属性存储的是 Request 类实例 ， 这个 \
Request 才是表示请求的请求对象 ， 如代码清单所示 。 

未完待续 ...

上一篇文章 ： `上一篇`_

下一篇文章 ： `下一篇`_ 

.. _`上一篇`: flask-0.1-02.rst
.. _`下一篇`: flask-0.1-04.rst