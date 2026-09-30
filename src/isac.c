#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "isa_core.h"

static void print_usage(const char *program)
{
    printf("ISA Compiler\n");
    printf("Usage:\n");
    printf("  %s <file.isa>\n", program);
}

static int make_output_name(const char *input, char *output, size_t size)
{
    size_t len = strlen(input);

    if (len < 4 || strcmp(input + len - 4, ".isa") != 0) {
        return 0;
    }

    if (len - 4 + 5 + 1 > size) {
        return 0;
    }

    memcpy(output, input, len - 4);
    memcpy(output + len - 4, ".isac", 6);

    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *input = argv[1];

    if (input == NULL || input[0] == '\0') {
        fprintf(stderr, "ISA Compiler Error: invalid input file\n");
        return 1;
    }

    char output[4096];

    if (!make_output_name(input, output, sizeof(output))) {
        fprintf(stderr,
                "ISA Compiler Error: input must be a .isa file\n");
        return 1;
    }

    FILE *file = fopen(input, "rb");

    if (file == NULL) {
        fprintf(stderr,
                "ISA Compiler Error: cannot open '%s'\n",
                input);
        return 1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        fprintf(stderr, "ISA Compiler Error: cannot read file\n");
        return 1;
    }

    long file_size = ftell(file);

    if (file_size < 0) {
        fclose(file);
        fprintf(stderr, "ISA Compiler Error: cannot get file size\n");
        return 1;
    }

    rewind(file);

    char *source = malloc((size_t)file_size + 1);

    if (source == NULL) {
        fclose(file);
        fprintf(stderr,
                "ISA Compiler Error: out of memory\n");
        return 1;
    }

    size_t read_size =
        fread(source, 1, (size_t)file_size, file);

    fclose(file);

    source[read_size] = '\0';

    if (isa_compile_source(source, output) != 0) {
        free(source);

        fprintf(stderr,
                "ISA Compiler Error: compilation failed\n");

        return 1;
    }

    free(source);

    printf("ISA compiled successfully:\n");
    printf("  Source : %s\n", input);
    printf("  Output : %s\n", output);

    return 0;
}