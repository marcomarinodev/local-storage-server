#if !defined(LL_H_)
#define LL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INVALID_INDEX -1
#define NO_DATA -2
#define EMPTY_LIST -3
#define NOT_FOUND -4

#define TRUE 1
#define FALSE 0

typedef struct _node 
{
    struct _node* prev;
    struct _node* next;

    const char* key;
    void* data;
} Node;

typedef struct list
{
    Node *head;
    Node *tail;
    int length;
    void (*printer)(Node *to_print);
} LList;

Node* create_node(void* to_set, const char* key);

void clean_node(Node* to_clean, int clean_data);

void LL_init(LList *list, void (*printer)(Node *));

void LL_free(LList list, void (*cleaner)(Node *));

void *LL_remove_by_key(LList *list, const char *key);

void *LL_pop(LList *list);

void *LL_get(LList list, unsigned int index);

int LL_push(LList *list, void *data, const char *key);

void LL_print(LList to_print, char *name);

int LL_enqueue(LList *list, void *data, const char *key);

void *LL_dequeue(LList *list);

int LL_contains_key(LList list, const char *key);

void *LL_get_by_key(LList list, const char *key);

#endif