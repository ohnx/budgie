#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <rudolph/buffer.h>
#include <rudolph/elf_link.h>
#include "ir.h"
#include "stack.h"

#define BUDGIE_MAX_CELLS 131072
#define c(x) ((const unsigned char *)(x))

static budgie_stack *loop_stack;

__inline static void budgie_translate_op_next(rd_buf_t **buf, unsigned char arg) {
    if (arg == 1) {
        /* inc rsi */
        rd_buffer_push(buf, c("\x48\xFF\xC6"), 3);
    } else if (arg <= 127) {
        /* add rsi, <number less than or equal to 127> */
        rd_buffer_push(buf, c("\x48\x83\xC6"), 3);
        rd_buffer_push(buf, &arg, 1);
    } else {
        /* add rsi, <number greater than 127> */
        rd_buffer_push(buf, c("\x48\x81\xC6"), 3);
        rd_buffer_push(buf, &arg, 1);
        /* we only have a maximum of 1 byte, so pad the rest with zero */
        rd_buffer_push(buf, c("\x00\x00\x00"), 3);
    }
}

__inline static void budgie_translate_op_prev(rd_buf_t **buf, unsigned char arg) {
    if (arg == 1) {
        /* dec rsi */
        rd_buffer_push(buf, c("\x48\xFF\xCE"), 3);
    } else if (arg <= 127) {
        /* sub rsi, <number less than or equal to 127> */
        rd_buffer_push(buf, c("\x48\x83\xEE"), 3);
        rd_buffer_push(buf, &arg, 1);
    } else {
        /* sub rsi, <number greater than 127> */
        rd_buffer_push(buf, c("\x48\x81\xEE"), 3);
        rd_buffer_push(buf, &arg, 1);
        /* we only have a maximum of 1 byte, so pad the rest with zero */
        rd_buffer_push(buf, c("\x00\x00\x00"), 3);
    }
}

__inline static void budgie_translate_op_incr(rd_buf_t **buf, unsigned char arg) {
    if (arg == 1) {
        /* incb [rsi] */
        rd_buffer_push(buf, c("\xFE\x06"), 2);
    } else {
        /* addb [rsi], <number less than or equal to 255> */
        rd_buffer_push(buf, c("\x80\x06"), 2);
        rd_buffer_push(buf, &arg, 1);
    }
}

__inline static void budgie_translate_op_decr(rd_buf_t **buf, unsigned char arg) {
    if (arg == 1) {
        /* decb [rsi] */
        rd_buffer_push(buf, c("\xFE\x0E"), 2);
    } else {
        /* subb [rsi], <number less than or equal to 255> */
        rd_buffer_push(buf, c("\x80\x2E"), 2);
        rd_buffer_push(buf, &arg, 1);
    }
}

__inline static void budgie_translate_op_out(rd_buf_t **buf, unsigned char arg) {
    /*
     * mov rax, r12 ; r12 is set to 1 initially - rax = 1 = sys_write
     * mov rdi, r12 ; r12 is set to 1 initially - rdi = 1 = stdout
     * mov rdx, r12 ; r12 is set to 1 initially - rdx = 1 = 1 byte
     * syscall ; rax = 1 = sys_write, rdi = 1 = stdout, rsi = address to write
     *         ; to, rdx = 1 byte
     */
    rd_buffer_push(buf, c("\x4C\x89\xE0" "\x4C\x89\xE7" "\x4C\x89\xE2" "\x0F\x05"), 11);
}

__inline static void budgie_translate_op_in(rd_buf_t **buf, unsigned char arg) {
    /*
     * mov rax, rbx ; rbx is set to 0 initially = 0 = sys_read
     * mov rdi, rbx ; rbx is set to 0 initially = 0 = stdin
     * mov rdx, r12 ; r12 is set to 1 initially - rdx = 1 = 1 byte
     * syscall ; rax = 0 = sys_read, rdi = 1 = stdin, rsi = address to write to,
     *         ; rdx = 1 byte
     */
    rd_buffer_push(buf, c("\x48\x89\xD8" "\x48\x89\xDF" "\x4C\x89\xE2" "\x0F\x05"), 11);
}

__inline static void budgie_translate_op_lopen(rd_buf_t **buf, unsigned char arg) {
    /*
     * cmp bh, [rsi] ; check if the cell is 0
     * je $+0x00000000 ; jump to a different memory address if the cell is 0
     */
    /* so technically there is a possibility to use a short jump instead
     * of a near jump here, but that reaalllly complicates things (probably need
     * 2 passes of the code) so i'll force near jumps always instead. */
    /* for now the jump destination will be 0 */
    rd_buffer_push(buf, c("\x3A\x3E" "\x0F\x84\x00\x00\x00\x00"), 8);

    /* also push the location of the jump so we can overwrite later */
    budgie_stack_push(loop_stack, (void *)(intptr_t)((*buf)->len));
}

