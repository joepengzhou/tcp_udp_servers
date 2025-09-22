#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <iostream>

using namespace std;

void *Consumer(void *ptr);
void *Producer(void *ptr);

int common_int = 0;
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

int main()
{
    pthread_t thread1, thread2;
    int iret1, iret2;

    if ((iret1 = pthread_create(&thread1, NULL, &Consumer, NULL)))
        exit(EXIT_FAILURE);
    if ((iret2 = pthread_create(&thread2, NULL, &Producer, NULL)))
        exit(EXIT_FAILURE);

    cout << "pthread_create() for thread1 return: " << iret1 << endl;
    cout << "pthread_create() for thread2 return: " << iret2 << endl;

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    exit(EXIT_SUCCESS);
}

void *Consumer(void *ptr)
{
    do {
        pthread_mutex_lock(&mutex1);
        common_int--;
        printf("Consuming: %i\n", common_int);
        pthread_mutex_unlock(&mutex1);
    } while (common_int > -1000000);

    return NULL;
}

void *Producer(void *ptr)
{
    do {
        pthread_mutex_lock(&mutex1);
        common_int++;
        printf("Producing: %i\n", common_int);
        pthread_mutex_unlock(&mutex1);
    } while (common_int < 1000000);

    return NULL;
}
