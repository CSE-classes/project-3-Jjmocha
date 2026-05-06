#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define BUFFER_SIZE 5

//buffer is a global (shared)
char buffer[BUFFER_SIZE];
int in = 0;
int out = 0;
int count = 0;
int done = 0;

pthread_mutex_t mutex;
pthread_cond_t not_full;
pthread_cond_t not_empty;

void *producer(void *arg)
{
    FILE *fp;
    char c;

    fp = fopen("message.txt", "r");
    if (fp == NULL)
    {
        printf("ERROR: cannot open message.txt\n");
        pthread_exit(NULL);
    }

    while ((c = fgetc(fp)) != EOF)
    {
        //about to touch shared memory so lock it
        pthread_mutex_lock(&mutex);

        //if full, prod can't add more
        while (count == BUFFER_SIZE)
        {//function makes thread go to sleep, so not wasting cpu, and unlock mutex
            pthread_cond_wait(&not_full, &mutex);
        }
        //if consumer takes out of buffer, then locks it and continues

        buffer[in] = c;
        in = (in + 1) % BUFFER_SIZE;//move pointer forward here the modulo makes it circular
        count++;

        pthread_cond_signal(&not_empty);//showing data is available
        pthread_mutex_unlock(&mutex);   //now unlocks data
    }

    fclose(fp);

    pthread_mutex_lock(&mutex);//modifying shared done flag
    done = 1;//let consumer know we are done writing
    pthread_cond_signal(&not_empty);
    pthread_mutex_unlock(&mutex);//unlocks again

    pthread_exit(NULL);
}

void *consumer(void *arg)
{
    char c;

    //keep checking until explicitly told done = 1
    while (1)
    {
        //lock because about to check and pull out variables
        pthread_mutex_lock(&mutex);

        //if no data, but not told done either
        while (count == 0 && !done)
        {//go to sleep, and unlock for producer
            pthread_cond_wait(&not_empty, &mutex);
        }

        //if no data, and told done
        if (count == 0 && done)
        {
            //unlock and break for forever loop
            pthread_mutex_unlock(&mutex);
            break;
        }

        c = buffer[out];//read out buffer
        out = (out + 1) % BUFFER_SIZE;
        count--;//decrease count when read from buffer

        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&mutex);

        printf("%c", c);//print the char to the terminal
    }

    pthread_exit(NULL);
}

int main()
{
    pthread_t prod, cons;

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&not_full, NULL);
    pthread_cond_init(&not_empty, NULL);

    pthread_create(&prod, NULL, producer, NULL);
    pthread_create(&cons, NULL, consumer, NULL);

    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&not_full);
    pthread_cond_destroy(&not_empty);

    return 0;
}