__inline static void budgie_translate_op_lclos(rd_buf_t **buf, unsigned char arg) {
    size_t lopen_pos;
    int32_t diff;
    /*
     * cmp bh, [rsi] ; check if the cell is 0
     * jne $+0x00000000 ; jump to a different memory address if the cell is 0
     */
    /* so technically there is a possibility to use a short jump instead
     * of a near jump here, but for consistency with lopen, i'll force near
     * jumps always instead. */
    rd_buffer_push(buf, c("\x3A\x3E" "\x0F\x85"), 4);

    /* get the location of the lopen */
    lopen_pos = (size_t)(intptr_t)budgie_stack_pop(loop_stack);

    /* find the difference and make the jump go to the end of that place */
    diff = lopen_pos - (*buf)->len - 4;
    rd_buffer_push(buf, (unsigned char *)(intptr_t)(&diff), 4);

    /* also overwrite the old jump in the lopen that was blank */
    *((int32_t *)(rd_buffer_data(*buf) + lopen_pos - 4)) = (*buf)->len - lopen_pos;
}

__inline static void budgie_translate_op_set(rd_buf_t **buf, unsigned char arg) {
    /* movb [rsi], <number to set to> */
    rd_buffer_push(buf, c("\xC6\x06"), 2);
    rd_buffer_push(buf, &arg, 1);
}

int budgie_translate_x86_64_linux(rd_buf_t *in, rd_buf_t **out) {
    size_t i, ninstrs;
    rd_buf_t *code, *data;
    struct rd_elf_link_relocation relocs[2];
    struct budgie_op *ops;
    int rc;

    /* initialization */
    code = rd_buffer_init();
    loop_stack = budgie_stack_new();
    ops = (struct budgie_op *)rd_buffer_data(in);
    ninstrs = in->len / sizeof(struct budgie_op);

    /* preamble (same code for everything) */

    /* TODO: remove development comments
    pseudocode:

    allocate enough memory for the program (maybe use bss??)
    set rsi to the start of the memory (zeroed out)
    clear r12
    clear rbx
    increment r12
    set r13 to 0x3C (60)

    rsp,rbp, rbx, r12, r13, r14, and r15 are protected

    rsi is the pointer to the memory space
    rdx is always 1
    rdi is sometimes 1, othertimes 0 (copy it from r10 or r11)
    0 = read (r10)
    1 = write (r11)
    60 = exit (r12)
    
    */

    /* set rsi to the start of the memory (movabs rsi, 0x00)
     * (will be relocated to start of bss segment later) */
    rd_buffer_push(&code, c("\x48\xBE\x00\x00\x00\x00\x00\x00\x00\x00"), 10);
    relocs[0].type = RELOC_BSS64;
    relocs[0].target = 2;
    relocs[0].src = 0; /* can customize where to start from here (allowing for 
                        * moving the cell pointer backwards */
    relocs[1].type = RELOC_NULL;

    /* set r12 and rbx to the correct values */
    /* xor rbx, rbx
     * xor r12, r12
     * inc r12
     */
    rd_buffer_push(&code, c("\x48\x31\xDB" "\x4D\x31\xE4" "\x49\xFF\xC4"), 9);

    /* translate each instruction */
    /* TODO: remove development comments
    pseudocode:

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

    for (i = 0; i < ninstrs; i++) {
        /* translate each instruction */
        switch (ops->type) {
        case BOPT_P_NEXT:   budgie_translate_op_next(&code, ops->arg);  break;
        case BOPT_P_PREV:   budgie_translate_op_prev(&code, ops->arg);  break;
        case BOPT_D_INCR:   budgie_translate_op_incr(&code, ops->arg);  break;
        case BOPT_D_DECR:   budgie_translate_op_decr(&code, ops->arg);  break;
        case BOPT_D_OUT:    budgie_translate_op_out(&code, ops->arg);   break;
        case BOPT_D_IN:     budgie_translate_op_in(&code, ops->arg);    break;
        case BOPT_F_LOPEN:  budgie_translate_op_lopen(&code, ops->arg); break;
        case BOPT_F_LCLOS:  budgie_translate_op_lclos(&code, ops->arg); break;
        case BOPT_D_SET:    budgie_translate_op_set(&code, ops->arg);   break;
        default: break;
        }
        ops++;
    }

    /* epilogue */
    /* exit with code 0 */
    /* mov rax, 0x3c ; (60 = exit)
     * mov rdi, rbx ; rbx is set to 0 and rdi = 0 = exit success
     * syscall
     */
    rd_buffer_push(&code, c("\x48\xC7\xC0\x3C\x00\x00\x00" "\x48\x89\xDF" "\x0F\x05"), 12);

    /* no data for this; only bss */
    data = rd_buffer_init();

    /* final linking */
    rc = rd_elf_link64(RD_ELFHDR_MACHINE_X86_64, code, data, BUDGIE_MAX_CELLS, relocs, out);

    /* done */
    rd_buffer_free(code);
    rd_buffer_free(data);
    budgie_stack_destroy(loop_stack);
    return rc;
}
