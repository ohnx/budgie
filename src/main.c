#define _POSIX_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <rudolph/buffer.h>
#include <rudolph/elf.h>
#include <rudolph/elf_link.h>
#include "ir.h"

#define READBUF_SZ  65536

extern int budgie_translate_x86_64_linux(rd_buf_t *in, rd_buf_t **out);

int main(int argc, char **argv) {
    /* todo - better input */
    rd_buf_t *iput_buf = NULL, *ir = NULL, *final = NULL;
    int rc;
    char read_buf[READBUF_SZ];
    FILE *fp;
    struct stat statinfo;
    const char *out_name;

    while (fgets(read_buf, READBUF_SZ, stdin)) {
        rd_buffer_push(&iput_buf, (unsigned char *)read_buf, strlen(read_buf));
    }

    /* "compile" the code to an IR */
    rc = budgie_oplist_create(iput_buf, &ir);
    if (rc != 0) {
        fprintf(stderr, "Syntax error in input!\n");
        goto cleanup;
    }

    /* optimize */
    budgie_oplist_optimize(&ir);

    /* temporary - force x86_64_linux */
    rc = budgie_translate_x86_64_linux(ir, &final);

    if (rc) {
        fprintf(stderr, "Error %d occurred :(\n", rc);
        goto cleanup;
    }

    /* write output file */
    if (isatty(fileno(stdout))) {
        /* open the file to write output to */
        out_name = argc == 2 ? argv[1] : "a.out";
        fp = fopen(out_name, "wb");
        if (!fp) {
            fprintf(stderr, "Error writing output!\n");
            goto cleanup;
        }

        /* write file */
        fwrite(rd_buffer_data(final), final->len, sizeof(unsigned char), fp);

        /* make output executable */
        stat(out_name, &statinfo);
        statinfo.st_mode |= S_IXUSR | S_IXGRP | S_IXOTH;
        chmod(out_name, statinfo.st_mode);

        fclose(fp);
    } else {
        /* write to stdout */
        if (argc == 2)
            fprintf(stderr, "Ignoring output file given and printing to stdout\n");

        fwrite(rd_buffer_data(final), final->len, sizeof(unsigned char), stdout);
    }

    /* no errors */
    rc = 0;

cleanup:
    rd_buffer_free(iput_buf);
    rd_buffer_free(ir);
    rd_buffer_free(final);

    return rc;
}
