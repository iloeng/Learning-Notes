"""
TinyDB is a tiny, document oriented database optimized for your happiness :)

TinyDB stores differrent types of python data types using a configurable
backend. It has support for handy querying and tables.

.. codeauthor:: Markus Siemens <markus@m-siemens.de>

Usage example:

    >>> db = TinyDB(storage=MemoryStorage)
    >>> db.insert({'data': 5})  # Insert into '_default' table
    >>> db.search(where('data') == 5)
    [{'data': 5, '_id': 1}]
    >>> # Now let's create a new table
    >>> tbl = db.table('our_table')
    >>> for i in range(10):
    ...     tbl.insert({'data': i})
    ...
    >>> len(tbl.search(where('data') < 5))
    5
"""

from tinydb.storages import Storage, JSONStorage
from tinydb.queries import query, where

__all__ = ('TinyDB', 'where')


class TinyDB(object):
    """
    The main class of TinyDB.

    Gives access to the database, provides methods to insert/search/remove
    and getting tables.
    """
    """
    1. _table_cache 数据表缓存
    """
    _table_cache = {}

    def __init__(self, *args, **kwargs):
        """
        Create a new instance of TinyDB.

        All arguments and keyword arguments will be passed to the underlying
        storage class (default: :class:`~tinydb.storages.JSONStorage`).
        """
        """
        1. 使用 TinyDB 时， 首先进行初始化操作。
        2. 初始化工作会从参数字典了拿 storage 的值， 如果没有传入这个参数， 则使用 
           JSONStorage 形式存储数据
        3. 初始化工作指定了数据的存储方式以及数据表， 数据表默认为 _default
        """
        storage = kwargs.pop('storage', JSONStorage)
        #: :type: Storage
        self._storage = storage(*args, **kwargs)
        self._table = self.table('_default')

    def table(self, name='_default'):
        """
        Get access to a specific table.

        Creates a new table, if it hasn't been created before, otherwise it
        returns the cached :class:`~tinydb.Table` object.

        :param name: The name of the table.
        :type name: str
        """
        """
        1. 首先判断 _table_cache 数据表缓存字典中是否含有名称为 name 的数据表， 如果
           有， 就返回缓存字典中的 table 对象
        2. 没有的话， 就会执行下面的步骤。 对当前 TinyDB 对象创建 Table 对象， 然后将
           其加入到数据表缓存字典当中
        """
        if name in self._table_cache:
            return self._table_cache[name]

        table = Table(name, self)
        self._table_cache[name] = table
        return table

    def purge_tables(self):
        """
        Purge all tables from the database. **CANT BE REVERSED!**
        """
        """
        1. 清除数据表操作， 首先会执行 _write 函数， 由于没有指定 table 名称， 因此
           表示将 '{}' 表示的空数据覆盖到 TinyDB 里面， 达到清除数据的目的
        2. 然后在清除数据表缓存字典中的数据
        """
        self._write({})
        self._table_cache.clear()

    def _read(self, table=None):
        """
        Reading access to the backend.

        :param table: The table, we want to read, or None to read the 'all
        tables' dict.
        :type table: str or None
        :returns: all values
        :rtype: dict, list
        """
        """
        1. 读取当前 TinyDB 对象的数据
        2. 如果没有指定 table， 会读取当前 TinyDB 对象的所有 table 的值
        3. 如果指定了 table， 则会读取对应的 table 
        """
        if not table:
            try:
                return self._storage.read()
            except ValueError:
                return {}

        try:
            return self._read()[table]
        except (KeyError, TypeError):
            return []

    def _write(self, values, table=None):
        """
        Writing access to the backend.

        :param table: The table, we want to write, or None to write the 'all
        tables' dict.
        :type table: str or None
        :param values: the new values to write
        :type values: list, dict
        """
        """
        1. TinyDB 对象的写操作， 至少需要一个参数 value， 如果 table 没有指定相应的值，
           会将 value 写入所有的 table 中
        2. 如果指定了 table， 首先会读取当前 TinyDB 的数据 current_data ， 然后将 
           values 赋值给指定 table， 最后将 current_data 覆盖到 TinyDB 对象里
        """
        if not table:
            self._storage.write(values)
        else:
            current_data = self._read()
            current_data[table] = values

            self._write(current_data)

    def __len__(self):
        """
        Get the total number of elements in the DB.

        >>> len(db)
        0
        """
        """
        1. 对 TinyDB 对象实现了 len() 函数， 用于获取该 DB 对象共有多少元素
        """
        return len(self._table)

    def __contains__(self, item):
        """
        A shorthand for ``query(...) == ... in db.table()``. Intendet to be
        used in if-clauses (avoiding ``if len(db.serach(...)):``)

        >>> if where('field') == 'value' in db:
        ...     print True
        """
        """
        1. 对 db 实现了 in 操作符
        """
        return item in self._table

    def __getattr__(self, name):
        """
        Forward all unknown attribute calls to the underlying standard table.
        """
        """
        1. 实现了 getattr 方法
        """
        return getattr(self._table, name)


