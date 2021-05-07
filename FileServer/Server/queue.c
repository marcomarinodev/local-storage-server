#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

struct qnode *newNode(int key)
{
    struct qnode *temp = (struct qnode *)malloc(sizeof(struct qnode));
    temp->key = key;
    temp->next = NULL;
    return temp;
}

struct queue *create_queue()
{
    struct queue *q = (struct queue *)malloc(sizeof(struct queue));
    q->front = q->rear = NULL;
    q->size = 0;
    return q;
}

void enqueue(struct queue *q, int k)
{
    // Create a new LL node
    struct qnode *temp = newNode(k);

    // If queue is empty, then new node is front and rear both
    if (q->rear == NULL)
    {
        q->front = q->rear = temp;
        return;
    }

    // Add the new node at the end of queue and change rear
    q->rear->next = temp;
    q->rear = temp;
    q->size++;
}

struct qnode *dequeue(struct queue *q)
{
    if (q->front == NULL)
        return NULL;

    struct qnode *tmp = q->front;

    q->front = q->front->next;

    if (q->front == NULL)
        q->rear = NULL;

    q->size--;
    return tmp;
}

void destroy_queue(struct queue *q)
{
    while (q->front == NULL)
    {
        struct qnode *tmp = dequeue(q);
        free(tmp);
    }
}

int is_empty(struct queue *q)
{
    if (q->front == NULL) return 1;
    else return 0;
}