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



