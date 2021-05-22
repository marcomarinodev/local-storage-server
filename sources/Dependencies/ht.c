#include "ht.h"

unsigned long hash_function(char *str, int ht_dim)
{
    unsigned long i = 0;
    for (int j = 0; str[j]; j++)
        i += str[j];
    return i % ht_dim;
}

LinkedList *allocate_list()
{
    // Allocates memory for a Linkedlist pointer
    LinkedList *list = (LinkedList *)malloc(sizeof(LinkedList));
    return list;
}

LinkedList *linkedlist_insert(LinkedList *list, Ht_item *item)
{
    // Inserts the item onto the Linked List
    if (!list)
    {
        LinkedList *head = allocate_list();
        head->item = item;
        head->next = NULL;
        list = head;
        return list;
    }

    else if (list->next == NULL)
    {
        LinkedList *node = allocate_list();
        node->item = item;
        node->next = NULL;
        list->next = node;
        return list;
    }

    LinkedList *temp = list;
    while (temp->next->next)
    {
        temp = temp->next;
    }

    LinkedList *node = allocate_list();
    node->item = item;
    node->next = NULL;
    temp->next = node;

    return list;
}

Ht_item *linkedlist_remove(LinkedList *list)
{
    // Removes the head from the linked list
    // and returns the item of the popped element
    if (!list)
        return NULL;
    if (!list->next)
        return NULL;
    LinkedList *node = list->next;
    LinkedList *temp = list;
    temp->next = NULL;
    list = node;
    Ht_item *it = NULL;
    memcpy(temp->item, it, sizeof(Ht_item));
    free(temp->item->key);
    if (temp->item->value.content != NULL)
        free(temp->item->value.content);
    free(&(temp->item->value));
    free(temp->item);
    free(temp);
    return it;
}

void free_linkedlist(LinkedList *list)
{
    LinkedList *temp = list;
    while (list)
    {
        temp = list;
        list = list->next;
        free(temp->item->key);
        if (temp->item->value.content != NULL)
            free(temp->item->value.content);
        free(&(temp->item->value));
        free(temp->item);
        free(temp);
    }
}

LinkedList **create_overflow_buckets(HashTable *table)
{
    // Create the overflow buckets; an array of linkedlists
    LinkedList **buckets = (LinkedList **)calloc(table->size, sizeof(LinkedList *));
    for (int i = 0; i < table->size; i++)
        buckets[i] = NULL;
    return buckets;
}

void free_overflow_buckets(HashTable *table)
{
    // Free all the overflow bucket lists
    LinkedList **buckets = table->overflow_buckets;
    for (int i = 0; i < table->size; i++)
        free_linkedlist(buckets[i]);
    free(buckets);
}

Ht_item *create_item(char *key, FRecord value, size_t file_size)
{
    // Creates a pointer to a new hash table item
    Ht_item *item = (Ht_item *)malloc(sizeof(Ht_item));
    item->key = (char *)malloc(strlen(key) + 1);
    sprintf(item->value.pathname, "%s", value.pathname);
    // printf("content strlen = %ld\n", strlen(value.content));

    if (value.content == NULL)
    {
        item->value.content = NULL;
    }
    else
    {
        printf("\n<<<sizeof_ content = %ld>>>\n", file_size);
        item->value.content = malloc(1 + (sizeof(char) * file_size));
        memcpy(item->value.content, value.content, file_size);
    }

    item->value.is_open = value.is_open;
    item->value.is_locked = value.is_locked;
    item->value.is_new = value.is_new;
    item->value.is_victim = value.is_victim;
    item->value.last_client = value.last_client;
    item->value.last_edit = value.last_edit;
    item->value.size = value.size;
    item->value.lock = value.lock;

    strcpy(item->key, key);

    return item;
}

