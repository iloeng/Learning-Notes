#include <stdio.h>
#include <math.h>

typedef struct {
    char* key;
    char* value;
} ht_item;

// hash_table.h
typedef struct {
    int size;
    int count;
    int base_size;
    ht_item** items;
} ht_hash_table;

ht_hash_table* ht_new();

static void ht_del_item(ht_item* i);

void ht_del_hash_table(ht_hash_table* ht);

// hash_table.h
void ht_insert(ht_hash_table* ht, const char* key, const char* value);

char* ht_search(ht_hash_table* ht, const char* key);

void ht_delete(ht_hash_table* h, const char* key);

