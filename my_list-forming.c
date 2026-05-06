/*
  list-forming.c: 
  Each thread generates a data node, attaches it to a global list. This is reapeated for K times.
  There are num_threads threads. The value of "num_threads" is input by the student.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sched.h>

#define K 200 // genreate a data node for K times in each thread

struct Node
{
    int data;
    struct Node* next;
};

struct list
{
     struct Node * header;
     struct Node * tail;
};

pthread_mutex_t    mutex_lock;

struct list *List;

void bind_thread_to_cpu(int cpuid) {
     cpu_set_t mask;
     CPU_ZERO(&mask);

     CPU_SET(cpuid, &mask);
     if (sched_setaffinity(0, sizeof(cpu_set_t), &mask)) {
         fprintf(stderr, "sched_setaffinity");
         exit(EXIT_FAILURE);
     }
}

struct Node* generate_data_node()
{
    struct Node *ptr;
    ptr = (struct Node *)malloc(sizeof(struct Node));    

    if( NULL != ptr ){
        ptr->next = NULL;
    }
    else {
        printf("Node allocation failed!\n");
    }
    return ptr;
}


//first of all try lock does busy waiting, so try -> fail, try again etc, so like polling
//so changing from trylock does help, but the main thing is how many threads lock
//and cause waiting for other threads, but if each thread builds their own list
//locks the global and attaches their list it greatly reduces bottle neck of needing exclusive
//time changing globals 
void * producer_thread(void *arg)
{
    bind_thread_to_cpu(*((int*)arg));

    struct Node *ptr;//temperary pointer for each new node
    struct Node *local_head = NULL;
    struct Node *local_tail = NULL;
    int counter = 0;

    //build local list first, no lock for this
    while (counter < K)
    {
        ptr = generate_data_node();//allocate node

        if (ptr != NULL)
        {
            ptr->data = 1;
            ptr->next = NULL;

            if (local_head == NULL)
            {
                local_head = ptr;
                local_tail = ptr;
            }
            else//link current tail to new node, and move tail forward
            {
                local_tail->next = ptr;
                local_tail = ptr;
            }
        }

        counter++;
    }
    //after this while loop each thread has its own private linked list of K nodes
    //no locking was used

    // attach local list to global list, one lock for entire local list
    pthread_mutex_lock(&mutex_lock);

    if (List->header == NULL)//if global list is empty
    {
        List->header = local_head;
        List->tail = local_tail;
    }
    else//if global list is not empty
    {
        List->tail->next = local_head;
        List->tail = local_tail;
    }

    pthread_mutex_unlock(&mutex_lock);
    //then unlock once done attaching local list, so other threads can attach
    //their own local lists

    return NULL;
}

int main(int argc, char* argv[])
{
    int i, num_threads;

    int NUM_PROCS;//number of CPU
    int* cpu_array = NULL;

    struct Node  *tmp,*next;
    struct timeval starttime, endtime;

    num_threads = atoi(argv[1]); //read num_threads from user
    pthread_t producer[num_threads];
    NUM_PROCS = sysconf(_SC_NPROCESSORS_CONF);//get number of CPU
    if( NUM_PROCS > 0)
    {
        cpu_array = (int *)malloc(NUM_PROCS*sizeof(int));
        if( cpu_array == NULL )
        {
            printf("Allocation failed!\n");
            exit(0);
        }
        else
        {
            for( i = 0; i < NUM_PROCS; i++)
               cpu_array[i] = i;
        }

    }

    pthread_mutex_init(&mutex_lock, NULL);

    List = (struct list *)malloc(sizeof(struct list));
    if( NULL == List )
    {
       printf("End here\n");
       exit(0);	
    }
    List->header = List->tail = NULL;

    gettimeofday(&starttime,NULL); //get program start time
    for( i = 0; i < num_threads; i++ )
    {
        pthread_create(&(producer[i]), NULL, (void *) producer_thread, &cpu_array[i%NUM_PROCS]); 
    }

    for( i = 0; i < num_threads; i++ )
    {
        if(producer[i] != 0)
        {
            pthread_join(producer[i],NULL);
        }
    }

    gettimeofday(&endtime,NULL); //get the finish time

    if( List->header != NULL )
    {
        next = tmp = List->header;
        while( tmp != NULL )
        {  
           next = tmp->next;
           free(tmp);
           tmp = next;
        }            
    }
    if( cpu_array!= NULL)
       free(cpu_array);
    /* calculate program runtime */
    printf("Total run time is %ld microseconds.\n", (endtime.tv_sec-starttime.tv_sec) * 1000000+(endtime.tv_usec-starttime.tv_usec));
    return 0; 
}