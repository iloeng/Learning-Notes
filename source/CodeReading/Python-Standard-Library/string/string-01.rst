##############################################################################
Python 标准库之 String 01
##############################################################################

.. contents::

******************************************************************************
第 1 章  文本处理服务 
******************************************************************************

1.1 ``sting`` 常用字符串操作 
==============================================================================

1.1.1 字符串常量
------------------------------------------------------------------------------

此模块中定义的常量为 ：

- ``string.ascii_letters``
  
  英文大小写 ASCII 字符 ， 下文所述 ascii_lowercase 和 ascii_uppercase 常量的拼\
  连 。 该值不依赖于语言区域 。

- ``string.ascii_lowercase``

  小写字母 'abcdefghijklmnopqrstuvwxyz' 。 该值不依赖于语言区域 ， 不会发生改变 。

- ``string.ascii_uppercase``

  大写字母 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' 。 该值不依赖于语言区域 ， 不会发生改变 。

- ``string.digits``
  
  数字字符串 '0123456789' 。

- ``string.hexdigits``

  十六进制字符串 '0123456789abcdefABCDEF' 。

- ``string.octdigits``
  
  八进制字符串 '01234567' 。

- ``string.punctuation``
  
  由在 C 语言区域中被视为标点符号的 ASCII 字符组成的字符串 。

- ``string.printable``

  由被视为可打印符号的 ASCII 字符组成的字符串 。 这是 digits ， ascii_letters ， \
  punctuation 和 whitespace 的集合 。

- ``string.whitespace``
  
  由被视为空白符号的 ASCII 字符组成的字符串 。 其中包括空格 、 制表 、 换行 、 回车 \
  、 进纸和纵向制表符 。


1.1.2 自定义字符串格式化
------------------------------------------------------------------------------

内置的字符串类提供了通过使用 `PEP 3101`_ 所描述的 `format()`_ 方法进行复杂变量替换\
和值格式化的能力 。 `string`_ 模块中的 `Formatter`_ 类允许你使用与内置 `format()`_ \
方法相同的实现来创建并定制你自己的字符串格式化行为 。 

.. _`PEP 3101`: https://www.python.org/dev/peps/pep-3101

.. _`format()`: https://docs.python.org/zh-cn/3.7/library/stdtypes.html#str.format

.. _`string`: https://docs.python.org/zh-cn/3.7/library/string.html#module-string

.. _`Formatter`: https://docs.python.org/zh-cn/3.7/library/string.html#string.Formatter

Formatter 类提供的方法有 ： 

1.1.2.1 format 方法
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

源码如下 ： 

.. code-block:: python 

    class Formatter:
        def format(*args, **kwargs):
            if not args:
                raise TypeError("descriptor 'format' of 'Formatter' object "
                                "needs an argument")
            self, *args = args  # allow the "self" keyword be passed
            try:
                format_string, *args = args # allow the "format_string" keyword be passed
            except ValueError:
                raise TypeError("format() missing 1 required positional "
                                "argument: 'format_string'") from None
            return self.vformat(format_string, args, kwargs)

如果没有参数就会抛出异常 ， 然后对函数的参数进行拆解 ， `*args` 是一个 tuple 元组 \
， 第一个是类对象本身 ， 后面依次是输入的参数 ， 我们将源码进行稍微的改动如下 ： 

.. code-block:: python 

    # Test Code in IPython

    In [8]: class YYYY:
    ...:         def format(*args, **kwargs):
    ...:             if not args:
    ...:                 raise TypeError("descriptor 'format' of 'Formatter' object "
    ...:                                 "needs an argument")
    ...:             print(args)
    ...:             self, *args = args  # allow the "self" keyword be passed
    ...:             print(self, args)
    ...:             try:
    ...:                 print(args)
    ...:                 format_string, *args = args # allow the "format_string" keyword be passed
    ...:                 print(format_string, args)
    ...:             except ValueError:
    ...:                 raise TypeError("format() missing 1 required positional "
    ...:                                 "argument: 'format_string'") from None
    ...:

    In [9]: a = YYYY()

    In [10]: a.format('1', '22', '3', '4', '5','6')
    (<__main__.YYYY object at 0x000001A9D01E9D48>, '1', '22', '3', '4', '5', '6')
    <__main__.YYYY object at 0x000001A9D01E9D48> ['1', '22', '3', '4', '5', '6']
    ['1', '22', '3', '4', '5', '6']
    1 ['22', '3', '4', '5', '6']

所以第一次拆解的时候 ， `self` 被赋值为类对象 ， `*args` 为后面的参数 。 在 try 内\
部有进行了一次拆解 ， `format_string` 被赋值为第一个参数 ， 同理 `*args` 是从第二个\
参数开始以后的参数 。 

对传入的参数进行拆解后 ， 随后将拆解后的结果传入到 `vformat()` 方法中 ， 进行下一步\
处理 。 

1.1.2.2 vformat 方法
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

其代码如下 ： 

.. code-block:: python  

    class Formatter:

        def vformat(self, format_string, args, kwargs):
            used_args = set()
            result, _ = self._vformat(format_string, args, kwargs, used_args, 2)
            self.check_unused_args(used_args, args, kwargs)
            return result

首先进入到这个函数中 ， used_args 是一个集合类型 ， 意味着不能包含重复的元素 ， 然后\
就执行了两个函数 ， 一个是类私有方法 `_vformat()` ， 一个是共有方法 \
`check_unused_args()` ， 最后会返回私有方法 `_vformat()` 的执行结果 。 有两个方法\
功能未知 ， 先放下继续向下阅读 。 

