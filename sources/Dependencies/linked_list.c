#include "linked_list.h"

Node *create_node(void *to_set, const char *key)
{
    Node *node = malloc(sizeof(Node));

    node->prev = NULL;
    node->next = NULL;
    node->data = to_set;
    node->key = key;

    return node;
}

void clean_node(Node *to_clean, int clean_data)
{
    if (to_clean->data != NULL && clean_data == TRUE)
        free(to_clean->data);
    if (to_clean->key != NULL)
        free((void *)to_clean->key);

    free(to_clean);
}

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
        clean_node(prev, TRUE);
    }
}

void LL_init(LList *list, void (*printer)(Node *))
{
    list->head = NULL;
    list->tail = NULL;
    list->length = 0;
    list->printer = printer;
}

int LL_enqueue(LList *list, void *data, const char *key)
{
    // Creo il nodo
    Node *to_add = create_node(data, key);

    // Se non ho la coda, non ho mai inserito niente, quindi la lista è vuota
    if (list->tail == NULL)
    {
        list->head = to_add;
        list->tail = list->head;
    }
    else
    {
        // Lo metto in fondo e aggiorno la coda
        list->tail->next = to_add;
        list->tail = list->tail->next;
    }

    // La lunghezza della lista è aumentata
    list->length++;

    return 0;
}

void *LL_dequeue(LList *list)
{
    // Equivale a una pop
    return LL_pop(list);
}

void *LL_remove_by_key(LList *list, const char *key)
{
    // Nodi di scorrimento
    Node *curr = list->head;
    Node *prev = NULL;

    // Scorro per trovare il nodo che voglio cancellare
    while (curr != NULL && curr->key != NULL && strcmp(curr->key, key) != 0)
    {
        prev = curr;
        curr = curr->next;
    }

    // Se ho rimosso l'ultimo elemento, aggiorno la coda
    if (curr != NULL && curr == (list->tail))
        list->tail = prev;

    // Aggiusto i puntatori
    if (prev == NULL)
        list->head = list->head->next;
    else
        prev->next = curr->next;
    // Pulisco il nodo
    clean_node(curr, TRUE);
    // Decremento la lunghezza
    list->length--;

    return 0;
}

int LL_push(LList *list, void *data, const char *key)
{
    // Ritorno null se i dati non esistono
    if (data == NULL)
        return NO_DATA;

    // Altrimenti creo un nuovo nodo con i dati
    Node *to_add = create_node(data, key);
    // E lo imposto come nuova testa della lista
    to_add->next = list->head;
    list->head = to_add;

    // Incremento il numero di elementi nella lista
    list->length++;
    // Se ho aggiunto il primo elemento, aggiorno la coda
    if (list->length == 1)
        list->tail = list->head;

    return 0;
}

void *LL_get(LList list, unsigned int index)
{
    int i = 0;
    Node *current = list.head;

    if (index >= list.length)
        return NULL;

    while (i < index)
    {
        current = current->next;
        i++;
    }

    if (current != NULL)
        return current->data;

    return NULL;
}

void *LL_pop(LList *list)
{

    Node *curr = list->head;
    void *data;

    if (list->head != NULL)
    {

        data = curr->data;

        list->head = curr->next;
        list->length--;

        if (list->length == 0)
            list->tail = NULL;

        clean_node(curr, FALSE);

        return data;
    }

    return NULL;
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

void LL_print(LList to_print, char *name)
{
    if (to_print.head != NULL)
        printf("\nRow %s with %d elements\n", name, to_print.length);
    else
        return;

    Node *curr = to_print.head;

    while (curr != NULL)
    {
        to_print.printer(curr);
        curr = curr->next;
    }
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