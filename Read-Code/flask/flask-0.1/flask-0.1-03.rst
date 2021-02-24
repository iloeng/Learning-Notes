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

**以下 need 修改**

构造函数中创建了 request 和 session 属性 ， request 对象使用 \
app.request_class(environ) 创建 ， 传入了包含请求信息的 environ 字典 。 而 \
session 在构造函数中只是 None ， 它会在 push() 方法中被调用 ， 即在请求上下文被\
推入请求上下文堆栈时创建 。  need 修改

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

.. code-block:: python 

    [flask/ctx.py：RequestContext.auto_pop（）]

    class RequestContext(object):
    ...
    def auto_pop(self, exc):
        if self.request.environ.get('flask._preserve_context') or \
        (exc is not None and self.app.preserve_context_on_exception):
            self.preserved = True
            self._preserved_exc = exc
        else:
            self.pop(exc)

这个方法里添加了一个 if 判断 ， 用来确保没有异常发生时才调用
pop（）方法移除上下文。异常发生时需要保持上下文以便进行相关操
作，比如在页面的交互式调试器中执行操作或是测试。

2.3.3.5 程序上下文
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

程序上下文对象AppContext类的定义和RequestContext类基本相
同，但要更简单一些。它的构造函数里创建了current_app变量指向的
app属性和g变量指向的g属性，如代码清单16-24所示。

.. code-block:: python 

    [flask/ctx.py：AppContext]

    class AppContext(object):
        def __init__(self, app):
            self.app = app
            self.url_adapter = app.create_url_adapter(None)
            self.g = app.app_ctx_globals_class()

        def push(self):
    ...
    def pop(self, exc=_sentinel):

你也许会困惑代理对象current_app和request命名的不一致，这是因
为如果将当前程序的代理对象命名为app会和程序实例的名称相冲突。
你可以把request理解成current request（当前请求）。
有两种方式创建程序上下文，一种是自动创建，当请求进入时，程
序上下文会随着请求上下文一起被创建。在RequestContext类中，程序
上下文在请求上下文推入之前推入，在请求上下文移除之后移除，如代
码清单16-25所示。

.. code-block:: python 

    [flask/ctx.py：请求上下文和程序上下文的生命周期关系]

    class RequestContext(object):
        def __init__(self, app, environ, request=None):
            self.app = app
            if request is None:
                request = app.request_class(environ)
                self.request = request
            ...
        def push(self):
            ...
            # 在推入请求上下文前先推入程序上下文
            app_ctx = _app_ctx_stack.top
            if app_ctx is None or app_ctx.app != self.app:
                app_ctx = self.app.app_context() # 获取程序上下文对象
                app_ctx.push() # 将程序上下文对象推入堆栈（_app_ctx_stack）
                self._implicit_app_ctx_stack.append(app_ctx)
            else:
            ...

而在没有请求处理的时候，你就需要手动创建上下文。你可以使用
程序上下文对象中的push（）方法，也可以使用with语句。
我们用来构建URL的url_for（）函数会优先使用请求上下文对象提
供的url_adapter，如果请求上下文没有被推送，则使用程序上下文提供
的url_adapter。所以AppContext的构造函数里也同样创建了url_adapter属
性。
g使用保存在app_ctx_globals_class属性的_AppCtxGlobals类表示，
只是一个普通的类字典对象。我们可以把它看作“增加了本地线程支持
的全局变量”。有一个常见的疑问是，为什么说每次请求都会重设g？这
是因为g保存在程序上下文中，而程序上下文的生命周期是伴随着请求
上下文产生和销毁的。每个请求都会创建新的请求上下文堆栈，同样也
会创建新的程序上下文堆栈，所以g会在每个新请求中被重设。
程序上下文和请求上下文的联系非常紧密（在代码中就可以看
出）。如果你在前面阅读了0.1版本的代码，你会发现在flask.py底部，
全局对象创建时只存在一个请求上下文堆栈。四个全局对象都从请求上
下文中获取。可以说，程序上下文是请求上下文的衍生物。这样做的原
因主要是为了更加灵活。程序中确实存在着两种明显的状态，分离开可
以让上下文的结构更加清晰合理。这也方便了测试等不需要请求存在的
使用场景，这时只需要单独推送程序上下文，而且这个分离催生出了
Flask的程序运行状态。

