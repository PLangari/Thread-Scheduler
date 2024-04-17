//Tutorial of using contexts 

#include<ucontext.h>
#include<stdio.h>
#include<stdlib.h>

ucontext_t *ucp, *ucp2;

#define STACK_SIZE 1

int x = 0;

void sample () {

    printf("Hi there in user thread\n");
    x++;
}

void sample_with_arg (int x) {
    x++;
    printf("Integer in context %d\n", x);
}

int main() {
    ucp = (ucontext_t *)malloc(sizeof(ucontext_t));
    ucp2 = (ucontext_t *)malloc(sizeof(ucontext_t));

    getcontext(ucp);
    
    char *stack;

    stack = (char *)malloc(sizeof(char) * STACK_SIZE);

    ucp ->uc_stack.ss_sp = stack;
    ucp->uc_stack.ss_size = STACK_SIZE;
    ucp->uc_stack.ss_flags = 0;

    ucp->uc_link = ucp2;

    //Saves state to ucp

    //Initialize the context to run your desired function
    //makecontext(ucp, sample_with_arg, 1, x);

    //Sets the current context to ucp 

    //Save main thread's state
    //getcontext(ucp2);
    //if(x == 0) {
    //    setcontext(ucp);
    //}
    //resume main thread from here

    swapcontext(ucp2, ucp);

    printf("Main thread here \n");

    free(ucp);
    free(ucp2);
    free(stack);


    return 0;

}