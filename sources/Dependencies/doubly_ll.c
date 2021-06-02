#include "doubly_ll.h"

/* Given a reference (pointer to pointer) to the head
   of a DLL and an int, appends a new node at the end  */
void d_append(struct dd_Node **head_ref, int new_data)
{
    /* 1. allocate node */
    struct dd_Node *new_node = (struct dd_Node *)malloc(sizeof(struct dd_Node));

    struct dd_Node *last = *head_ref; /* used in step 5*/

    /* 2. put in the data  */
    new_node->data = new_data;

    /* 3. This new node is going to be the last node, so
          make next of it as NULL*/
    new_node->next = NULL;

    /* 4. If the Linked List is empty, then make the new
          node as head */
    if (*head_ref == NULL)
    {
        new_node->prev = NULL;
        *head_ref = new_node;
        return;
    }

    /* 5. Else traverse till the last node */
    while (last->next != NULL)
        last = last->next;

    /* 6. Change the next of last node */
    last->next = new_node;

    /* 7. Make last node as previous of new node */
    new_node->prev = last;

    return;
}

/* Given a node as prev_node, insert a new node after the given node */
void d_insertAfter(struct dd_Node *prev_node, int new_data)
{
    /*1. check if the given prev_node is NULL */
    if (prev_node == NULL)
    {
        printf("the given previous node cannot be NULL");
        return;
    }

    /* 2. allocate new node */
    struct dd_Node *new_node = (struct dd_Node *)malloc(sizeof(struct dd_Node));

    /* 3. put in the data  */
    new_node->data = new_data;

    /* 4. Make next of new node as next of prev_node */
    new_node->next = prev_node->next;

    /* 5. Make the next of prev_node as new_node */
    prev_node->next = new_node;

    /* 6. Make prev_node as previous of new_node */
    new_node->prev = prev_node;

    /* 7. Change previous of new_node's next node */
    if (new_node->next != NULL)
        new_node->next->prev = new_node;
}

// This function prints contents of linked list starting
// from the given node
void d_print(struct dd_Node *node)
{
    struct dd_Node *last;
    printf("\nTraversal in forward direction \n");
    while (node != NULL)
    {
        printf(" %d ", node->data);
        last = node;
        node = node->next;
    }

    printf("\nTraversal in reverse direction \n");
    while (last != NULL)
    {
        printf(" %d ", last->data);
        last = last->prev;
    }
}

/* Function to delete a node in a Doubly Linked list */
void d_delete_node(struct dd_Node **head_ref, struct dd_Node *del)
{
    /* base case */
    if (*head_ref == NULL || del == NULL)
        return;

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