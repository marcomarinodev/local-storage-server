#include "doubly_ll.h"

int d_is_empty(struct dd_Node *node)
{
    return node == NULL;
}

/* Given a reference (pointer to pointer) to the head
   of a DLL and an int, appends a new node at the end  */
void d_append(struct dd_Node **head_ref, int new_data)
{
    struct dd_Node *new_node = (struct dd_Node *)malloc(sizeof(struct dd_Node));

    if (!new_node)
    {
        perror("cannot allocate memory");
        exit(EXIT_FAILURE);
    }

    struct dd_Node *last = *head_ref;

    new_node->data = new_data;
    new_node->next = NULL;

    if (*head_ref == NULL)
    {
        new_node->prev = NULL;
        *head_ref = new_node;
        return;
    }

    while (last->next != NULL)
        last = last->next;

    last->next = new_node;

    new_node->prev = last;

    return;
}

/* Given a node as prev_node, insert a new node after the given node */
void d_insertAfter(struct dd_Node *prev_node, int new_data)
{
    if (prev_node == NULL)
    {
        printf("the given previous node cannot be NULL");
        return;
    }

    struct dd_Node *new_node = (struct dd_Node *)malloc(sizeof(struct dd_Node));

    if (!new_node)
    {
        perror("cannot allocate memory");
        exit(EXIT_FAILURE);
    }

    new_node->data = new_data;
    new_node->next = prev_node->next;
    prev_node->next = new_node;
    new_node->prev = prev_node;

    if (new_node->next != NULL)
        new_node->next->prev = new_node;
}

void d_print(struct dd_Node *node)
{
    struct dd_Node *curr;

    curr = node;

    while (curr != NULL)
    {
        printf(" %d ", curr->data);
        curr = curr->next;
    }
}

/* Function to delete a node in a Doubly Linked list */
void d_delete_node(struct dd_Node **head_ref, struct dd_Node *del)
{
    /* base case */
    if (*head_ref == NULL || del == NULL)
    {
        printf("attempting to delete NULL\n");
        return;
    }

    /* If node to be deleted is head node */
    if (*head_ref == del)
        *head_ref = del->next;

    /* Change next only if node to be deleted is NOT
       the last node */
    if (del->next != NULL)
        del->next->prev = del->prev;

    /* Change prev only if node to be deleted is NOT
       the first node */
    if (del->prev != NULL)
        del->prev->next = del->next;

    /* Finally, free the memory occupied by del*/
    free(del);
}

/* Function to delete a node in a Doubly Linked List given a key */
void d_delete_with_key(struct dd_Node **head_ref, int key)
{
    if (*head_ref == NULL)
        return;

    struct dd_Node *current = *head_ref;

    while (current != NULL)
    {
        if (current->data == key)
        {
            d_delete_node(head_ref, current);
            return;
        }
        current = current->next;
    }
}