#ifndef __BUDGIE_OPLIST_INC_H
#define __BUDGIE_OPLIST_INC_H
#include <rudolph/buffer.h>

#define BUDGIE_MAX_ARG 255

enum budgie_op_type {
    /* basic brainfuck arguments */
    BOPT_P_NEXT, /* arg is how many to advance by */
    BOPT_P_PREV, /* arg is how many to go back by */
    BOPT_D_INCR, /* arg is how many to add by */
    BOPT_D_DECR, /* arg is how many to subtract by */
    BOPT_D_OUT, /* arg unused */
    BOPT_D_IN, /* arg unused */
    BOPT_F_LOPEN, /* arg unused */
    BOPT_F_LCLOS, /* arg unused */
    /* advanced optimization stuff */
    BOPT_NOOP, /* arg unused */
    BOPT_D_SET, /* arg is value to set to */
    _BOPT_MAX = BUDGIE_MAX_ARG /* maximum type */
} __attribute__ ((__packed__));

struct __attribute__((__packed__)) budgie_op {
    enum budgie_op_type type;
    unsigned char arg;
};

int budgie_oplist_create(rd_buf_t *in, rd_buf_t **out);
int budgie_oplist_optimize(rd_buf_t **in);

#endif /* __BUDGIE_OPLIST_INC_H */

    /* pseudocode:

    1. create stack
    2. top of the stack is a single value, 0.
    3. usually, when you add an instruction, push it to the data buffer
        and also add its size to the top of the stack
    4. if LOPEN is reached instead, push the usual code, but also push a new
        0 to the top of the stack
    5. if LCLOSE is reached instead, pop the top value of the stack. this
        is the jump offset (in bytes) needed to get to the start of the loop.
        push the usual code. also, add the popped value to the new top of the
        stack.
    6. at the end, the number of bytes written out will be in the top of the
        stack.
    */

