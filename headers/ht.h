#if !defined(HT_H_)
#define HT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "linked_list.h"
#include "utility.h"

#define MAX_CHARACTERS 900000
#define MAX_PATHNAME 300

typedef struct _frecord
{
    /* metadata */
    char pathname[MAX_PATHNAME];
    size_t size;
    time_t last_edit;
    pthread_mutex_t lock;
    int is_new;
    int last_op;
    int is_open;
    pid_t last_client;
    /* data */
    char *content;
} FRecord;

typedef struct ht
{
    LList *lists;
    int size;
} HashTable;

void ht_create(HashTable *storage, int size);

void ht_free(HashTable storage);

void *ht_get(HashTable storage, const char *key);

int ht_insert(HashTable *storage, void *data, const char *key);

unsigned int sdbm_hash(const char *str, int size);

int ht_delete(HashTable *storage, const char *key);

int ht_exists(HashTable storage, const char *key);

void print_record_node(Node *to_print);

void ht_print(HashTable storage);

void clean_record_node(Node *to_clean);

#endif