1.1.2.3 _vformat 方法
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

其代码如下 ： 

.. code-block:: python  

    class Formatter:

        def _vformat(self, format_string, args, kwargs, used_args, recursion_depth,
                    auto_arg_index=0):
            if recursion_depth < 0:
                raise ValueError('Max string recursion exceeded')
            result = []
            for literal_text, field_name, format_spec, conversion in \
                    self.parse(format_string):

                # output the literal text
                if literal_text:
                    result.append(literal_text)

                # if there's a field, output it
                if field_name is not None:
                    # this is some markup, find the object and do
                    #  the formatting

                    # handle arg indexing when empty field_names are given.
                    if field_name == '':
                        if auto_arg_index is False:
                            raise ValueError('cannot switch from manual field '
                                            'specification to automatic field '
                                            'numbering')
                        field_name = str(auto_arg_index)
                        auto_arg_index += 1
                    elif field_name.isdigit():
                        if auto_arg_index:
                            raise ValueError('cannot switch from manual field '
                                            'specification to automatic field '
                                            'numbering')
                        # disable auto arg incrementing, if it gets
                        # used later on, then an exception will be raised
                        auto_arg_index = False

                    # given the field_name, find the object it references
                    #  and the argument it came from
                    obj, arg_used = self.get_field(field_name, args, kwargs)
                    used_args.add(arg_used)

                    # do any conversion on the resulting object
                    obj = self.convert_field(obj, conversion)

                    # expand the format spec, if needed
                    format_spec, auto_arg_index = self._vformat(
                        format_spec, args, kwargs,
                        used_args, recursion_depth-1,
                        auto_arg_index=auto_arg_index)

                    # format the object and append to the result
                    result.append(self.format_field(obj, format_spec))

            return ''.join(result), auto_arg_index

这个私有方法一共有 6 个参数 ， 其中 5 个必选参数 ， 一个可选参数 。 分别是 ： 

- format_string : 格式化字符串

- args : 待定

- kwargs : 待定

- used_args : 待定

- recursion_depth : 递归深度

- auto_arg_index : 待定

进入方法内部 ， 首先判断 recursion_depth 的值 ， 如果小于 0 ， 抛出值异常 。 然后\
创建一个空 result list 存放结果 。 接着进行 for 循环解析 format_string 格式化字符\
串 ， 对解析结果进行拆包 。 解析格式化字符串时调用了 `parse` 函数 。 拆解之后又 4 \
个结果 ， 分别是 ： literal_text ， field_name ， format_spec ， conversion

进入 `parse` 函数看看 ： `1.1.2.4 parse 方法`_ 

然后判断 literal_text 值是否存在 ， 如果存在就将 literal_text 追加到 result ； 接\
着判断 field_name 字段名是否为空 ：

1. 当 field_name 为空值时 
    1. 判断 auto_arg_index 是否为 False ， 如为 False ， 则抛出值异常
    
    2. 将 auto_arg_index 转换为字符串并赋值给 field_name ， 同时 auto_arg_index \
       增加 1 

2. 如果 field_name 为数字
    1. 如果 auto_arg_index 值是正常的 ， 抛出值异常

    2. 将 auto_arg_index 赋值为 False

接下来用 obj, arg_used 变量存储 `get_field` 函数的返回结果 ， 并将 arg_used 添加\
到 used_args 参数中 ， 接着将 obj 赋值为转换字段 `convert_field` 函数的处理结果 \
， 然后是递归处理 ， 再次执行 `_vformat` 方法 ， 其结果存储为 format_spec ， \
auto_arg_index 。 然后对 obj 和 format_spec 变量进行格式化字段 `format_field` 方\
法处理 ， 并将结果追加到 result 列表中 。 

最终返回一个含有两个元素元组 ： 1. result 列表拼接后的字符串 ； 2. auto_arg_index 。

进入 `get_field` 方法查看 ： `1.1.2.5 get_field 方法`_

进入 `convert_field` 方法查看 ： `1.1.2.6 convert_field 方法`_

进入 `format_field` 方法查看 ： `1.1.2.7 format_field 方法`_


1.1.2.4 parse 方法
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

代码很简短 ： 

.. code-block:: python  

    class Formatter:

        def parse(self, format_string):
            return _string.formatter_parser(format_string)

该函数返回了 ``_string.formatter_parser`` 函数执行结果 。 而 \
``_string.formatter_parser`` 函数是 string 的内置方法 ， 暂时不知道其作用 ， 先放\
下 。 继续向下阅读 get_field 方法 。

1.1.2.5 get_field 方法
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

源码如下 ： 

.. code-block:: python 

    class Formatter:

        def get_field(self, field_name, args, kwargs):
            first, rest = _string.formatter_field_name_split(field_name)

            obj = self.get_value(first, args, kwargs)

            # loop through the rest of the field_name, doing
            #  getattr or getitem as needed
            for is_attr, i in rest:
                if is_attr:
                    obj = getattr(obj, i)
                else:
                    obj = obj[i]

            return obj, first

首先其参数是 3 个 ， 分别是 field_name : 字段名称

1.1.2.6 convert_field 方法
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

源码如下 ： 

.. code-block:: python 

    class Formatter:

        def convert_field(self, value, conversion):
            # do any conversion on the resulting object
            if conversion is None:
                return value
            elif conversion == 's':
                return str(value)
            elif conversion == 'r':
                return repr(value)
            elif conversion == 'a':
                return ascii(value)
            raise ValueError("Unknown conversion specifier {0!s}".format(conversion))

1.1.2.7 format_field 方法
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

