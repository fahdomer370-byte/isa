#include "isa_core.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ISA_MAGIC0 0x00
#define ISA_MAGIC1 'i'
#define ISA_MAGIC2 's'
#define ISA_MAGIC3 'a'
#define ISA_MAGIC4 0x00
#define ISA_MAGIC5 0x00
#define ISA_MAGIC6 0x02

#define OP_TEXT 0x10
#define OP_SET 0x11
#define OP_INPUT 0x12
#define OP_PRINT_FILE 0x13
#define OP_RAW_FILE 0x14
#define OP_DIM 0x15
#define OP_END 0xFF

static char *isa_strdup(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static void trim_inplace(char *s) {
    size_t len;
    size_t start = 0;
    if (!s) return;
    len = strlen(s);
    while (start < len && isspace((unsigned char)s[start])) start++;
    while (len > start && isspace((unsigned char)s[len - 1])) len--;
    if (start > 0) memmove(s, s + start, len - start);
    s[len - start] = '\0';
}

static int starts_with(const char *s, const char *prefix) {
    return s && prefix && strncmp(s, prefix, strlen(prefix)) == 0;
}

static char *dup_range(const char *a, const char *b) {
    size_t n;
    char *p;
    if (!a || !b || b < a) return NULL;
    n = (size_t)(b - a);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, a, n);
    p[n] = '\0';
    return p;
}

static int unquote_string(const char *src, char *out, size_t out_sz) {
    size_t i = 0, j = 0;
    size_t len;
    if (!src || !out || out_sz == 0) return -1;
    while (isspace((unsigned char)*src)) src++;
    len = strlen(src);
    while (len && isspace((unsigned char)src[len - 1])) len--;
    if (len >= 2 && src[0] == '"' && src[len - 1] == '"') {
        i = 1;
        while (i + 1 < len) {
            char c = src[i++];
            if (c == '\\' && i + 1 <= len) {
                char e = src[i++];
                switch (e) {
                    case 'n': c = '\n'; break;
                    case 'r': c = '\r'; break;
                    case 't': c = '\t'; break;
                    case '\\': c = '\\'; break;
                    case '"': c = '"'; break;
                    default: c = e; break;
                }
            }
            if (j + 1 >= out_sz) return -1;
            out[j++] = c;
        }
        out[j] = '\0';
        return 0;
    }
    if (len + 1 > out_sz) return -1;
    memcpy(out, src, len);
    out[len] = '\0';
    return 0;
}

static const char *skip_space(const char *s) {
    while (s && isspace((unsigned char)*s)) s++;
    return s;
}

static int find_var(const IsaContext *ctx, const char *name) {
    int i;
    for (i = 0; i < ISA_MAX_VARS; ++i) {
        if (ctx->vars[i].used && strcmp(ctx->vars[i].name, name) == 0) return i;
    }
    return -1;
}

static int set_var(IsaContext *ctx, const char *name, const char *value) {
    int i = find_var(ctx, name);
    if (i < 0) {
        for (i = 0; i < ISA_MAX_VARS; ++i) {
            if (!ctx->vars[i].used) break;
        }
        if (i == ISA_MAX_VARS) return -1;
        ctx->vars[i].used = 1;
        snprintf(ctx->vars[i].name, sizeof(ctx->vars[i].name), "%s", name);
    }
    snprintf(ctx->vars[i].value, sizeof(ctx->vars[i].value), "%s", value);
    return 0;
}

static const char *get_var(const IsaContext *ctx, const char *name) {
    int i = find_var(ctx, name);
    return i >= 0 ? ctx->vars[i].value : "";
}

