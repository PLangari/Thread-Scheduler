// How to use threads in C

#include<pthread.h>

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>

pthread_mutex_t *lock;

typedef struct s { 
    int a;
    int b;
}s;

void * sample() {
    printf("hello\n");
}

void * sample_int(void *x) {

    pthread_mutex_lock(lock);
    printf("hello %d\n", *((int *)x));
    pthread_mutex_unlock(lock);

    pthread_exit((void **) x);
}

void * sample_int2(void *x) {

    printf("hello %d %d\n", ((s *)x)->a,((s *)x)->b);

}


int main() {

    pthread_t *t;

    lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));

    pthread_mutex_init(lock, NULL);

    int y = 0;

    t = (pthread_t *)malloc(sizeof(pthread_t));

    //pthread_create(t, NULL, sample, NULL);
    //
    //pthread_join(*t, NULL);

    pthread_create(t, NULL, sample_int, (void *)(&y));


    pthread_mutex_lock(lock);  
    for(int i = 0; i < 10000; i++) {
        y++;
    }
    pthread_mutex_unlock(lock);

    pthread_join(*t, NULL);

    s *x;

    x = (s *)malloc(sizeof(s));

    x->a = 145;
    x ->b = 11;

    pthread_create(t, NULL, sample_int2, (void*)x);

    pthread_join(*t, NULL);
 
    void **ret;
    int **arr;

    ret = (int **)malloc(sizeof(int *));

    *ret = (int *)malloc(sizeof(int));

    pthread_create(t, NULL, sample_int, (void*)&y);

    pthread_join(*t, ret);

    printf("The int %d \n", *((int *)(*ret)));

    return 0;

}