2.3.3.6 总结
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Flask中的上下文由表示请求上下文的RequestContext类实例和表示
程序上下文的AppContext类实例组成。请求上下文对象存储在请求上下
文堆栈（_request_ctx_stack）中，程序上下文对象存储在程序上下文堆
栈（_app_ctx_stack）中。而request、session则是保存在RequestContext
中的变量，相对地，current_app和g则是保存在AppContext中的变量。当
然，request、session、current_app、g变量所指向的实际对象都有相应的
类：

- request——Request
- session——SecureCookieSession
- current_app——Flask
- g——_AppCtxGlobals

看到这里，想必你已经对上下文有了比较深入的认识。现在你再回
头看globals模块的代码，应该就会非常容易理解了。我们可以来总结一
下，这一系列事物为什么要存在。当第一个请求发来的时候：

1. 需要保存请求相关的信息——有了请求上下文。
#. 为了更好地分离程序的状态，应用起来更加灵活——有了程序上下文。
#. 为了让上下文对象可以在全局动态访问，而不用显式地传入视图函数，同时确保线程安全——有了Local（本地线程）。
#. 为了支持多个程序——有了LocalStack（本地堆栈）。
#. 为了支持动态获取上下文对象——有了LocalProxy（本地代理）。
#. ……
#. 为了让这一切愉快的工作在一起——有了Flask。

2.3.4 请求与响应对象
------------------------------------------------------------------------------

2.3.4.1 请求对象
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

一个请求从客户端发出，假如忽略掉更深的细节，它大致经过了这
些变化：从HTTP请求报文，到符合WSGI规定的Python字典，再到
Werkzeug中的werkzeug.wrappers.Request对象，最后再到Flask中我们熟
悉的请求对象request。
前面我们说过，从flask中导入的request是代理，被代理的实际对象
是请求上下文RequestContext对象的request属性，这个属性存储的是
Request类实例，这个Request才是表示请求的请求对象，如代码清单16-
26所示。

.. code-block:: python 

    [flask/wrappers.py：Request]

    from werkzeug.wrappers import Request as RequestBase
    class JSONMixin(object):
    ... # 定义is_json、json属性和get_json()方法

    class Request(RequestBase, JSONMixin):
        url_rule = None
        view_args = None
        routing_exception = None

        @property
        def max_content_length(self):
        """返回配置变量MAX_CONTENT_LENGTH的值"""
            if current_app:
                return current_app.config['MAX_CONTENT_LENGTH']
        
        @property
        def endpoint(self):
        """与请求相匹配的端点。"""
            if self.url_rule is not None:
                return self.url_rule.endpoint
        
        @property
        def blueprint(self):
        """当前蓝本名称。"""
        if self.url_rule and '.' in self.url_rule.endpoint:
            return self.url_rule.endpoint.rsplit('.', 1)[0]
        ...

Request类继承Werkzeug提供的Request类和添加JSON支持的
JSONMixin类。请求对象request的大部分属性都直接继承Werkzeug中
Request类的属性，比如method、args等。Flask中的这个Request类主要
添加了一些Flask特有的属性，比如表示所在蓝本的blueprint属性，或是
为了方便获取当前端点的endpoint属性等。
Flask允许我们自定义请求类，通常情况下，我们会子类化这个
Request类，并添加一些自定义的设置，然后把这个自定义请求类赋值
给程序实例的request_class属性。

2.3.4.1 响应对象
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

一般情况下，在编写程序时我们并不需要直接与响应打交道。在
Flask中的请求-响应循环中，我们知道响应是由finalize_request（）方法生成的，它调用了flask.Flask.make_response（）方法生成响应对象，传
入的rv参数是dispatch_request（）的返回值，也就是视图函数的返回
值。
我们在前面介绍过，视图函数可以返回多种类型的返回值。完整的
合法返回值如表16-3所示。

..image: img/2-4.png

这个Flask.make_response（）方法主要的工作就是判断返回值是表
16-3中的哪一种类型，最后根据类型做相应处理，最后生成一个响应对
象并返回它。响应对象为Response类的实例，Response类在wrappers.py
脚本中定义，如代码清单16-27所示。

