一个简单的 BitTorrent "bencode" 解码器
---------------------------------------------

原文来自于 这里_

.. _这里: https://effbot.org/zone/bencode.htm

这是一个 BitTorrent Torrent 编码格式 `bencode`_ 的简单解码器。\
它使用了来自于 `Simple Iterator-based Parsing`_ 基于迭代器的方法，从而产生了可读且高效的代码。

.. _`bencode`: https://www.bittorrent.org/beps/bep_0003.html
.. _`Simple Iterator-based Parsing`: https://effbot.org/zone/simple-iterator-parser.htm

.. code-block:: python

    import re


    def tokenize(text, match=re.compile("([idel])|(\d+):|(-?\d+)").match):
        i = 0
        while i < len(text):
            m = match(text, i)
            s = m.group(m.lastindex)
            i = m.end()
            if m.lastindex == 2:
                yield "s"
                yield text[i:i+int(s)]
                i = i + int(s)
            else:
                yield s


    def decode_item(next, token):
        if token == "i":
            # integer: "i" value "e"
            data = int(next())
            if next() != "e":
                raise ValueError
        elif token == "s":
            # string: "s" value (virtual tokens)
            data = next()
        elif token == "l" or token == "d":
            # container: "l" (or "d") values "e"
            data = []
            tok = next()
            while tok != "e":
                data.append(decode_item(next, tok))
                tok = next()
            if token == "d":
                data = dict(zip(data[0::2], data[1::2]))
        else:
            raise ValueError
        return data


    def decode(text):
        try:
            src = tokenize(text)
            data = decode_item(src.next, src.next())
            for token in src: # look for more tokens
                raise SyntaxError("trailing junk")
        except (AttributeError, ValueError, StopIteration):
            raise SyntaxError("syntax error")
        return data


    if __name__ == '__main__':
        data = open("test.torrent", "rb").read()

        torrent = decode(data)

        for file in torrent["info"]["files"]:
            print "%r - %d bytes" % ("/".join(file["path"]), file["length"])
    
大多数编码对象由类型代码，对象值和结束代码组成；而例外是编码字符串，\
其前缀为长度。为了简化解析器，**tokenize** 函数将字符串作为两个“虚\
拟”令牌返回；一个类型代码，后跟着字符串值。一些例子：

.. code-block:: python

    >>> list(tokenize("4:spam"))
    ['s', 'spam']
    >>> list(tokenize("i3e")) # int
    ['i', '3', 'e']
    >>> list(tokenize("i-3e"))
    ['i', '-3', 'e']
    >>> list(tokenize("l4:spam4:eggse")) # list
    ['l', 's', 'spam', 's', 'eggs', 'e']
    >>> list(tokenize("d3:cow3:moo4:spam4:eggse")) # dict
    ['d', 's', 'cow', 's', 'moo', 's', 'spam', 's', 'eggs', 'e']

请注意，越简单的格式通常可以使用 **finditer ** 方法进行拆分（或甚至于使用 **findall** ）。

**encode_item** 函数使用类型信息将数据转换为合适的 Python 对象。 它调用自\
身来处理嵌套的容器类型。

使用方法
==========================

这是一个代码清单，列出了种子文件中包含的所有文件：

.. code-block:: python

    data = open("test.torrent", "rb").read()

    torrent = decode(data)

    for file in torrent["info"]["files"]:
        print "%r - %d bytes" % ("/".join(file["path"]), file["length"])

运行此命令将产生类似以下内容：

..code-block:: 

    'Little Earthquakes/01 Crucify.m4a' - 4845721 bytes
    'Little Earthquakes/02 Girl.m4a' - 4012517 bytes
    'Little Earthquakes/03 Silent All These Years.m4a' - 4076790 bytes
    'Little Earthquakes/04 Precious Things.m4a' - 4328948 bytes
    'Little Earthquakes/05 Winter.m4a' - 5538530 bytes
    'Little Earthquakes/06 Happy Phantom.m4a' - 3204091 bytes
    'Little Earthquakes/07 China.m4a' - 4859246 bytes
    'Little Earthquakes/08 Leather.m4a' - 3125716 bytes
    'Little Earthquakes/09 Mother.m4a' - 6785591 bytes
    'Little Earthquakes/10 Tear In Your Hand.m4a' - 4515482 bytes
    'Little Earthquakes/11 Me And A Gun.m4a' - 3649914 bytes
    'Little Earthquakes/12 Little Earthquakes.m4a' - 6663794 bytes
