*******************************************************************************
Part 02 - 哈希表结构
*******************************************************************************

.. contents:: 目录

我们的键值对 (条目) 将每个都存储在一个结构中： 

.. code-block:: c 

    // hash_table.h
    typedef struct {
        char* key;
        char* value;
    } ht_item;

我们的哈希表存储了一个指向条目的指针数组， 以及一些关于它的大小和它是否装满的细节：

.. code-block:: C 

    // hash_table.h
    typedef struct {
        int size;
        int count;
        ht_item** items;
    } ht_hash_table;

2.1 - 初始化和删除
===============================================================================

我们需要为 ``ht_items`` 定义初始化函数。 这个函数分配了一个与 ``ht_item`` 大小相当\
的内存块， 并在新的内存块中保存了字符串 ``k`` 和 ``v`` 的副本。 这个函数被标记为静态\
的， 因为它只会被哈希表内部的代码调用。 

.. code-block:: C 
    :name: hash_table.c

    // hash_table.c
    #include <stdlib.h>
    #include <string.h>

    #include "hash_table.h"

    static ht_item* ht_new_item(const char* k, const char* v) {
        ht_item* i = malloc(sizeof(ht_item));
        i->key = strdup(k);
        i->value = strdup(v);
        return i;
    }

``ht_new`` 初始化一个新的哈希表。 ``size`` 定义了我们可以存储多少个条目。 目前固定\
为 53。 我们将在有关 `调整大小`_ 的部分对此进行扩展。 我们使用 ``calloc`` 初始化项\
目数组， 它用 ``NULL`` 字节填充分配的内存。 数组中的 ``NULL`` 条目表示该存储桶为空。 

.. _调整大小: waiting

.. code-block:: C 

    // hash_table.c
    ht_hash_table* ht_new() {
        ht_hash_table* ht = malloc(sizeof(ht_hash_table));

        ht->size = 53;
        ht->count = 0;
        ht->items = calloc((size_t)ht->size, sizeof(ht_item*));
        return ht;
    }

我们还需要有删除 ``ht_items`` 和 ``ht_hash_tables`` 的函数， 它将释放我们分配的内\
存， 所以我们不会导致 `内存泄漏`_。

.. _`内存泄漏`: https://en.wikipedia.org/wiki/Memory_leak

.. code-block:: C 

    // hash_table.c
    static void ht_del_item(ht_item* i) {
        free(i->key);
        free(i->value);
        free(i);
    }


    void ht_del_hash_table(ht_hash_table* ht) {
        for (int i = 0; i < ht->size; i++) {
            ht_item* item = ht->items[i];
            if (item != NULL) {
                ht_del_item(item);
            }
        }
        free(ht->items);
        free(ht);
    }

我们已经编写了定义哈希表的代码， 并让我们创建和销毁一个。 虽然目前它没有做太多事情， \
但我们仍然可以尝试一下。 

.. code-block:: C 

    // main.c
    #include "hash_table.h"


    int main() {
        ht_hash_table* ht = ht_new();
        printf("%d, %d, %s, %s", ht->count, ht->size, ht->items[0], ht->items[1]);
        ht_del_hash_table(ht);
    }

下一节： 哈希函数