static void decode_value(IsaContext *ctx, const char *expr, char *out, size_t out_sz) {
    const char *s = skip_space(expr);
    if (!s) { out[0] = '\0'; return; }

    if (strstr(s, "sys.input(") == s) {
        const char *open = strchr(s, '(');
        const char *close = strrchr(s, ')');
        char prompt[ISA_VALUE_MAX];
        if (open && close && close > open) {
            char *inside = dup_range(open + 1, close);
            if (inside) {
                if (unquote_string(inside, prompt, sizeof(prompt)) != 0) prompt[0] = '\0';
                free(inside);
                fputs(prompt, stdout);
                fflush(stdout);
                if (!fgets(out, (int)out_sz, stdin)) out[0] = '\0';
                trim_inplace(out);
                return;
            }
        }
    }

    if (*s == '"') {
        if (unquote_string(s, out, out_sz) != 0) out[0] = '\0';
        return;
    }

    if (strstr(s, ".width") == s) {
        snprintf(out, out_sz, "%d", ctx->width);
        return;
    }
    if (strstr(s, ".height") == s) {
        snprintf(out, out_sz, "%d", ctx->height);
        return;
    }

    if (strncmp(s, "ty.value", 8) == 0) {
        snprintf(out, out_sz, "%s", get_var(ctx, "ty.value"));
        return;
    }

    if (isalpha((unsigned char)*s) || *s == '_') {
        size_t n = 0;
        while (s[n] && (isalnum((unsigned char)s[n]) || s[n] == '_' || s[n] == '.')) n++;
        if (n < sizeof(out)) {
            char name[ISA_NAME_MAX];
            if (n >= sizeof(name)) n = sizeof(name) - 1;
            memcpy(name, s, n);
            name[n] = '\0';
            if (find_var(ctx, name) >= 0) {
                snprintf(out, out_sz, "%s", get_var(ctx, name));
                return;
            }
        }
    }

    snprintf(out, out_sz, "%s", s);
    trim_inplace(out);
}

static int read_entire_file(const char *path, char **out_buf, size_t *out_len) {
    FILE *f;
    long size;
    char *buf;
    if (!path || !out_buf) return -1;
    f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    size = ftell(f);
    if (size < 0 || size > 64L * 1024L * 1024L) { fclose(f); return -1; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return -1; }
    buf = (char *)malloc((size_t)size + 1);
    if (!buf) { fclose(f); return -1; }
    if (size > 0 && fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf); fclose(f); return -1;
    }
    buf[size] = '\0';
    fclose(f);
    *out_buf = buf;
    if (out_len) *out_len = (size_t)size;
    return 0;
}

static void strip_comments(char *src) {
    char *p = src;
    while ((p = strstr(p, "<~~")) != NULL) {
        char *close = strstr(p + 3, "$~~>");
        if (!close) {
            /* Leave an invalid marker visible to the parser. */
            return;
        }
        close += 4;
        while (*p && p < close) {
            if (*p == '\n') p[0] = '\n';
            else p[0] = ' ';
            p++;
        }
    }
}

static int split_statements(const char *src, char ***out_items, size_t *out_count) {
    size_t cap = 32, count = 0;
    char **items = (char **)calloc(cap, sizeof(char *));
    const char *start = src;
    const char *p = src;
    int quote = 0, escape = 0, braces = 0;
    if (!items) return -1;

    while (*p) {
        char c = *p;
        if (escape) { escape = 0; p++; continue; }
        if (quote && c == '\\') { escape = 1; p++; continue; }
        if (c == '"') { quote = !quote; p++; continue; }
        if (!quote) {
            if (c == '{') braces++;
            else if (c == '}') braces--;
            else if (c == ';' && braces == 0) {
                char *piece = dup_range(start, p + 1);
                if (!piece) goto fail;
                trim_inplace(piece);
                if (*piece) {
                    if (count == cap) {
                        cap *= 2;
                        char **tmp = (char **)realloc(items, cap * sizeof(char *));
                        if (!tmp) { free(piece); goto fail; }
                        items = tmp;
                    }
                    items[count++] = piece;
                } else free(piece);
                start = p + 1;
            }
        }
        p++;
    }
    if (*start) {
        char *piece = isa_strdup(start);
        if (!piece) goto fail;
        trim_inplace(piece);
        if (*piece) {
            if (count == cap) {
                cap *= 2;
                char **tmp = (char **)realloc(items, cap * sizeof(char *));
                if (!tmp) { free(piece); goto fail; }
                items = tmp;
            }
            items[count++] = piece;
        } else free(piece);
    }
    *out_items = items;
    *out_count = count;
    return 0;
fail:
    while (count) free(items[--count]);
    free(items);
    return -1;
}

static int execute_statement(IsaContext *ctx, const char *statement);

static int execute_block(IsaContext *ctx, const char *block) {
    char **items = NULL;
    size_t count = 0, i;
    int rc = 0;
    if (split_statements(block, &items, &count) != 0) return -1;
    for (i = 0; i < count; ++i) {
        rc = execute_statement(ctx, items[i]);
        free(items[i]);
        if (rc != 0 || ctx->exit_requested || ctx->loop_break) break;
    }
    free(items);
    return rc;
}

