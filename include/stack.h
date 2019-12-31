#ifndef __STACK_INC
#define __STACK_INC

#define INITAL_STACK_LENGTH     4

#define E_NONE      0
#define E_NOMEM     -4

typedef struct _stack {
    int stack_len; /* length of the budgie_stack */
    int stack_buf_len; /* length of the stack buffer */
    void **stack_buf; /* pointer to the start of the stack buffer */
    void **stack; /* pointer to the start of the budgie_stack */
} budgie_stack;

/* create new budgie_stack */
budgie_stack *budgie_stack_new();
/* remove an item from the budgie_stack */
void *budgie_stack_pop(budgie_stack *s);
/* get top item from the budgie_stack */
void *budgie_stack_top(budgie_stack *s);
/* add an item to the budgie_stack */
int budgie_stack_push(budgie_stack *s, void *data);
/* destroy the budgie_stack */
void budgie_stack_destroy(budgie_stack *q);
/* loop through the stack with a function, with a given context c
 * f is called as f(stack elem, c) */
void budgie_stack_foreach(budgie_stack *q, void (*f)(void *,void *), void *c);

#endif /* __STACK_INC */
