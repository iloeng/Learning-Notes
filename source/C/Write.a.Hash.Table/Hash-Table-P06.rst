*******************************************************************************
Part 06 - 调整表格大小
*******************************************************************************

.. contents:: 目录

目前我们的哈希表有固定数量的桶。 随着更多条目的插入， 表格开始填满。 这是有问题的， \
原因有两个： 

1. 哈希表的性能随着高冲突率而降低
2. 我们的哈希表只能存储固定数量的条目。 如果我们尝试存储更多， 插入功能将失败。 

为了缓解这种情况， 我们可以在条目数组太满时增加它的大小。 我们将存储在哈希表中的条目\
数存储在表的 ``count`` 属性中。 在每次插入和删除时， 我们计算表的 "负载"， 或已填充\
的桶与总桶的比率。 如果它高于或低于某些值， 我们将向上或向下调整桶的大小。 

我们将调整大小： 

- 增加， 如果负载 > 0.7
- 减少， 如果负载 < 0.1

为了调整大小， 我们创建一个新的哈希表， 大约是当前哈希表的一半或两倍， 并将所有未删除\
的条目插入其中。 

我们的新数组大小应该是一个质数， 大约是当前大小的两倍或一半。 找到新的数组大小并非易\
事。 为此我们存储我们希望数组的基本大小， 然后将实际大小定义为第一个大于基本大小的素\
数。 要调整大小， 我们将基本大小加倍， 并找到第一个较大的素数， 而要调整大小， 我们将\
大小减半并找到下一个较大的素数。 

我们的基本大小从 50 开始。 而不是存储

我们使用暴力方法通过检查每个连续数字是否为素数来找到下一个素数。 虽然强制执行任何事情\
听起来令人震惊， 但我们实际需要检查的值的数量很少， 并且重新散列表中的每个条目所花费\
的时间超过了它所花费的时间。 

首先， 让我们定义一个查找下一个素数的函数。 我们将在两个新文件中执行此操作， \
``prime.h`` 和 ``prime.c``。 

.. code-block:: c

    // prime.h
    int is_prime(const int x);
    int next_prime(int x);

.. code-block:: c

    // prime.c

    #include <math.h>

    #include "prime.h"


    /*
    * Return whether x is prime or not
    *
    * Returns:
    *   1  - prime
    *   0  - not prime
    *   -1 - undefined (i.e. x < 2)
    */
    int is_prime(const int x) 
    {
        if (x < 2) 
        { 
            return -1; 
        }
        if (x < 4) 
        { 
            return 1; 
        }
        if ((x % 2) == 0) 
        { 
            return 0; 
        }
        for (int i = 3; i <= floor(sqrt((double) x)); i += 2) 
        {
            if ((x % i) == 0) 
            {
                return 0;
            }
        }
        return 1;
    }


    /*
    * Return the next prime after x, or x if x is prime
    */
    int next_prime(int x) 
    {
        while (is_prime(x) != 1) 
        {
            x++;
        }
        return x;
    }

接下来， 我们需要更新我们的 ``ht_new`` 函数以支持创建特定大小的哈希表。 为此， 我们\
将创建一个新函数 ``ht_new_sized``。 我们将 ``ht_new`` 更改为使用默认起始大小调用 \
``ht_new_size``。 

.. code-block:: C 

    // hash_table.c
    static ht_hash_table* ht_new_sized(const int base_size)
    {
        ht_hash_table* ht = xmalloc(sizeof(ht_hash_table));
        ht->base_size = base_size;

        ht->size = next_prime(ht->base_size);

        ht->count = 0;
        ht->items = xcalloc((size_t)ht->size, sizeof(ht_item*));
        return ht;
    }

    // hash_table.c
    ht_hash_table* ht_new()
    {
        return ht_new_sized(HT_INITIAL_BASE_SIZE);
    }

现在我们有了编写调整大小函数所需的所有部分。 

在我们的 ``resize`` 函数中， 我们检查以确保我们没有试图将哈希表的大小减小到其最小值\
以下。 然后我们初始化一个具有所需大小的新哈希表。 所有非 ``NULL`` 或已删除的条目都被\
插入到新的哈希表中。 然后我们在删除旧哈希表之前交换新旧哈希表的属性。 

.. code-block:: C 

    // hash_table.c
    static void ht_resize(ht_hash_table* ht, const int base_size) 
    {
        if (base_size < HT_INITIAL_BASE_SIZE)
        {
            return;
        }
        ht_hash_table* new_ht = ht_new_sized(base_size);
        for (int i = 0; i < ht->size; i++)
        {
            ht_item* item = ht->items[i];
            if (item != NULL && item != &HT_DELETED_ITEM)
            {
                ht_insert(new_ht, item->key, item->value);
            }
        }

        ht->base_size = new_ht->base_size;
        ht->count = new_ht->count;

        // To delete new_ht, we give it ht's size and items
        const int tmp_size = ht->size;
        ht->size = new_ht->size;
        new_ht->size = tmp_size;

        ht_item** tmp_items = ht->items;
        ht->items = new_ht->items;
        new_ht->items = tmp_items;

        ht_del_hash_table(new_ht);
    }

为了简化调整大小， 我们定义了两个用于调整大小的小函数。 

.. code-block:: C 

    // hash_table.c
    static void ht_resize_up(ht_hash_table* ht)
    {
        const int new_size = ht->base_size * 2;
        ht_resize(ht, new_size);
    }


    static void ht_resize_down(ht_hash_table* ht)
    {
        const int new_size = ht->base_size / 2;
        ht_resize(ht, new_size);
    }

为了执行调整大小， 我们在插入和删除时检查哈希表上的负载。 如果它高于或低于 0.7 和 \
0.1 的预定义限制， 我们将分别向上或向下调整大小。 

为了避免进行浮点数学运算， 我们将计数乘以 100， 然后检查它是否高于或低于 70 或 10。 

.. code-block:: C 

    // hash_table.c
    void ht_insert(ht_hash_table* ht, const char* key, const char* value) {
        const int load = ht->count * 100 / ht->size;
        if (load > 70) {
            ht_resize_up(ht);
        }
        // ...
    }


    void ht_delete(ht_hash_table* ht, const char* key) {
        const int load = ht->count * 100 / ht->size;
        if (load < 10) {
            ht_resize_down(ht);
        }
        // ...
    }

下一节: 附录： 替代碰撞处理