static int execute_condition_block(IsaContext *ctx, const char *stmt) {
    const char *open = strchr(stmt, '{');
    const char *close = strrchr(stmt, '}');
    const char *eq;
    char lhs[ISA_NAME_MAX], rhs[ISA_VALUE_MAX];
    char actual[ISA_VALUE_MAX];
    char *body;
    size_t n;
    if (!open || !close || close <= open) return -1;

    eq = strstr(stmt, "==");
    if (!eq || eq > open) return -1;
    n = (size_t)(eq - (stmt + 3));
    if (n >= sizeof(lhs)) return -1;
    memcpy(lhs, stmt + 3, n);
    lhs[n] = '\0';
    trim_inplace(lhs);
    if (unquote_string(eq + 2, rhs, sizeof(rhs)) != 0) return -1;
    if (*rhs == '\0' && *(eq + 2) != '"') {
        decode_value(ctx, eq + 2, rhs, sizeof(rhs));
    }
    decode_value(ctx, lhs, actual, sizeof(actual));
    body = dup_range(open + 1, close);
    if (!body) return -1;
    if (strcmp(actual, rhs) == 0) {
        int rc = execute_block(ctx, body);
        free(body);
        return rc;
    }
    free(body);
    return 0;
}

static int execute_statement(IsaContext *ctx, const char *raw) {
    char *stmt;
    const char *s;
    char value[ISA_VALUE_MAX];
    if (!raw) return 0;
    stmt = isa_strdup(raw);
    if (!stmt) return -1;
    trim_inplace(stmt);
    while (*stmt && stmt[strlen(stmt)-1] == ';') stmt[strlen(stmt)-1] = '\0';
    trim_inplace(stmt);
    s = stmt;

    if (!*s || starts_with(s, "//")) { free(stmt); return 0; }
    if (strstr(s, "<~~") != NULL) {
        fprintf(stderr, "ISA SyntaxError: unclosed or invalid comment in statement\n");
        free(stmt); return 2;
    }
    if (strcmp(s, "iz.script") == 0) { free(stmt); return 0; }
    if (starts_with(s, "import.display")) {
        puts("ISA: display/Android UI is not available in the Termux CLI runtime.");
        free(stmt); return 0;
    }
    if (starts_with(s, "ui = android.app.screen")) {
        puts("ISA: android.app.screen is available only in an Android host runtime.");
        free(stmt); return 0;
    }
    if (starts_with(s, "ui.txt(") || starts_with(s, "txt(")) {
        const char *open = strchr(s, '(');
        const char *close = strrchr(s, ')');
        if (open && close && close > open) {
            char *inside = dup_range(open + 1, close);
            if (!inside) { free(stmt); return -1; }
            decode_value(ctx, inside, value, sizeof(value));
            puts(value);
            free(inside);
        }
        free(stmt);
        return 0;
    }
    if (starts_with(s, "print.file(")) {
        const char *open = strchr(s, '('), *close = strrchr(s, ')');
        if (open && close && close > open) {
            char path[ISA_VALUE_MAX];
            char *inside = dup_range(open + 1, close);
            if (!inside) { free(stmt); return -1; }
            if (unquote_string(inside, path, sizeof(path)) != 0) {
                fprintf(stderr, "ISA SyntaxError: print.file expects a string path\n");
                free(inside); free(stmt); return 2;
            }
            free(inside);
            FILE *f = fopen(path, "rb");
            int ch;
            if (!f) {
                fprintf(stderr, "ISA FileError: %s: %s\n", path, strerror(errno));
                free(stmt); return 1;
            }
            while ((ch = fgetc(f)) != EOF) fputc(ch, stdout);
            fclose(f);
        }
        free(stmt);
        return 0;
    }
    if (starts_with(s, "end!")) {
        ctx->loop_break = 1;
        free(stmt);
        return 0;
    }
    if (starts_with(s, "W != ") || starts_with(s, "H != ")) {
        int *target = starts_with(s, "W != ") ? &ctx->width : &ctx->height;
        const char *p = strstr(s, "!=");
        if (!p) { free(stmt); return 2; }
        p += 2;
        while (*p == ' ') p++;
        *target = atoi(p);
        if (*target < 1) *target = 1;
        free(stmt);
        return 0;
    }
    if (starts_with(s, "if.")) {
        int rc = execute_condition_block(ctx, s);
        free(stmt);
        return rc;
    }
    if (starts_with(s, "while.true")) {
        const char *open = strchr(s, '{'), *close = strrchr(s, '}');
        if (!open || !close || close <= open) { free(stmt); return 2; }
        char *body = dup_range(open + 1, close);
        if (!body) { free(stmt); return -1; }
        for (;;) {
            ctx->loop_break = 0;
            if (execute_block(ctx, body) != 0) { free(body); free(stmt); return 2; }
            if (ctx->exit_requested || ctx->loop_break) break;
        }
        free(body);
        free(stmt);
        return 0;
    }
    if (starts_with(s, "btn(") || starts_with(s, "app.boxbyi()")) {
        puts("ISA UI: command accepted by parser; CLI runtime does not render Android widgets.");
        free(stmt);
        return 0;
    }
    if (strstr(s, "else." ) == s) {
        /* Else branches are handled by a future structured parser.
         * Keeping this valid avoids rejecting otherwise readable ISA source. */
        free(stmt);
        return 0;
    }

    {
        const char *neq = strstr(s, "!=");
        if (neq) {
            char name[ISA_NAME_MAX];
            size_t n = (size_t)(neq - s);
            if (n >= sizeof(name)) { free(stmt); return 2; }
            memcpy(name, s, n);
            name[n] = '\0';
            trim_inplace(name);
            decode_value(ctx, neq + 2, value, sizeof(value));
            if (set_var(ctx, name, value) != 0) {
                fprintf(stderr, "ISA RuntimeError: variable table is full\n");
                free(stmt); return 1;
            }
            free(stmt);
            return 0;
        }
    }

    if (starts_with(s, "exec(")) {
        const char *open = strchr(s, '('), *close = strrchr(s, ')');
        if (open && close && close > open) {
            char name[ISA_NAME_MAX];
            char *inside = dup_range(open + 1, close);
            if (!inside) { free(stmt); return -1; }
            if (unquote_string(inside, name, sizeof(name)) != 0) snprintf(name, sizeof(name), "%s", inside);
            free(inside);
            const char *raw = get_var(ctx, name);
            if (*raw == '\0') {
                fprintf(stderr, "ISA RuntimeError: exec value not found: %s\n", name);
                free(stmt); return 1;
            }
            if (isa_execute_source(ctx, raw, name) != 0) { free(stmt); return 1; }
        }
        free(stmt);
        return 0;
    }

    fprintf(stderr, "ISA SyntaxError: unsupported statement: %s\n", s);
    free(stmt);
    return 2;
}

