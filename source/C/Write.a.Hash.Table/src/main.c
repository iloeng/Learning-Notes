/*
 *
 *-----------------------------------------------------------------------------
 * @Name：    main.c
 * @Desc:     
 * @Author:   liangz.org@gmail.com
 * @Create:   2021.06.03   23:03
 *-----------------------------------------------------------------------------
 * @Change:   2021.06.03
 *-----------------------------------------------------------------------------
*/

// main.c
#include "hash_table.h"


int main() {
    ht_hash_table* ht = ht_new();
    printf("%d, %d, %s, %s", ht->count, ht->size, ht->items[0], ht->items[1]);
    ht_del_hash_table(ht);
}


