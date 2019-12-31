/* this file parses the brainfuck input into an IR and optimizes the IR */
#include "stack.h"
#include "ir.h"

int budgie_oplist_create(rd_buf_t *in, rd_buf_t **out) {
    size_t n, i, m;
    int rc, d;
    struct budgie_op cur_op, *last_op;
    unsigned char *buf;

    /* initial setup */
    n = in->len;
    *out = NULL;
    m = d = 0;
    buf = rd_buffer_data(in);
    cur_op.arg = 1;

    /* loop through each character */
    /* at this step, we already perform the basic optimization of grouping instructions */
    for (i = 0; i < n; i++) {
        switch (buf[i]) {
        case '>':
            cur_op.type = BOPT_P_NEXT;
            goto groupinstr;
        case '<':
            cur_op.type = BOPT_P_PREV;
            goto groupinstr;
        case '+':
            cur_op.type = BOPT_D_INCR;
            goto groupinstr;
        case '-':
            cur_op.type = BOPT_D_DECR;
            goto groupinstr;
        case '.':
            cur_op.type = BOPT_D_OUT;
            goto pushinstr;
        case ',':
            cur_op.type = BOPT_D_IN;
            goto pushinstr;
        case '[':
            cur_op.type = BOPT_F_LOPEN;
            d++;
            goto pushinstr;
        case ']':
            cur_op.type = BOPT_F_LCLOS;
            if (--d < 0) {
                /* stack pop returned null */
                rc = -__LINE__;
                goto cleanup;
            }
            goto pushinstr;
        default: continue;
        }
    groupinstr:
        /* groupable instruction */
        /* check if last operation was the same as this one */
        last_op = (struct budgie_op *)(rd_buffer_data(*out)) + m - 1;
        if (m > 0 && last_op->type == cur_op.type && last_op->arg < BUDGIE_MAX_ARG) {
            last_op->arg++;
            continue;
        }
    pushinstr:
        /* non-groupable instruction or different from last one */
        /* push the instruction to the array */
        rd_buffer_push(out, (const unsigned char *)&cur_op, sizeof(cur_op));

        /* reset the argument */
        cur_op.arg = 1;

        /* increment number of instructions pushed */
        m++;
    }

    /* check number of braces */
    if (d) {
        /* too many open ones */
        rc = -__LINE__;
    } else {
        /* no problemo */
        rc = 0;
    }

cleanup:
    return rc;
}

int budgie_oplist_optimize(rd_buf_t **in) {
    size_t i, n;
    struct budgie_op *ops;

    /* initial values */
    ops = (struct budgie_op *)rd_buffer_data(*in);
    n = (*in)->len / sizeof(struct budgie_op);

    /* loop through all instructions */
    for (i = 0; i < n; i++) {
        if (ops[i].type == BOPT_F_LCLOS) {
            if (i >= 2 && (ops[i-1].type == BOPT_D_DECR || ops[i-1].type == BOPT_D_INCR) && ops[i-2].type == BOPT_F_LOPEN) {
                /* this is [-] or [+] which optimizes to set 0 */
                ops[i-2].type = BOPT_D_SET;
                ops[i-2].arg = 0;

                /* the remaining ops can be changed to NOOP's */
                ops[i-1].type = BOPT_NOOP;
                ops[i].type = BOPT_NOOP;
            }
        }
    }

    /* ... TODO ... add more kinds of optimizations */
    /* see http://calmerthanyouare.org/2015/01/07/optimizing-brainfuck.html */
    return 0;
}

/* debug code - to use, compile with
 * `gcc src/ir.c src/stack.c -Iinclude/ -Idist/librudolph/include/ -Wall -Werror \
     -g -ansi -pedantic -Ldist/ -lrudolph -DRUDOLF_USE_STDLIB -D__IR_DEBUG` */
#ifdef __IR_DEBUG
#include <stdio.h>

const char *op2str(enum budgie_op_type type) {
    switch (type) {
    case BOPT_P_NEXT: return "BOPT_P_NEXT";
    case BOPT_P_PREV: return "BOPT_P_PREV";
    case BOPT_D_INCR: return "BOPT_D_INCR";
    case BOPT_D_DECR: return "BOPT_D_DECR";
    case BOPT_D_OUT: return "BOPT_D_OUT";
    case BOPT_D_IN: return "BOPT_D_IN";
    case BOPT_F_LOPEN: return "BOPT_F_LOPEN";
    case BOPT_F_LCLOS: return "BOPT_F_LCLOS";
    case BOPT_NOOP: return "BOPT_NOOP";
    case BOPT_D_SET: return "BOPT_D_SET";
    default: return NULL;
    }
}

void test_code(const char *code, size_t len, int expect) {
    rd_buf_t *data = NULL, *output;
    struct budgie_op *ops;
    size_t i, n, j, t = 0;
    int rc;

    printf("compiling `%s`... ", code);

    /* push the code */
    rd_buffer_push(&data, (const unsigned char *)code, len);

    /* compile the instructions to an IR */
    rc = budgie_oplist_create(data, &output);
    if (rc == 0 && expect != 0) {
        printf("expected failure but got success...\n");
        goto cleanup;
    } else if (rc != 0 && expect == 0) {
        printf("expected success but got failure...\n");
        goto cleanup;
    } else if (rc != 0) {
        printf("ok!\n");
        goto cleanup;
    } else if (rc == 0) {
        printf("ok!\n");
    }

    /* optimize the IR */
    budgie_oplist_optimize(&output);

    /* loop through all the IR instructions */
    n = output->len / sizeof(struct budgie_op);
    ops = (struct budgie_op *)rd_buffer_data(output);
    for (i = 0; i < n; i++) {
        if (ops[i].type == BOPT_F_LCLOS) t--;
        for (j = 0; j < t; j++) {
            printf("\t");
        }
        printf("%lu = %s %u\n", i, op2str(ops[i].type), ops[i].arg);
        if (ops[i].type == BOPT_F_LOPEN) t++;
    }

cleanup:
    /* free memory */
    rd_buffer_free(data);
    rd_buffer_free(output);
}

int main() {
    /* example brainfuck code */
    const char code[] = "say hello world! +[-[<<[+[--->]-[<<<]]]>>>-]>-.---.>..>.<<<<-.<+.>>>>>.>.<<.<-.ok goodbye";
    const char optim_code[] = "++++[-]++";

    /* example bad code */
    const char bad_code1[] = "[[ oops no good";
    const char bad_code2[] = "oops no good ]]";

    /* try all */
    test_code(code, sizeof(code), 0);
    test_code(optim_code, sizeof(optim_code), 0);
    test_code(bad_code1, sizeof(bad_code1), 1);
    test_code(bad_code2, sizeof(bad_code2), 1);

    if (sizeof(struct budgie_op) != 2) {
        printf("warning: expecting sizeof(struct budgie_op) = 2, but it is %ld instead. memory usage may be increased.\n",
            sizeof(struct budgie_op));
    }

    return 0;
}
#endif