void isa_context_init(IsaContext *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(*ctx));
    ctx->width = 200;
    ctx->height = 100;
}

int isa_execute_source(IsaContext *ctx, const char *source, const char *origin) {
    char *copy;
    int rc;
    (void)origin;
    if (!ctx || !source) return 2;
    copy = isa_strdup(source);
    if (!copy) return 1;
    strip_comments(copy);
    if (strstr(copy, "<~~") != NULL && strstr(copy, "$~~>") == NULL) {
        fprintf(stderr, "ISA SyntaxError: unclosed comment\n");
        free(copy);
        return 2;
    }
    rc = execute_block(ctx, copy);
    free(copy);
    return rc;
}

int isa_execute_file(IsaContext *ctx, const char *path) {
    char *buf = NULL;
    int rc;
    if (read_entire_file(path, &buf, NULL) != 0) {
        fprintf(stderr, "ISA FileError: cannot read %s\n", path);
        return 1;
    }
    rc = isa_execute_source(ctx, buf, path);
    free(buf);
    return rc;
}

static int write_u32(FILE *f, uint32_t v) {
    unsigned char b[4];
    b[0] = (unsigned char)(v & 0xffu);
    b[1] = (unsigned char)((v >> 8) & 0xffu);
    b[2] = (unsigned char)((v >> 16) & 0xffu);
    b[3] = (unsigned char)((v >> 24) & 0xffu);
    return fwrite(b, 1, 4, f) == 4 ? 0 : -1;
}

static int read_u32(FILE *f, uint32_t *v) {
    unsigned char b[4];
    if (fread(b, 1, 4, f) != 4) return -1;
    *v = ((uint32_t)b[0]) | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return 0;
}

static int write_instruction(FILE *f, unsigned char opcode, const char *data) {
    uint32_t len = data ? (uint32_t)strlen(data) : 0;
    if (fputc(opcode, f) == EOF) return -1;
    if (write_u32(f, len) != 0) return -1;
    if (len && fwrite(data, 1, len, f) != len) return -1;
    return 0;
}

