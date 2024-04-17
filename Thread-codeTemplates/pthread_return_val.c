#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>

pthread_t *t; 
char* str;

//Integer argument
void *sample_int(void *int_arg) {

    printf("The integer argument passed is %d \n", *(int *)int_arg);
    printf("The string argument passed is %s \n", str);
    pthread_exit((void **)int_arg);

}

int cacapipi() {

    str = (char *)malloc(sizeof(char) * 100);

    str = "Hello World";

    int n = 1028975;

    t = (pthread_t *)malloc(sizeof(pthread_t));

    void **ret_val;

    ret_val = (void **)malloc(sizeof(int *));

    //Creating a thread passing an integer argument to the routine
    pthread_create(t, NULL, sample_int, (void *)&n);
    pthread_join(*t, ret_val);

    //Please take care of this carefully
    //We need the return value sent from the routine.
    //But now the type is void **
    //So first, we deference it as void * by doing (*ret_val)
    //Now we do the type conversion.
    //void *return_value = *ret_val
    printf("Integer value returned by the thread is %d\n", *(int *)(*ret_val));
    printf("String value returned by the thread is %s\n", str);
    return 0;

}