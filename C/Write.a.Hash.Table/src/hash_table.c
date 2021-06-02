/*
 *
 *-----------------------------------------------------------------------------
 * @Name：    hash_table.c
 * @Desc:     
 * @Author:   liangz.org@gmail.com
 * @Create:   2021.06.01   22:40
 *-----------------------------------------------------------------------------
 * @Change:   2021.06.01
 *-----------------------------------------------------------------------------
*/

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

// hash_table.c
ht_hash_table* ht_new() {
    ht_hash_table* ht = malloc(sizeof(ht_hash_table));

    ht->size = 53;
    ht->count = 0;
    ht->items = calloc((size_t)ht->size, sizeof(ht_item*));
    return ht;
}