HashTable *create_table(int size, size_t capacity, int max_files)
{
    // Creates a new HashTable
    HashTable *table = (HashTable *)malloc(sizeof(HashTable));
    table->size = size;
    table->file_size = 0;
    table->capacity = capacity;
    table->max_files = max_files;
    table->count = 0;
    table->items = (Ht_item **)calloc(table->size, sizeof(Ht_item *));
    for (int i = 0; i < table->size; i++)
        table->items[i] = NULL;
    table->overflow_buckets = create_overflow_buckets(table);

    return table;
}

void free_item(Ht_item *item)
{
    // Frees an item
    free(item->key);
    if (item->value.content != NULL)
        free(item->value.content);
    free(item);
}

void free_table(HashTable *table)
{
    // Frees the table
    for (int i = 0; i < table->size; i++)
    {
        Ht_item *item = table->items[i];
        if (item != NULL)
            free_item(item);
    }

    free_overflow_buckets(table);
    free(table->items);
    free(table);
}

void handle_collision(HashTable *table, unsigned long index, Ht_item *item)
{
    LinkedList *head = table->overflow_buckets[index];

    if (head == NULL)
    {
        // We need to create the list
        head = allocate_list();
        head->item = item;
        table->overflow_buckets[index] = head;
        return;
    }
    else
    {
        // Insert to the list
        table->overflow_buckets[index] = linkedlist_insert(head, item);
        return;
    }
}

void ht_insert(HashTable *table, char *key, FRecord value, size_t file_size)
{
    // Create the item
    Ht_item *item = create_item(key, value, file_size);

    // Compute the index
    unsigned long index = hash_function(key, table->size);

    Ht_item *current_item = table->items[index];

    if (current_item == NULL)
    {
        // Key does not exist.
        if (table->count == table->size)
        {
            // Hash Table Full
            printf("Insert Error: Hash Table is full\n");
            // Remove the create item
            free_item(item);
            return;
        }

        // Insert directly
        table->items[index] = item;
        table->file_size += item->value.size;
        table->count++;
    }

    else
    {
        // Scenario 1: We only need to update value
        if (strcmp(current_item->key, key) == 0)
        {
            table->file_size -= table->items[index]->value.size;
            table->items[index]->value = value;
            table->file_size += value.size;
            return;
        }

        else
        {
            // Scenario 2: Collision
            handle_collision(table, index, item);
            return;
        }
    }
}

FRecord *ht_search(HashTable *table, char *key)
{
    // Searches the key in the hashtable
    // and returns NULL if it doesn't exist
    int index = hash_function(key, table->size);
    Ht_item *item = table->items[index];
    LinkedList *head = table->overflow_buckets[index];

    // Ensure that we move to items which are not NULL
    while (item != NULL)
    {
        if (strcmp(item->key, key) == 0)
        {
            printf("SEARCH RESULT: %s\n", item->value.pathname);
            return &(item->value);
        }
        if (head == NULL)
            return NULL;
        item = head->item;
        head = head->next;
    }
    return NULL;
}

void ht_delete(HashTable *table, char *key)
{
    // Deletes an item from the table
    int index = hash_function(key, table->size);
    Ht_item *item = table->items[index];
    LinkedList *head = table->overflow_buckets[index];

    if (item == NULL)
    {
        // Does not exist. Return
        return;
    }
    else
    {
        if (head == NULL && strcmp(item->key, key) == 0)
        {
            // No collision chain. Remove the item
            // and set table index to NULL
            table->items[index] = NULL;
            table->file_size -= item->value.size;
            free_item(item);
            table->count--;
            return;
        }
        else if (head != NULL)
        {
            // Collision Chain exists
            if (strcmp(item->key, key) == 0)
            {
                // Remove this item and set the head of the list
                // as the new item

                free_item(item);
                LinkedList *node = head;
                head = head->next;
                node->next = NULL;
                table->items[index] = create_item(node->item->key, node->item->value, node->item->value.size);
                free_linkedlist(node);
                table->overflow_buckets[index] = head;
                return;
            }

            LinkedList *curr = head;
            LinkedList *prev = NULL;

            while (curr)
            {
                if (strcmp(curr->item->key, key) == 0)
                {
                    if (prev == NULL)
                    {
                        // First element of the chain. Remove the chain
                        free_linkedlist(head);
                        table->overflow_buckets[index] = NULL;
                        return;
                    }
                    else
                    {
                        // This is somewhere in the chain
                        prev->next = curr->next;
                        curr->next = NULL;
                        free_linkedlist(curr);
                        table->overflow_buckets[index] = head;
                        return;
                    }
                }
                curr = curr->next;
                prev = curr;
            }
        }
    }
}

