#if !defined(HT_H_)
#define HT_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define MAX_CHARACTERS 900000
#define MAX_PATHNAME 300

typedef struct _frecord
{
    /* metadata */
    char pathname[MAX_PATHNAME];
    size_t size;
    time_t last_edit;
    pthread_mutex_t lock;
    int is_locked;
    int is_new;
    int is_victim;
    int is_open;
    pid_t last_client; 
    /* data */
    char *content;
} FRecord;

typedef struct Ht_item Ht_item;

// Define the Hash Table Item here
struct Ht_item
{
    char *key;
    FRecord value;
};

typedef struct LinkedList LinkedList;

// Define the Linkedlist here
struct LinkedList
{
    Ht_item *item;
    LinkedList *next;
};

typedef struct HashTable HashTable;

// Define the Hash Table here
struct HashTable
{
    // Contains an array of pointers
    // to items
    Ht_item **items;
    LinkedList **overflow_buckets;
    int size;
    size_t file_size;
    size_t capacity;
    int max_files;
    int count;
};

unsigned long hash_function(char *str, int ht_dim);

LinkedList *allocate_list();

LinkedList *linkedlist_insert(LinkedList *list, Ht_item *item);

Ht_item *linkedlist_remove(LinkedList *list);

void free_linkedlist(LinkedList *list);

LinkedList **create_overflow_buckets(HashTable *table);

void free_overflow_buckets(HashTable *table);

Ht_item *create_item(char *key, FRecord value, size_t file_size);

HashTable *create_table(int size, size_t capacity, int max_files);

void free_item(Ht_item *item);

void free_table(HashTable *table);

void handle_collision(HashTable *table, unsigned long index, Ht_item *item);

void ht_insert(HashTable *table, char *key, FRecord value, size_t file_size);

FRecord *ht_search(HashTable *table, char *key);

void ht_delete(HashTable *table, char *key);

/* it returns the path of the file to be deleted using LRU politic */
int lru(HashTable *table, char *victim_pathname);

void print_table(HashTable *table);

#endif