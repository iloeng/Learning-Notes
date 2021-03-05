##############################################################################
Python Web 模块之 Flask v0.1
##############################################################################

.. contents::

******************************************************************************
第 2 部分  源码阅读准备 
******************************************************************************

2.3 Flask 工作流程
==============================================================================

2.3.6 模板渲染 
------------------------------------------------------------------------------

承接上文 ， 我们使用 context_processor 装饰器注册模板上下文处理函数 ， 这些处理函数\
被存储在 Flask.template_context_processors 列表里 ： 

.. code-block:: python  

    [flask.py]

    class Flask(object):

        self.template_context_processors = [_default_template_ctx_processor]

        def context_processor(self, f):
            """Registers a template context processor function."""
            self.template_context_processors.append(f)
            return f

列表中是函数的名称 ， 默认的处理函数是 _default_template_ctx_processor() ， 它把\
当前上下文中的 request 、 session 和 g 注入模板上下文 。 

.. code-block:: python 

    [flask.py]

    def _default_template_ctx_processor():
        """Default template context processor.  Injects `request`,
        `session` and `g`.
        """
        reqctx = _request_ctx_stack.top
        return dict(
            request=reqctx.request,
            session=reqctx.session,
            g=reqctx.g
        )

这个 update_template_context() 方法的主要任务就是调用这些模板上下文处理函数 ， 获\
取返回的字典 ， 然后统一添加到 context 字典 。 这里先复制原始的 context 并在最后更\
新了它 ， 这是为了确保最初设置的值不被覆盖 ， 即视图函数中使用 render_template() \
函数传入的上下文参数优先 。 

render_template() 函数最后使用这个 context 字典调用了 render() 函数 。 代码如下所\
示 : 

.. code-block:: python 

    [flask.py]

    def render_template(template_name, **context):
        current_app.update_template_context(context)
        return current_app.jinja_env.get_template(template_name).render(context)

这里对程序实例 app 调用的 Flask.jinja_env() 方法 ， 代码如下所示 : 

.. code-block:: python 

    [flask.py]

    self.jinja_env = Environment(loader=self.create_jinja_loader(),
                                     **self.jinja_options)

它调用 jinja2.Environment 类创建了一个 Jinja2 环境 ， 用于加载模板 。 这个属性完\
成了 Jinja2 环境在 Flask 中的初始化 ， 向模板上下文中添加了一些全局对象 (比如 \
url_for() 函数 、 get_flashed_messages() 函数以及 config 对象等) ， 更新了一些渲\
染设置 。 

虽然之前已经通过调用 update_template_context() 方法向模板上下文中添加了 request \
、 session 、 g (由 _default_template_ctx_processor() 获取) ， 这里再次添加是为\
了让导入的模板也包含这些变量 。 

在调用 render() 函数前 ， 经过了一段非常漫长的调用过程 ： 模板文件定位 、 加载 、 \
解析等 。 这个函数是 Jinja2 的 render 函数渲染模板 ， 并在渲染前后发送相应的信号 \
。 渲染工作结束后会返回渲染好的 unicode 字符串 ， 这个字符串就是最终的视图函数返回\
值 ， 即响应的主体 ， 也就是返回给浏览器的 HTML 页面 。 

