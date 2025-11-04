#include <pthread.h>
#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct{
    int *customers;
    int front;
    int rear;
    int size;
    int maxSize;
} queue_t;

queue_t* create(int maxSize)
{
    queue_t *q = (queue_t*)malloc(sizeof(queue_t));
    if (q == NULL)
    {
        fprintf(stderr, "Failed to allocate queue");
        exit(1);
    }

    q->customers = (int*)malloc(maxSize * sizeof(int*));
    if (q->customers == NULL)
    {
        fprintf(stderr, "Failed to allocate array");
        free(q);
        exit(1);
    }

    q->front = 0;
    q->rear = -1;
    q->maxSize = maxSize;
    q->size = 0; 

    return q;
}

int isEmpty(queue_t *q)
{
    return q->size == 0;
}

int dequeue(queue_t *q)
{
    int cust_id = q->customers[q->front];
    q->front = (q->front + 1) % q->maxSize;
    q->size--;
    return cust_id;
}

void enqueue(queue_t *q, int cust_id)
{
    q->rear = (q->rear + 1) % q->maxSize;
    q->customers[q->rear] = cust_id;
    q->size++;
}

void freeQueue(queue_t *q)
{
    if (q)
    {
        free(q->customers);
        free(q);
    }
}

queue_t *waiting_room;
pthread_mutex_t q_mut;
pthread_mutex_t p_mut;
pthread_mutex_t a_mut;
sem_t waiting;
sem_t ready;
sem_t *done;
int available;
int completed;
int num_chairs;

void* barber_thread()
{
    while(1)
    {
        sem_wait(&waiting);
        pthread_mutex_lock(&q_mut);
        if (isEmpty(waiting_room))
        {
            pthread_mutex_lock(&p_mut);
            printf("Barber is sleeping\n");
            pthread_mutex_unlock(&p_mut);
            pthread_mutex_unlock(&q_mut);
            break;
        }

        int cust_id = dequeue(waiting_room);

        pthread_mutex_lock(&a_mut);
        available++;
        pthread_mutex_unlock(&a_mut);
        pthread_mutex_unlock(&q_mut);

        pthread_mutex_lock(&p_mut);
        printf("Barber is cutting hair of Customer %d.\n", cust_id);
        pthread_mutex_unlock(&p_mut);

        sem_post(&done[cust_id]);

        sleep(5);

        completed++;
        
        pthread_mutex_lock(&p_mut);
        printf("Barber finished cutting hair of Customer %d. No. of cuts so far = %d\n", cust_id, completed);

        pthread_mutex_unlock(&p_mut);
    }
    return NULL;
}

void* customer_thread(void *arg)
{
    int id = *(int*)arg;
    free(arg);

tryagain:

    pthread_mutex_lock(&p_mut);
    printf("Customer %d enters the shop.\n", id);
    pthread_mutex_unlock(&p_mut);

    pthread_mutex_lock(&q_mut);
    pthread_mutex_lock(&a_mut);
    if (available > 0)
    {
        available--;
        pthread_mutex_unlock(&a_mut);

        enqueue(waiting_room, id);
        pthread_mutex_lock(&p_mut);
        printf("Customer %d takes a seat. Waiting customers = %d\n", id, waiting_room->size);
        pthread_mutex_unlock(&p_mut);

        pthread_mutex_unlock(&q_mut);

        sem_post(&waiting);

        sem_wait(&done[id]);

        pthread_mutex_lock(&p_mut);
        printf("Customer %d is getting a haircut.\n", id);
        pthread_mutex_unlock(&p_mut);

        sleep(1);

        pthread_mutex_lock(&p_mut);
        printf("Customer %d leaves the shop.\n", id);
        pthread_mutex_unlock(&p_mut);

        pthread_mutex_unlock(&q_mut);
    }
    else
    {
        pthread_mutex_lock(&p_mut);
        printf("Customer %d found no empty chair. Leaving.\n", id);
        pthread_mutex_unlock(&p_mut);
        pthread_mutex_unlock(&q_mut);
        pthread_mutex_unlock(&a_mut);

        sleep(6);
        goto tryagain;
    }

    return NULL;
}


int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        printf("invalid arguments\n");
        return -1;
    }

    int num_cust = atoi(argv[1]);
    int num_chairs_in = atoi(argv[2]);

    if (num_cust <= 0 || num_chairs_in <= 0)
    {
        printf("Number of customers and chairs must be positive\n");
        return 1;
    }
    
    num_chairs = num_chairs_in;
    available = num_chairs;

    waiting_room = create(num_chairs);

    done = (sem_t*)malloc(num_cust * sizeof(sem_t));
    if (done == NULL)
    {
        fprintf(stderr, "Failed to allocate customer semaphore\n");
        freeQueue(waiting_room);
        return 1;
    }

    for (int i = 0; i < num_cust; i++)
    {
        sem_init(&done[i], 0, 0);
    }
    sem_init(&waiting, 0, 0);
    sem_init(&ready, 0, 0);

    pthread_mutex_init(&q_mut, NULL);
    pthread_mutex_init(&p_mut, NULL);
    pthread_mutex_init(&a_mut, NULL);

    pthread_t barber;
    pthread_t *customers = (pthread_t*)malloc(num_cust * sizeof(pthread_t));

    pthread_create(&barber, NULL, barber_thread, NULL);

    for (int i = 0; i < num_cust; i++)
    {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&customers[i], NULL, customer_thread, id);
        sleep(1);
    }

    for (int i = 0; i < num_cust; i++)
    {
        pthread_join(customers[i], NULL);
    }

    sem_post(&waiting);
    pthread_join(barber, NULL);

    freeQueue(waiting_room);
    free(customers);
    free(done);
    pthread_mutex_destroy(&q_mut);
    pthread_mutex_destroy(&p_mut);

    sem_destroy(&waiting);
    sem_destroy(&ready);

    return 0;
}
