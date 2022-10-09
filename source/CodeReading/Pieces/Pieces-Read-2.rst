Pieces 源码阅读系列 2 编码器
---------------------------------

前言
===================

接上上一篇文章，这篇文章来分析编码器

``info = bencoding.Encoder(self.meta_info[b'info']).encode()`` 从 ``Torrent`` 类分\
析到这个步骤，这个步骤主要是对解码后的元数据中的 ``info`` 字段进行编码方便后面对编码的信\
息计算 Hash 值。编码器的源码如下：

.. code-block:: python

    class Encoder:
        """
        Encodes a python object to a bencoded sequence of bytes.

        Supported python types is:
            - str
            - int
            - list
            - dict
            - bytes

        Any other type will simply be ignored.
        """
        def __init__(self, data):
            self._data = data

        def encode(self) -> bytes:
            """
            Encode a python object to a bencoded binary string

            :return The bencoded binary data
            """
            return self.encode_next(self._data)

        def encode_next(self, data):
            ...

        def _encode_int(self, value):
            return str.encode('i' + str(value) + 'e')

        def _encode_string(self, value: str):
            res = str(len(value)) + ':' + value
            return str.encode(res)

        def _encode_bytes(self, value: str):
            ...
            return result

        def _encode_list(self, data):
            ...
            return result

        def _encode_dict(self, data: dict) -> bytes:
            ...
            return result

编码器首先有个初始化函数，用 ``self._data`` 存储传入的数据，然后调用 ``decode`` 函数\
进行解码，而在 ``decode`` 函数内部，则调用了 ``encode_next`` 函数，其参数是 \
``self._data`` ，看一下 ``encode_next`` 函数：

.. code-block:: python

    def encode_next(self, data):
        if type(data) == str:
            return self._encode_string(data)
        elif type(data) == int:
            return self._encode_int(data)
        elif type(data) == list:
            return self._encode_list(data)
        elif type(data) == dict or type(data) == OrderedDict:
            return self._encode_dict(data)
        elif type(data) == bytes:
            return self._encode_bytes(data)
        else:
            return None

其过程就比较简单了，首先判断传入的数据是何种类型，并以此执行相应的编码函数。

字符串编码
==========================

首先以字符串为例，假设 ``self._data`` 为 ``'Middle Earth'`` ，数据是从单元测试中拿的。\
判断出类型是 ``str`` 后，会调用 ``_encode_string`` 函数对其进行编码，其代码如下：

.. code-block:: python

    def _encode_string(self, value: str):
        res = str(len(value)) + ':' + value
        return str.encode(res)

先获取字符串的长度，然后拼接成 ``长度:字符串`` 类型，然后对拼接后的字符串进行 ``encode`` \
编码成字节型。最终结果如下：

::

    'Middle Earth' => b'12:Middle Earth'

数字编码
==========================

假设 ``self._data`` 为 ``123`` ，判断出类型是 ``int`` 后，会调用 ``_encode_int`` 函数。\
其代码如下：

.. code-block:: python

    def _encode_int(self, value):
        return str.encode('i' + str(value) + 'e')

数字编码的时候是以 ``i`` 开头， 以 ``e`` 结尾，中间是原来的数字，拼接完成后对拼接后的字\
符串进行编码为字节码操作，并返回字节码。

::

    123 => i123e

列表编码
==========================

假设 ``self._data`` 为 ``['spam', 'eggs', 123]`` ，判断出类型是 ``list`` 后，调用 \
``_encode_list`` 函数，其代码如下：

.. code-block:: python 

    def _encode_list(self, data):
        result = bytearray('l', 'utf-8')
        result += b''.join([self.encode_next(item) for item in data])
        result += b'e'
        return result

1. 首先创建一个以 ``l`` 开头，以 ``utf-8`` 编码的字节序列，需要注意的是 ``bytearray`` \
   是可变的字节序列，何以再后面添加字符，而 ``bytes`` 是不可变字节序列。

2. 对 list 中的每个元素进行编码，同上面的字符串和数字编码，编码完毕后结果是一个 list 。\
   使用 ``join`` 函数拼接成一个字符串。

3. 随后再末尾加上 ``e`` 并返回编码结果。

::

    ['spam', 'eggs', 123] => b'l4:spam4:eggsi123ee'

字典编码
=====================

假设 ``self._data`` 为 ``{'cow': 'moo', 'spam': 'eggs'}`` ，判断出类型是 ``dict`` \
或是 ``OrderedDict`` 后，调用 ``_encode_dict`` 函数，其代码如下：

.. code-block:: python

    def _encode_dict(self, data: dict) -> bytes:
        result = bytearray('d', 'utf-8')
        for k, v in data.items():
            key = self.encode_next(k)
            value = self.encode_next(v)
            if key and value:
                result += key
                result += value
            else:
                raise RuntimeError('Bad dict')
        result += b'e'
        return result

其处理过程如下：

1. 首先创建一个以 ``d`` 开头，以 ``utf-8`` 编码的字节序列

2. 对字典中的数据循环迭代，分别对 key 和 value 进行相应的编码（数字或字符串或列表编码）

3. 当 key 和 value 编码后的结果不为空时，先将 key 的编码的结果加入到字节序列中，然后\
   将 value 的编码结果加入到字节序列；为空时就会抛出异常，直到整个字典迭代完成

4. 最后在字节序列的随后添加字符 ``e`` 并返回编码结果。

::

    {'cow': 'moo', 'spam': 'eggs'} => b'd3:cow3:moo4:spam4:eggse'

数据为空时的编码
==================

代码中显示，为空时直接返回 None 。

编码器分析完毕。接下来分析下面的内容
