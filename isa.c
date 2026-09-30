#include "isa_core.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <main.isa|main.isac>\n", prog);
}

int main(int argc, char **argv) {
    IsaContext ctx;
    const char *path;
    int rc;
    if (argc != 2) {
        usage(argv[0]);
        return 64;
    }
    path = argv[1];
    isa_context_init(&ctx);
    if (strcmp(path + (strlen(path) > 5 ? strlen(path) - 5 : 0), ".isac") == 0) {
        rc = isa_run_bytecode(&ctx, path);
    } else {
        rc = isa_execute_file(&ctx, path);
    }
    return rc;
}
