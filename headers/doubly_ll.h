#if !defined(DOUBLY_LL_H_)
#define DOUBLY_LL_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* dd_Node of a doubly linked list */
struct dd_Node
{
    int data;
    struct dd_Node *next; // Pointer to next node in DLL
    struct dd_Node *prev; // Pointer to previous node in DLL
};

/* Given a reference (pointer to pointer) to the head
   of a DLL and an int, appends a new node at the end  */
void d_append(struct dd_Node **head_ref, int new_data);

/* Given a node as prev_node, insert a new node after the given node */
void d_insertAfter(struct dd_Node *prev_node, int new_data);

// This function prints contents of linked list starting
// from the given node
void d_print(struct dd_Node *node);

/* Function to delete a node in a Doubly Linked list */
void d_delete_node(struct dd_Node **head_ref, struct dd_Node *del);

/* Function to delete a node in a Doubly Linked List given a key */
void d_delete_with_key(struct dd_Node **head_ref, int key);

#endif