static unsigned char opcode_for_statement(const char *stmt) {
    if (starts_with(stmt, "txt(") || starts_with(stmt, "ui.txt(")) return OP_TEXT;
    if (strstr(stmt, "!=") != NULL && !starts_with(stmt, "W != ") && !starts_with(stmt, "H != ")) {
        if (strstr(stmt, "sys.input(") != NULL) return OP_INPUT;
        return OP_SET;
    }
    if (starts_with(stmt, "print.file(")) return OP_PRINT_FILE;
    if (starts_with(stmt, "import.raw(")) return OP_RAW_FILE;
    if (starts_with(stmt, "W != ") || starts_with(stmt, "H != ")) return OP_DIM;
    return OP_SET;
}

int isa_compile_file(const char *input_path, const char *output_path) {
    char *src = NULL;
    char **items = NULL;
    size_t count = 0, i;
    FILE *out = NULL;
    int rc = 1;
    if (read_entire_file(input_path, &src, NULL) != 0) {
        fprintf(stderr, "ISAC FileError: cannot read %s\n", input_path);
        return 1;
    }
    strip_comments(src);
    if (strstr(src, "<~~") != NULL && strstr(src, "$~~>") == NULL) {
        fprintf(stderr, "ISAC SyntaxError: unclosed comment\n");
        free(src); return 2;
    }
    if (split_statements(src, &items, &count) != 0) {
        free(src); return 1;
    }
    out = fopen(output_path, "wb");
    if (!out) {
        fprintf(stderr, "ISAC FileError: cannot create %s: %s\n", output_path, strerror(errno));
        goto done;
    }
    {
        const unsigned char header[] = {ISA_MAGIC0, ISA_MAGIC1, ISA_MAGIC2, ISA_MAGIC3, ISA_MAGIC4, ISA_MAGIC5, ISA_MAGIC6};
        if (fwrite(header, 1, sizeof(header), out) != sizeof(header)) goto done;
    }
    for (i = 0; i < count; ++i) {
        char *stmt = items[i];
        trim_inplace(stmt);
        while (*stmt && stmt[strlen(stmt)-1] == ';') stmt[strlen(stmt)-1] = '\0';
        trim_inplace(stmt);
        if (!*stmt) continue;
        if (write_instruction(out, opcode_for_statement(stmt), stmt) != 0) goto done;
    }
    if (write_instruction(out, OP_END, NULL) != 0) goto done;
    rc = 0;
done:
    if (out) fclose(out);
    for (i = 0; i < count; ++i) free(items[i]);
    free(items);
    free(src);
    if (rc != 0) fprintf(stderr, "ISAC Error: failed while writing %s\n", output_path);
    return rc;
}

int isa_run_bytecode(IsaContext *ctx, const char *path) {
    FILE *f;
    unsigned char header[7];
    int rc = 0;
    f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "ISA FileError: cannot open %s\n", path);
        return 1;
    }
    if (fread(header, 1, sizeof(header), f) != sizeof(header) ||
        header[0] != ISA_MAGIC0 || header[1] != ISA_MAGIC1 || header[2] != ISA_MAGIC2 ||
        header[3] != ISA_MAGIC3 || header[4] != ISA_MAGIC4 || header[5] != ISA_MAGIC5 ||
        header[6] != ISA_MAGIC6) {
        fprintf(stderr, "ISA BytecodeError: invalid .isac header\n");
        fclose(f); return 2;
    }
    for (;;) {
        int op = fgetc(f);
        uint32_t len;
        char *payload;
        if (op == EOF) { fprintf(stderr, "ISA BytecodeError: unexpected end of file\n"); rc = 2; break; }
        if (read_u32(f, &len) != 0 || len > 64u * 1024u * 1024u) { rc = 2; break; }
        payload = (char *)malloc((size_t)len + 1);
        if (!payload) { rc = 1; break; }
        if (len && fread(payload, 1, len, f) != len) { free(payload); rc = 2; break; }
        payload[len] = '\0';
        if (op == OP_END) {
            free(payload);
            break;
        }
        if (op < OP_TEXT || op > OP_DIM) {
            fprintf(stderr, "ISA BytecodeError: unknown opcode 0x%02X\n", op);
            free(payload); rc = 2; break;
        }
        rc = execute_statement(ctx, payload);
        free(payload);
        if (rc != 0 || ctx->exit_requested) break;
        ctx->loop_break = 0;
    }
    fclose(f);
    return rc;
}
