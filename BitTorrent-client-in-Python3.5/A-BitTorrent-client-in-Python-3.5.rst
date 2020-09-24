基于 Python 3.5 的 BitTorrent 客户端
-------------------------------------

- 原文标题: A BitTorrent client in Python 3.5
- 原文发布于: 周三, 2016-08-24
- 原文来自于 https://markuseliasson.se/article/bittorrent-in-python/
- 想法来自于 build-your-own-x_

.. _build-your-own-x: https://github.com/danistefanovic/build-your-own-x

Python 3.5 开始支持异步 IO, 这似乎是实现 BitTorrent 客户端的完美\
选择。本文将指导您了解 BitTorrent 协议的细节，同时展示如何使用它实\
现一个小的客户端。

当 Python 3.5 与新的模块 ``asyncio`` 一起发布时，我很好奇地尝试了\
一下。最近，我决定使用 ``asyncio`` 实现一个简单的 BitTorrent 客户\
端--我一直对点对点(P2P, peer-to-peer)协议感兴趣，它似乎是一个完美\
的选择。

这个项目名为 Pieces, 