int lru(HashTable *table, char *victim_pathname)
{
    if (table->size == 0)
        return -1;

    /* most recent time */
    time_t oldest = time(NULL) + 1;

    for (int i = 0; i < table->size; i++)
    {
        if (table->items[i])
        {

            if ((difftime(oldest, table->items[i]->value.last_edit) > 0) && (table->items[i]->value.is_new == 0) && (table->items[i]->value.is_victim == 0))
            {
                /* found a later time than oldest */
                oldest = table->items[i]->value.last_edit;
                strncpy(victim_pathname, table->items[i]->value.pathname, strlen(table->items[i]->value.pathname) + 1);
            }

            /* searching through the overflow buckets if exist */
            if (table->overflow_buckets[i])
            {
                LinkedList *head = table->overflow_buckets[i];
                while (head)
                {
                    if ((difftime(oldest, head->item->value.last_edit) > 0) && (table->overflow_buckets[i]->item->value.is_new == 0) && (table->overflow_buckets[i]->item->value.is_victim == 0))
                    {
                        /* found a later time than oldest */
                        oldest = head->item->value.last_edit;
                        strncpy(victim_pathname, head->item->value.pathname, strlen(head->item->value.pathname) + 1);
                    }
                    head = head->next;
                }
            }
        }
    }

    return 0;
}

void print_table(HashTable *table)
{
    printf("\n--------SERVER-INFO----------\n");
    printf(" (*) current storage size = %ld out of %ld\n", table->file_size, table->capacity);
    printf(" (*) number of file       = %d\n", table->count);
    printf("\n--------STORAGE--------------\n");
    if (table->size == 0)
        printf("Storage is empty\n");

    for (int i = 0; i < table->size; i++)
    {
        if (table->items[i])
        {
            struct tm time_format = *localtime(&table->items[i]->value.last_edit);
            printf("Index:%d, Key:%s,\nISNEW: %d, LOCKED: %d, OPENED: %d, SIZE: %ld,\n LAST EDIT: %d/%d/%d at %d-%d-%d\n", i, table->items[i]->key,
                   table->items[i]->value.is_new, table->items[i]->value.is_locked, table->items[i]->value.is_open, table->items[i]->value.size,
                   1900 + time_format.tm_year, time_format.tm_mon + 1, time_format.tm_mday,
                   time_format.tm_hour, time_format.tm_min, time_format.tm_sec);

            if (table->overflow_buckets[i])
            {
                printf(" => Overflow Bucket => ");
                LinkedList *head = table->overflow_buckets[i];
                while (head)
                {
                    struct tm _time_format = *localtime(&head->item->value.last_edit);
                    printf("Index:%d, Key:%s,\nISNEW: %d, LOCKED: %d, OPENED: %d, SIZE: %ld,\n LAST EDIT: %d/%d/%d at %d-%d-%d\n", i, head->item->key,
                           head->item->value.is_new, head->item->value.is_locked, head->item->value.is_open, head->item->value.size,
                           1900 + _time_format.tm_year, _time_format.tm_mon + 1, _time_format.tm_mday,
                           _time_format.tm_hour, _time_format.tm_min, _time_format.tm_sec);
                    head = head->next;
                }
            }
            printf("\n");
        }
    }
    printf("-------------------\n");
}