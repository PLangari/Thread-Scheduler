#define _XOPEN_SOURCE 600

#include<ucontext.h>

#include<stdio.h>
#include<stdlib.h>

ucontext_t *ucp, *ucp2;

#define STACK_SIZE 16000


void sample () {
    printf("Hi there in user thread\n");
    setcontext(ucp2);
}

void sample_with_arg (int x) {
    printf("Integer in context %d\n", x);
    setcontext(ucp2);
}

int main () {
    // allocate memory for ucp
    ucp = (ucontext_t *) malloc(sizeof(ucontext_t));
    ucp2 = (ucontext_t *)malloc(sizeof(ucontext_t));

    // get the current context and set to ucp
    if (getcontext(ucp) == -1) {
        printf("Error getting context\n");
        exit(1);
    }

    // allocate memory for stack
    char* stack;
    stack = (char *) malloc(sizeof(char) * STACK_SIZE);

    // setting ucp stack
    ucp->uc_stack.ss_sp = stack;
    ucp->uc_stack.ss_size = STACK_SIZE;
    ucp->uc_stack.ss_flags = 0;

    // setting link to null

    ucp->uc_link = NULL;

    // setting context to run sample function
    makecontext(ucp, sample, 0);

    swapcontext(ucp2, ucp);

    printf("Main thread here\n");
    return 0;
}


