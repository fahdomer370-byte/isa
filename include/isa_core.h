#ifndef ISA_CORE_H
#define ISA_CORE_H

#include <stddef.h>

#define ISA_VERSION "0.1.0"
#define ISA_MAX_VARS 256
#define ISA_NAME_MAX 128
#define ISA_VALUE_MAX 4096

typedef struct {
    char name[ISA_NAME_MAX];
    char value[ISA_VALUE_MAX];
    int used;
} IsaVar;

typedef struct {
    IsaVar vars[ISA_MAX_VARS];
    int exit_requested;
    int loop_break;
    int width;
    int height;
} IsaContext;

void isa_context_init(IsaContext *ctx);
int isa_execute_source(IsaContext *ctx, const char *source, const char *origin);
int isa_execute_file(IsaContext *ctx, const char *path);
int isa_compile_file(const char *input_path, const char *output_path);
int isa_run_bytecode(IsaContext *ctx, const char *path);

#endif
