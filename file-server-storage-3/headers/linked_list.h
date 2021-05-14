// // USAGE
// int main(void)
// {

//     List requests;

//     list_initialize(&requests, print_request);

//     Request *curr_request = malloc(sizeof(Request));

//     char dir[] = "dirname";

//     curr_request->code = 'D';
//     curr_request->arguments = (char *)dir;

//     list_enqueue(&requests, (void *)curr_request, NULL);

//     print_list(requests, "requests");

//     list_clean(requests, NULL);

//     return 0;
// }


#if !defined(LINKED_LIST_H_)
#define LINKED_LIST_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRUE 1
#define FALSE 0
#define INVALID_INDEX -1
#define NO_DATA -2
#define EMPTY_LIST -3
#define NOT_FOUND -4

typedef struct node
{
    struct node *next;

    const char *key;
    void *data;
} Node;

typedef struct list
{
    Node *head;
    Node *tail;
    int length;
    void (*printer)(Node *to_print);
} LList;

/* NODES METHODS */
Node *create_node(void *to_set, const char *key);

void clean_node(Node *to_clean);

/* LIST METHODS */
void LL_init(LList *list, void (*printer)(Node *));

void LL_free(LList list, void (*cleaner)(Node *));

void *LL_remove_by_index(LList *list, unsigned int index);

void *LL_remove_by_key(LList *list, const char *key);

void *LL_get(LList list, unsigned int index);

void *LL_get_by_key(LList list, const char *key);

int LL_insert_at_index(LList *list, unsigned int index, void *data, const char *key);

void *LL_pop(LList *list);

int LL_push(LList *list, void *data, const char *key);

void LL_print(LList to_print, char *name);

int LL_enqueue(LList *list, void *data, const char *key);

void *LL_dequeue(LList *list);

int LL_contains_key(LList list, const char *key);

#endif