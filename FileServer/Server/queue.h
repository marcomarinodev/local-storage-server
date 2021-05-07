

/* Node to store a queue entry */
struct qnode
{
    int key;
    struct qnode *next;
};

struct queue
{
    struct qnode *front;
    struct qnode *rear;
    int size;
};

struct qnode *newNode(int key);

/* Creates an empty queue */
struct queue *create_queue();

/* Add a new node at the end of queue and change rear */
void enqueue(struct queue *q, int key);

/* Removes the head of the queue */
struct qnode *dequeue(struct queue *q);

/* Remove elements in queue */
void destroy_queue(struct queue *q);

/* Returns 0 if the queue is not empty, 1 otherwise */
int is_empty(struct queue *q);
