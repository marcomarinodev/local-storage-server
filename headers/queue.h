#if !defined(QUEUE_H_)
#define QUEUE_H_

/* Node to store a queue entry */
struct qnode
{
    void *key;
    struct qnode *next;
};

struct queue
{
    struct qnode *front;
    struct qnode *rear;
    int size;
};

struct qnode *newNode(void *key);

/* Creates an empty queue */
struct queue *create_queue();

/* Add a new node at the end of queue and change rear */
void enqueue(struct queue *q, void *key);

/* Removes the head of the queue */
struct qnode *dequeue(struct queue *q);

/* Remove elements in queue */
void destroy_queue(struct queue *q);

/* Returns 0 if the queue is not empty, 1 otherwise */
int is_empty(struct queue *q);

#endif /* QUEUE_H_ */
