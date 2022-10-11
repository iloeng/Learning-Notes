*******************************************************************************
Part 05 - 哈希表方法
*******************************************************************************

.. contents:: 目录

我们的哈希函数将实现以下 API： 

.. code-block:: c

    // hash_table.h
    void ht_insert(ht_hash_table* ht, const char* key, const char* value);
    char* ht_search(ht_hash_table* ht, const char* key);
    void ht_delete(ht_hash_table* h, const char* key);

5.1 - 插入
===============================================================================

为了插入一个新的键值对， 我们遍历索引直到找到一个空桶。 然后我们将项目插入到该存储桶\
中并增加哈希表的 ``count`` 属性， 以指示已添加新项目。 当我们在下一节中查看 \
`resizing`_ 大小时， 哈希表的 ``count`` 属性将变得有用。 

.. _resizing:

.. code-block:: C 

    // hash_table.c
    void ht_insert(ht_hash_table* ht, const char* key, const char* value) {
        ht_item* item = ht_new_item(key, value);
        int index = ht_get_hash(item->key, ht->size, 0);
        ht_item* cur_item = ht->items[index];
        int i = 1;
        while (cur_item != NULL) {
            index = ht_get_hash(item->key, ht->size, i);
            cur_item = ht->items[index];
            i++;
        } 
        ht->items[index] = item;
        ht->count++;
    }

5.2 - 搜索
===============================================================================

搜索类似于插入， 但在 ``while`` 循环的每次迭代中， 我们检查项目的键是否与我们正在搜\
索的键匹配。 如果是， 我们返回项目的值。 如果 while 循环命中 ``NULL`` 存储桶， 我们\
将返回 ``NULL``， 以指示未找到任何值。 

.. code-block:: C 

    // hash_table.c
    char* ht_search(ht_hash_table* ht, const char* key) {
        int index = ht_get_hash(key, ht->size, 0);
        ht_item* item = ht->items[index];
        int i = 1;
        while (item != NULL) {
            if (strcmp(item->key, key) == 0) {
                return item->value;
            }
            index = ht_get_hash(key, ht->size, i);
            item = ht->items[index];
            i++;
        } 
        return NULL;
    }

5.3 - 删除
===============================================================================

从一个开放寻址的哈希表中删除比插入或搜索更复杂。 我们想删除的条目可能是一个碰撞链的一\
部分。 从表中删除它将打破这个链， 并使寻找链尾的条目成为不可能。 为了解决这个问题， \
我们不会删除这个条目， 而是简单地把它标记为已删除。 

我们用一个指向全局标记项的指针来标记一个项目为已删除， 这个全局标记项表示一个桶中包含\
一个已删除的项目。 

.. code-block:: C 

    // hash_table.c
    static ht_item HT_DELETED_ITEM = {NULL, NULL};


    void ht_delete(ht_hash_table* ht, const char* key) {
        int index = ht_get_hash(key, ht->size, 0);
        ht_item* item = ht->items[index];
        int i = 1;
        while (item != NULL) {
           if (item != &HT_DELETED_ITEM) {
                if (strcmp(item->key, key) == 0) {
                    ht_del_item(item);
                    ht->items[index] = &HT_DELETED_ITEM;
                }
            }
            index = ht_get_hash(key, ht->size, i);
            item = ht->items[index];
            i++;
        } 
        ht->count--;
    }

删除后， 我们递减哈希表的 ``count`` 属性。 

我们还需要修改 ``ht_insert`` 和 ``ht_search`` 函数以考虑已删除的节点。 

搜索时， 我们忽略并 "跳过" 已删除的节点。 插入的时候， 如果我们命中一个被删除的节点\
， 就可以将新节点插入到被删除的槽中。 

.. code-block:: C 

    // hash_table.c
    void ht_insert(ht_hash_table* ht, const char* key, const char* value) {
        // ...
        while (cur_item != NULL && cur_item != &HT_DELETED_ITEM) {
            // ...
        }
        // ...
    }


    char* ht_search(ht_hash_table* ht, const char* key) {
        // ...
        while (item != NULL) {
           if (item != &HT_DELETED_ITEM) { 
                if (strcmp(item->key, key) == 0) {
                    return item->value;
                }
            }
            // ...
        }
        // ...
    }

5.4 - 更新
===============================================================================

我们的哈希表目前不支持更新键的值。 如果我们插入两个键相同的条， 键将发生冲突， 第二项\
将插入下一个可用的存储桶中。 搜索 key 的时候， 总是会找到原来的 key， 我们无法访问到\
第二项。 

我们可以通过修改 ``ht_insert`` 来解决这个问题， 以删除前一个条目并在其位置插入新条目。

.. code-block:: C 

    // hash_table.c
    void ht_insert(ht_hash_table* ht, const char* key, const char* value) {
        // ...
        while (cur_item != NULL) {
           if (cur_item != &HT_DELETED_ITEM) {
                if (strcmp(cur_item->key, key) == 0) {
                    ht_del_item(cur_item);
                    ht->items[index] = item;
                    return;
                }
            }
            // ...
        } 
        // ...
    }

下一节: 调整表格大小

