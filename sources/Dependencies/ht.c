#include "ht.h"

unsigned int sdbm_hash(const char *str, int size)
{
    unsigned long hash = 0;
    int c;

    while ((c = *str++))
        hash = c + (hash << 6) + (hash << 16) - hash;

    return hash % size;
}

void ht_create(HashTable *storage, int size)
{
    storage->lists = malloc(sizeof(LList) * size);
    storage->size = size;

    for (int i = 0; i < size; i++)
        LL_init(&(storage->lists[i]), print_record_node);
}

void ht_free(HashTable storage)
{
    for (int i = 0; i < storage.size; i++)
    {
        LL_free(storage.lists[i], clean_record_node);
    }

    free(storage.lists);
}

void *ht_get(HashTable storage, const char *key)
{
    Node *curr = NULL;
    int index = sdbm_hash(key, storage.size);
    LList container = storage.lists[index];

    curr = container.head;

    while (curr != NULL && strcmp(key, curr->key) != 0)
        curr = curr->next;

    if (curr != NULL)
        return curr->data;

    return NULL;
}

int ht_insert(HashTable *storage, void *data, const char *key)
{
    int index = sdbm_hash(key, storage->size);

    if (LL_contains_key(storage->lists[index], key))
        LL_remove_by_key(&(storage->lists[index]), key);

    LL_push(&(storage->lists[index]), data, key);

    return 0;
}

int ht_delete(HashTable *storage, const char *key)
{
    int index = sdbm_hash(key, storage->size);

    LL_remove_by_key(&(storage->lists[index]), key);

    return 0;
}

int ht_exists(HashTable storage, const char *key)
{
    for (int i = 0; i < storage.size; i++)
    {
        if (storage.lists[i].length != 0)
        {
            if (LL_contains_key(storage.lists[i], key))
                return TRUE;
        }
    }

    return FALSE;
}

void print_record_node(Node *to_print)
{
    FRecord r = *(FRecord *)to_print->data;

    printf("Path: %s\nContent:%s\n\n", r.pathname, r.content);
}

void clean_record_node(Node *to_clean)
{
    FRecord r = *(FRecord *)to_clean->data;

    if (r.content)
        free(r.content);

    // free(to_clean);
}

void ht_print(HashTable storage)
{
    char *list_name = (char *)malloc(sizeof(char) * 1024);

    for (int i = 0; i < storage.size; i++)
    {
        sprintf(list_name, "%d", i);
        LL_print(storage.lists[i], list_name);
    }

    free(list_name);
}
