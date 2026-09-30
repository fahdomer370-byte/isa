#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "isa_core.h"

static void print_usage(const char *program)
{
    printf("ISA Programming Language\n");
    printf("Usage:\n");
    printf("  %s <file.isa>     Run ISA source\n", program);
    printf("  %s <file.isac>    Run compiled ISA bytecode\n", program);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *filename = argv[1];

    if (filename == NULL || filename[0] == '\0') {
        fprintf(stderr, "ISA Error: invalid file name\n");
        return 1;
    }

    size_t len = strlen(filename);

    if (len >= 5 &&
        strcmp(filename + len - 5, ".isac") == 0) {

        if (isa_run_bytecode(filename) != 0) {
            fprintf(stderr, "ISA Error: failed to run bytecode: %s\n",
                    filename);
            return 1;
        }

        return 0;
    }

    if (len >= 4 &&
        strcmp(filename + len - 4, ".isa") == 0) {

        if (isa_execute_file(filename) != 0) {
            fprintf(stderr, "ISA Error: failed to execute: %s\n",
                    filename);
            return 1;
        }

        return 0;
    }

    fprintf(stderr,
            "ISA Error: unsupported file type: %s\n"
            "Expected .isa or .isac\n",
            filename);

    return 1;
}