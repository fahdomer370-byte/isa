#include "isa_core.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <source.isa>\n", prog);
}

static int has_suffix(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && strcmp(s + a - b, suffix) == 0;
}

int main(int argc, char **argv) {
    char out[4096];
    size_t n;
    if (argc != 2) {
        usage(argv[0]);
        return 64;
    }
    if (!has_suffix(argv[1], ".isa")) {
        fprintf(stderr, "ISAC InputError: source file must end with .isa\n");
        return 65;
    }
    n = strlen(argv[1]);
    if (n - 4 + 5 >= sizeof(out)) {
        fprintf(stderr, "ISAC InputError: path is too long\n");
        return 65;
    }
    memcpy(out, argv[1], n - 4);
    strcpy(out + n - 4, ".isac");
    if (isa_compile_file(argv[1], out) != 0) return 1;
    printf("ISAC: %s -> %s\n", argv[1], out);
    return 0;
}
