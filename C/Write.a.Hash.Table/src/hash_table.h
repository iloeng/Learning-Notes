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
    ht_item** items;
} ht_hash_table;

ht_hash_table* ht_new();

static void ht_del_item(ht_item* i);

void ht_del_hash_table(ht_hash_table* ht);