.. code-block:: python 

    [flask/wrappers.py：Response]

    from werkzeug.wrappers import Response as ResponseBase
    class JSONMixin(object):
        ...

    class Response(ResponseBase, JSONMixin):
        default_mimetype = 'text/html'

        def _get_data_for_json(self, cache):
            return self.get_data()
    
        @property
        def max_cookie_size(self):
        """返回配置变量MAX_COOKIE_SIZE的值"""
            if current_app:
                return current_app.config['MAX_COOKIE_SIZE']
            # 上下文未推送时返回Werkzeug中Response类的默认值
            return super(Response, self).max_cookie_size

和Request类相似，这个响应对象继承Werkzeug中的Response类和添
加JSON支持的JSONMixin类。这个类比Request类更简单，只是设置了
默认的MIME类型。
Flask也允许你自定义响应类，自定义的响应类通常会继承自内置的
Response类，然后赋值给flask.Flask.response_class属性。

2.3.5 session 
------------------------------------------------------------------------------

在开始介绍session的实现之前，我们有必要再重申一下措辞问题。
我会使用下面的方式来表述三个与session相关的内容：Flask提供
了“session变量/对象”来操作“用户会话（Session）”，它把用户会话保存
在“一块名/键为session的cookie”中。
我们在第2章对session进行过简单的介绍，现在我们来深入了解它
的一些具体细节。在Flask中使用session非常简单，只需要设置好密钥，
就可以在视图函数中操作session对象：

.. code-block:: python  

    from flask import Flask, session
    app = Flask(__name__)
    app.secret_key = 'secret string'

    @app.route('/')
    def hello():
        session['answer'] = 42
        return '<h1>Hello, Flask!</h1>'

当第一次介绍session时我们曾说它“可以记住请求间的值”，很多人
会对这句话感到困惑。就这个例子来说，当用户访问hello视图时，会把
数字42存储到session对象里，以answer作为键。假如我再定义一个bingo
视图，当用户访问bingo视图时，我们可以在bingo视图里再次从session
通过answer键获取这个数字。这一存一取背后的逻辑是这样的：
向session中存储值时，会生成加密的cookie加入响应。这时用户的
浏览器接收到响应会将cookie存储起来。当用户再次发起请求时，浏览
器会自动在请求报文中加入这个cookie值。Flask接收到请求会把session
cookie的值解析到session对象里。这时我们就可以再次从session中读取
内容。
我们在向session中存数字的这行代码设置断点：

:: 

    session['answer'] = 42

2.3.5.1 操作 session
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

在前面我们学习过，session变量在globals模块中定义：

.. code-block:: python 

    session = LocalProxy(partial(_lookup_req_object, 'session'))

它会调用_lookup_req_object（）函数，传入name参数的值为'session'：

.. code-block:: python 

    def _lookup_req_object(name):
        top = _request_ctx_stack.top
        if top is None:
            raise RuntimeError(_request_ctx_err_msg)
        return getattr(top, name)

从上面的代码中可以看到Flask从请求上下文堆栈的栈顶
（_request_ctx_stack.top）获取请求上下文，从用于获取属性的内置函数
getattr（）可以看出session是请求上下文对象（即RequestContext）的一
个属性，这也就意味着，session变量是在生成请求上下文的时候创建
的，后面我们会详细了解它的生成过程。
继续步进代码后，会执行LocalProxy类的 __setitem__（）方法，它
会把设置操作转发给真实的session对象：

.. code-block:: python 

    class LocalProxy(object):
        ...
        def __setitem__(self, key, value):
            self._get_current_object()[key] = value

这时在调试工具栏右侧的变量列表中可以看到已经被代理的session
对象实际上是sessions模块中的SecureCookieSession类的实例。
在Werkzeug中进行一系列查询工作后，最终执行了
SecureCookieSession类中的on_update（）方法，这个方法会将两个属性
self.modified和self.accessed设为True，说明更新（modify）并使用
（access）了session。这两个标志会在保存session的方法中使用，我们
下面会了解到。
那么session是否被更新是如何判断的？这个on_update（）方法又是
如何被执行的呢？要解答这些问题，我们需要先停止步进，在
SecureCookieSession中探索一下。首先可以看到Secure-CookieSession类
继承了CallbackDict类，CallbackDict在Werkzeug中定义，如代码清单16-
28所示。

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