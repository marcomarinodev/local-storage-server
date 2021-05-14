#include "linked_list.h"

/* NODES METHODS */
Node *create_node(void *to_set, const char *key)
{
    Node *node = malloc(sizeof(Node));

    node->next = NULL;
    node->data = to_set;
    node->key = key;

    return node;
}

void clean_node(Node *to_clean)
{
    if (to_clean->data != NULL)
        free(to_clean->data);
    if (to_clean->key != NULL)
        free((void *)to_clean->key);

    free(to_clean);
}

/* LIST METHODS */
void LL_free(LList list, void (*cleaner)(Node *))
{
    Node *curr = list.head;
    Node *prev = NULL;

    while (curr != NULL)
    {
        prev = curr;
        curr = curr->next;

        if (cleaner != NULL)
            cleaner(prev);
        clean_node(prev);
    }
}

void LL_init(LList *list, void (*printer)(Node *))
{
    list->head = NULL;
    list->tail = list->head;
    list->length = 0;
    list->printer = printer;
}

int LL_enqueue(LList *list, void *data, const char *key)
{

    Node *to_add = create_node(data, key);

    if (list->tail == NULL)
    {
        list->head = to_add;
        list->tail = list->head;
    }
    else
    {

        list->tail->next = to_add;
        list->tail = list->tail->next;
    }

    list->length++;

    return 0;
}

void *LL_dequeue(LList *list)
{

    return LL_pop(list);
}

void *LL_remove_by_index(LList *list, unsigned int index)
{

    int i = 0;
    Node *curr = list->head;
    Node *prev = NULL;

    if (index >= list->length)
        return NULL;
    else if (index == 0)
        return LL_pop(list);

    while (i < index && i < list->length)
    {
        prev = curr;
        curr = curr->next;

        i++;
    }

    if (index == (list->length - 1))
        list->tail = prev;

    prev->next = curr->next;
    clean_node(curr);
    list->length--;

    return 0;
}

void *LL_remove_by_key(LList *list, const char *key)
{

    Node *curr = list->head;
    Node *prev = NULL;

    while (curr != NULL && curr->key != NULL && strcmp(curr->key, key) != 0)
    {
        prev = curr;
        curr = curr->next;
    }

    if (curr != NULL && curr == (list->tail))
        list->tail = prev;

    if (prev == NULL)
        list->head = list->head->next;
    else
    {
        prev->next = curr->next;
    }

    clean_node(curr);
    list->length--;

    return 0;
}

int LL_insert_at_index(LList *list, unsigned int index, void *data, const char *key)
{

    int i = 0;

    Node *prev = NULL;

    Node *current = list->head;

    Node *to_insert = create_node(data, key);

    if (index > list->length)
        return INVALID_INDEX;

    if (index == 0)
    {
        clean_node(to_insert);
        return LL_push(list, data, key);
    }

    else if (index == list->length)
    {
        clean_node(to_insert);
        return LL_enqueue(list, data, key);
    }

    while (i < index)
    {
        prev = current;
        current = current->next;
        i++;
    }

    prev->next = to_insert;
    to_insert->next = current;

    list->length++;

    return 0;
}

void *LL_get(LList list, unsigned int index)
{

    int i = 0;

    Node *current = list.head;

    while (i < list.length && i < index)
    {
        current = current->next;
        i++;
    }

    if (current != NULL)
        return current->data;

    return NULL;
}

void *LL_get_by_key(LList list, const char *key)
{
    if (LL_contains_key(list, key) == FALSE)
        return NULL;

    Node *current = list.head;

    while (current != NULL)
    {
        if (current->key != NULL && strcmp(current->key, key) == 0)
            return current->data;

        current = current->next;
    }

    return NULL;
}

void *LL_pop(LList *list)
{

    Node *curr = list->head;

    void *data;

    if (list->head != NULL)
    {

        list->head = curr->next;
        list->length--;
        data = curr->data;
        clean_node(curr);

        return data;
    }

    return NULL;
}

int LL_push(LList *list, void *data, const char *key)
{

    if (data == NULL)
        return NO_DATA;

    Node *to_add = create_node(data, key);

    to_add->next = list->head;
    list->head = to_add;

    list->length++;

    if (list->length == 1)
        list->tail = list->head;

    return 0;
}

int LL_contains_key(LList list, const char *key)
{
    Node *curr = list.head;

    while (curr != NULL)
    {
        if (curr->key != NULL && strcmp(curr->key, key) == 0)
            return TRUE;

        curr = curr->next;
    }

    return FALSE;
}

// int list_contains_string(List list, const char *str)
// {
//     Node *curr = list.head;
//     int i = 0;

//     while (curr != NULL && str != NULL)
//     {
//         if (curr->data != NULL && strcmp((char *)(curr->data), str) == 0)
//         {
//             printf("Trovato\n");
//             return i;
//         }

//         i++;
//         curr = curr->next;
//     }

//     return NOT_FOUND;
// }

void LL_print(LList to_print, char *name)
{
    if (to_print.head != NULL)
        printf("List %s with size %d\n", name, to_print.length);
    else
        return;

    Node *curr = to_print.head;

    while (curr != NULL)
    {
        to_print.printer(curr);
        curr = curr->next;
    }
}