class Table(object):
    """
    Represents a single TinyDB Table.
    """

    def __init__(self, name, db):
        """
        Get access to a table.

        :param name: The name of the table.
        :type name: str
        :param db: The parent database.
        :type db: TinyDB
        """
        """
        1. Table 类的初始化需要两个参数， name 是数据表名称， db 是 DB 对象
        2. 同时将 self.name 和 self._db 暴露为类级别， 方便使用， 另增加了查询缓存字
           典 self._queries_cache， 同样是类级别访问
        3. 尝试读取数据表最新的 id， 如果无法读取到， 将 self._last_id 设为 0， 同样
           是类级别访问
        """
        self.name = name
        self._db = db
        self._queries_cache = {}

        try:
            self._last_id = self._read().pop()['_id']
        except IndexError:
            self._last_id = 0

    def _read(self):
        """
        Reading access to the DB.

        :returns: all values
        :rtype: list
        """
        """
        1. Table._read 方法通过调用 TinyDB._read 方法， 给定当前的数据表名称作为参数
           来读取当前数据表的内容           
        """
        return self._db._read(self.name)

    def _write(self, values):
        """
        Writing access to the DB.

        :param values: the new values to write
        :type values: list
        """

        self._clear_query_cache()
        self._db._write(values, self.name)

    def __len__(self):
        """
        Get the total number of elements in the table.
        """
        return len(self.all())

    def __contains__(self, condition):
        """
        Equals to ``bool(table.search(condition)))``.
        """
        return bool(self.search(condition))

    def all(self):
        """
        Get all elements stored in the table.

        Note: all elements will have an `_id` key.

        :returns: a list with all elements.
        :rtype: list
        """

        return self._read()

    def insert(self, element):
        """
        Insert a new element into the table.

        element has to be a dict, not containing the key 'id'.
        """

        next_id = self._last_id
        self._last_id += 1

        element['_id'] = next_id

        data = self._read()
        data.append(element)

        self._write(data)

    def remove(self, cond):
        """
        Remove the element matching the condition.

        :param cond: the condition to check against
        :type cond: query, int, list
        """

        to_remove = self.search(cond)
        self._write([e for e in self.all() if e not in to_remove])

    def purge(self):
        """
        Purge the table by removing all elements.
        """
        self._write([])

    def search(self, cond):
        """
        Search for all elements matching a 'where' cond.

        Note: all elements will have an `_id` key.

        :param cond: the condition to check against
        :type cond: query

        :returns: list of matching elements
        :rtype: list
        """

        if cond in self._queries_cache:
            return self._queries_cache[where]
        else:
            elems = [e for e in self.all() if cond(e)]
            self._queries_cache[where] = elems

            return elems

    def get(self, cond):
        """
        Search for exactly one element matching a 'where' condition.

        Note: all elements will have an `_id` key.

        :param cond: the condition to check against
        :type cond: query

        :returns: the element or None
        :rtype: dict or None
        """

        for el in self.all():
            if cond(el):
                return el

    def _clear_query_cache(self):
        """
        Clear query cache.
        """
        """
        1. 将查询缓存清除
        """
        self._queries_cache = {}
