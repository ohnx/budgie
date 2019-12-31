#include <string.h>
#include <stdlib.h>
#include "stack.h"

budgie_stack *budgie_stack_new() {
    budgie_stack *s;

    s = calloc(1, sizeof(budgie_stack));
    if (!s) return s;
    s->stack_len = 0;
    s->stack_buf_len = INITAL_STACK_LENGTH;
    s->stack_buf = calloc(INITAL_STACK_LENGTH, sizeof(void *));

    if (!s->stack_buf) {
        free(s);
        return NULL;
    }

    s->stack = s->stack_buf + INITAL_STACK_LENGTH;

    return s;
}

void *budgie_stack_pop(budgie_stack *s) {/* lazy pop */
    /* overflow check */
    if (s->stack_len == 0) return NULL;

    /* decrease the length of the stack and shift pointer over by 1 */
    s->stack_len--;
    return *(s->stack++); /* return the old pointer before the shift */
                          /* equivalent to doing (s->stack++)[0] */
}

void *budgie_stack_top(budgie_stack *s) {
    /* overflow check */
    if (s->stack_len == 0) return NULL;

    return *(s->stack); /* equivalent to doing (s->stack)[0] */
}

int budgie_stack_push(budgie_stack *s, void *data) {
    /* s->stack_buf_len is the length of memory available for stack data storage */
    
    if (s->stack_buf_len == s->stack_len) { /* we're running out of room! need to reallocate */
        void *tmp;
        /* increase stack length */
        s->stack_buf_len = s->stack_buf_len * 2;

        /* attempt realloc, return failure if failed */
        tmp = realloc(s->stack_buf, s->stack_buf_len * sizeof(void *));
        if (!tmp) return E_NOMEM;

        /* update pointer references */
        s->stack_buf = tmp;
        s->stack = s->stack_buf + s->stack_buf_len - s->stack_len;

        /* shift the stack over since realloc keeps stack data on leftmost block */
        memmove(s->stack, s->stack_buf, s->stack_len * sizeof(void *));
    }
    /* now we have enough room to add to the start. */
    s->stack_len++;
    *(--s->stack) = data;
    
    return E_NONE;
}

void budgie_stack_destroy(budgie_stack *s) {
    free(s->stack_buf);
    free(s);
}

void budgie_stack_foreach(budgie_stack *s, void (*f)(void *,void *), void *c) {
    int i = 0;
    for (i = 0; i < s->stack_len; i++) {
        f(s->stack[i], c);
    }
}

/* debug code - to use, compile with
 * `gcc src/stack.c -Iinclude/ -Wall -Werror -g -ansi -pedantic -D__STACK_DEBUG` */
#ifdef __STACK_DEBUG
#include <stdio.h>

/* simple wrapper around puts() to support budgie_stack_foreach */
void testfunc(void *d, void *c) {
    puts((char *)d);
}

int main() {
    /* test creation of new budgie_stack */
    budgie_stack *s = budgie_stack_new();

    char *str;
    char *one = "one";
    char *two = "two";
    char *three = "three";
    char *four = "four";
    char *five = "five";

    /* test addition of items normal case */
    budgie_stack_push(s, three);
    budgie_stack_push(s, two);
    budgie_stack_push(s, one);

    /* test stack top */
    puts((char *)budgie_stack_top(s));

    /* test removal of items until empty */
    while ((str = (char *)budgie_stack_pop(s)) != NULL) {
        puts(str);
    }
    puts("-----------");

    /* test addition of items normal case */
    budgie_stack_push(s, five);
    budgie_stack_push(s, four);
    budgie_stack_push(s, three);
    budgie_stack_push(s, two);

    /* test addition of items when realloc is required */
    budgie_stack_push(s, one);

    /* test foreach of budgie_stack */
    budgie_stack_foreach(s, testfunc, NULL);

    /* test removal of items until empty */
    while ((str = (char *)budgie_stack_pop(s)) != NULL);

    /* test destroying budgie_stack */
    budgie_stack_destroy(s);
    return 0;
}
#endif
