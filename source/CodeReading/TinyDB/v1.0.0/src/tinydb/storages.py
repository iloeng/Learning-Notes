"""
Contains the :class:`base class <tinydb.storages.Storage>` for storages and
two implementations.
"""

from abc import ABCMeta, abstractmethod

import os

try:
    import ujson as json
except ImportError:
    import json


def touch(fname, times=None):
    """
    1. 打开 fname 文件， 然后设置该文件的访问日期和修改日期
    """
    with open(fname, 'a'):
        os.utime(fname, times)


class Storage(object):
    """
    The abstract base class for all Storages.

    A Storage (de)serializes the current state of the database and stores it in
    some place (memory, file on disk, ...).
    """
    """
    1. 存储方式的基类， 声明了存储方式的 read 和 write
    """
    __metaclass__ = ABCMeta

    @abstractmethod
    def write(self, data):
        """
        Write the current state of the database to the storage.

        Any kind of serialization should go here.

        :param data: The current state of the database.
        :type data: dict
        """
        raise NotImplementedError('To be overriden!')

    @abstractmethod
    def read(self):
        """
        Read the last stored state.

        Any kind of deserialization should go here.

        :rtype: dict
        """
        raise NotImplementedError('To be overriden!')


class JSONStorage(Storage):
    """
    Store the data in a JSON file.
    """

    def __init__(self, path):
        """
        Create a new instance.

        Also creates the storage file, if it doesn't exist.

        :param path: Where to store the JSON data.
        :type path: str
        """
        """
        1. JSON 形式存储。
        2. 初始化时， 先执行 touch 方法， 如果 path 指定的文件不存在， 将会新建
        3. 然后将 path 赋值给 self.path， 整个类里面都可使用
        4. 然后用 self._handle 存储文件句柄
        """
        super(JSONStorage, self).__init__()
        touch(path)  # Create file if not exists
        self.path = path
        self._handle = open(path, 'r+')

    def __del__(self):
        """
        1. 删除对象就意味着关闭文件流
        """
        self._handle.close()

    def write(self, data):
        """
        1. 文件从开头读取 seek(0)
        2. 然后使用 json.dump 将 data 数据转换为 json 并存储到文件流
        3. 然后使用 flush 方法刷新缓冲区（同时清空缓冲区）
        """
        self._handle.seek(0)
        json.dump(data, self._handle)
        self._handle.flush()

    def read(self):
        """
        1. 文件从开头读取 seek(0)
        2. 将文件流加载为 json
        """
        self._handle.seek(0)
        return json.load(self._handle)


class MemoryStorage(Storage):
    """
    Store the data as JSON in memory.
    """

    def __init__(self):
        """
        Create a new instance.
        """
        """
        1. 将数据记录在内存中 self.memory 初始化为 None
        """
        super(MemoryStorage, self).__init__()
        self.memory = None

    def write(self, data):
        """
        1. 将 data 数据赋值给 self.memory
        """
        self.memory = data

    def read(self):
        """
        1. 读取内存中的数据
        2. 如果数据为空， 抛出 ValueError 异常
        3. 否则返回 self.memory
        """
        if self.memory is None:
            raise ValueError
        return self.memory
