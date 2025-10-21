// Rudra Tushir 231ADB234
// https://github.com/T-rudra/GradedLab1
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o bin/calc src/calc.c -lm
// General Instructions: all txt files should be in the same folder as calc.c
// Output files will be written to "Tasks" folder created alongside the executable
// Code is compiled in the bin folder


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* --------- Types and globals --------- */

typedef enum {
    TOK_EOF,
    TOK_NUM,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MUL,
    TOK_DIV,
    TOK_POW,   // '**'
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_ERR
} TokenType;

typedef struct {
    TokenType type;
    size_t start;   // start index in buffer (0-based)
    size_t len;     // length of token text
} Token;

/* Value holds parsed numeric values: double + integer cache + is_int flag */
typedef struct {
    double d;
    long long i;
    int is_int;
    size_t pos; /* token start index in buffer */
} Value;

/* File buffer and tokenizer state */
static char *g_buf = NULL;
static size_t g_len = 0;
static size_t g_i = 0; /* current index into buffer (0-based) */
static Token g_tok;
static int g_have_error = 0;
static size_t g_error_pos = 0; /* 0-based index where first error occurred */

static const double EPS_INT = 1e-12;

/* Student identity for output filenames */
static const char *STU_NAME = "Rudra";
static const char *STU_LAST = "Tushir";
static const char *STU_ID = "231ADB234";

/* --------- Helpers --------- */

static void error_at(size_t pos) {
    if (!g_have_error) {
        g_have_error = 1;
        g_error_pos = pos;
    }
}

/* Local strdup to avoid feature-macro/platform implicit-decl issues */
static char *my_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* Local rounding helper to avoid depending on llround symbol in some toolchains */
static long long my_llround(double x) {
    if (isnan(x)) return 0;
    if (x >= 0.0) return (long long)(x + 0.5);
    return (long long)(x - 0.5);
}

/* Peek ahead n characters (0 returns current) */
static char peek_char(size_t n) {
    size_t p = g_i + n;
    if (p >= g_len) return '\0';
    return g_buf[p];
}

/* Advance by n characters */
static void advance(size_t n) {
    g_i += n;
}

/* --------- Tokenizer fixed errors from reddit forums and Ai*/

/* Produce next token and advance g_i appropriately */
static Token get_token_internal(void) {
    Token t;
    t.type = TOK_EOF;
    t.start = g_i;
    t.len = 0;

    /* Skip whitespace (newlines count toward positions) */
    while (g_i < g_len && isspace((unsigned char)g_buf[g_i])) {
        g_i++;
    }

    /* If '#' is the first non-whitespace on the line, skip whole line (comments) */
    if (g_i < g_len && g_buf[g_i] == '#') {
        /* check if there is any non-whitespace after the previous newline up to here */
        size_t k = g_i;
        int found_nonws = 0;
        while (k > 0) {
            char prev = g_buf[k - 1];
            if (prev == '\n') break;
            if (!isspace((unsigned char)prev)) { found_nonws = 1; break; }
            k--;
        }
        if (!found_nonws) {
            while (g_i < g_len && g_buf[g_i] != '\n') g_i++;
            if (g_i < g_len && g_buf[g_i] == '\n') g_i++;
            return get_token_internal();
        }
    }

    t.start = g_i;
    if (g_i >= g_len) {
        t.type = TOK_EOF;
        t.len = 0;
        return t;
    }

    char c = g_buf[g_i];

    /* Number with optional attached sign */
    if ((c == '+' || c == '-') && (isdigit((unsigned char)peek_char(1)) || peek_char(1) == '.')) {
        char *startp = &g_buf[g_i];
        char *endp = NULL;
        (void)strtod(startp, &endp); /* use endp to determine token length */
        if (endp == startp) {
            t.type = (c == '+') ? TOK_PLUS : TOK_MINUS;
            t.len = 1;
            advance(1);
            return t;
        } else {
            t.type = TOK_NUM;
            t.len = (size_t)(endp - startp);
            advance(t.len);
            return t;
        }
    }

    /* Number starting with digit or dot */
    if (isdigit((unsigned char)c) || c == '.') {
        char *startp = &g_buf[g_i];
        char *endp = NULL;
        (void)strtod(startp, &endp);
        if (endp == startp) {
            t.type = TOK_ERR;
            t.len = 1;
            advance(1);
            return t;
        } else {
            t.type = TOK_NUM;
            t.len = (size_t)(endp - startp);
            advance(t.len);
            return t;
        }
    }

    /* Operators and parentheses */
    if (c == '*') {
        if (peek_char(1) == '*') {
            t.type = TOK_POW;
            t.len = 2;
            advance(2);
            return t;
        } else {
            t.type = TOK_MUL;
            t.len = 1;
            advance(1);
            return t;
        }
    }
    if (c == '/') { t.type = TOK_DIV; t.len = 1; advance(1); return t; }
    if (c == '+') { t.type = TOK_PLUS; t.len = 1; advance(1); return t; }
    if (c == '-') { t.type = TOK_MINUS; t.len = 1; advance(1); return t; }
    if (c == '(') { t.type = TOK_LPAREN; t.len = 1; advance(1); return t; }
    if (c == ')') { t.type = TOK_RPAREN; t.len = 1; advance(1); return t; }

    /* Unknown -> error token */
    t.type = TOK_ERR;
    t.len = 1;
    advance(1);
    return t;
}

/* Advance global token */
static void next_token(void) {
    g_tok = get_token_internal();
    if (g_tok.type == TOK_ERR) error_at(g_tok.start);
}

/* Extract token text (malloc'd) */
static char *token_text(const Token *t) {
    size_t n = t->len;
    char *s = (char *)malloc(n + 1);
    if (!s) return NULL;
    if (n > 0) memcpy(s, g_buf + t->start, n);
    s[n] = '\0';
    return s;
}

/* --------- Numeric helpers & ops --------- */

static Value value_from_token(const Token *t) {
    Value v;
    v.d = 0.0;
    v.i = 0;
    v.is_int = 0;
    v.pos = t->start;

    char *txt = token_text(t);
    if (!txt) { error_at(t->start); return v; }

    int looks_float = 0;
    for (size_t k = 0; k < t->len; ++k) {
        char ch = txt[k];
        if (ch == '.' || ch == 'e' || ch == 'E') { looks_float = 1; break; }
    }

    if (looks_float) {
        char *endp = NULL;
        double dv = strtod(txt, &endp);
        if (endp == txt) { error_at(t->start); free(txt); return v; }
        v.d = dv;
        double rounded = (double) my_llround(v.d);
        if (fabs(v.d - rounded) < EPS_INT && rounded <= (double)LLONG_MAX && rounded >= (double)LLONG_MIN) {
            v.is_int = 1; v.i = (long long)rounded;
        } else v.is_int = 0;
        free(txt);
        return v;
    } else {
        char *endp = NULL;
        long long iv = strtoll(txt, &endp, 10);
        if (endp == txt) {
            char *endp2 = NULL;
            double dv = strtod(txt, &endp2);
            if (endp2 == txt) { error_at(t->start); free(txt); return v; }
            v.d = dv; v.is_int = 0; free(txt); return v;
        }
        v.i = iv; v.d = (double)iv; v.is_int = 1;
        free(txt);
        return v;
    }
}

static double to_double(const Value *v) { return v->d; }

static Value apply_add(const Value *a, const Value *b, int plus) {
    Value res; res.pos = a->pos;
    if (a->is_int && b->is_int) {
        long long r = plus ? (a->i + b->i) : (a->i - b->i);
        res.is_int = 1; res.i = r; res.d = (double)r;
    } else {
        res.is_int = 0;
        res.d = plus ? (to_double(a) + to_double(b)) : (to_double(a) - to_double(b));
        double rounded = (double) my_llround(res.d);
        if (fabs(res.d - rounded) < EPS_INT && rounded <= (double)LLONG_MAX && rounded >= (double)LLONG_MIN) {
            res.is_int = 1; res.i = (long long)rounded; res.d = (double)res.i;
        }
    }
    return res;
}

static Value apply_mul(const Value *a, const Value *b) {
    Value res; res.pos = a->pos;
    if (a->is_int && b->is_int) {
        long long r = a->i * b->i;
        res.is_int = 1; res.i = r; res.d = (double)r;
    } else {
        res.is_int = 0;
        res.d = to_double(a) * to_double(b);
        double rounded = (double) my_llround(res.d);
        if (fabs(res.d - rounded) < EPS_INT && rounded <= (double)LLONG_MAX && rounded >= (double)LLONG_MIN) {
            res.is_int = 1; res.i = (long long)rounded; res.d = (double)res.i;
        }
    }
    return res;
}

static Value apply_div(const Value *a, const Value *b) {
    Value res; res.pos = a->pos;
    if ((b->is_int && b->i == 0) || (!b->is_int && fabs(b->d) < EPS_INT)) { error_at(b->pos); return res; }
    if (a->is_int && b->is_int) {
        if (a->i % b->i == 0) { res.is_int = 1; res.i = a->i / b->i; res.d = (double)res.i; }
        else { res.is_int = 0; res.d = ((double)a->i) / ((double)b->i); }
    } else {
        res.is_int = 0;
        res.d = to_double(a) / to_double(b);
        double rounded = (double) my_llround(res.d);
        if (fabs(res.d - rounded) < EPS_INT && rounded <= (double)LLONG_MAX && rounded >= (double)LLONG_MIN) {
            res.is_int = 1; res.i = (long long)rounded; res.d = (double)res.i;
        }
    }
    return res;
}

static Value apply_pow(const Value *a, const Value *b) {
    Value res; res.pos = a->pos;
    /* 0 ** negative -> error */
    if ((a->is_int && a->i == 0) || (!a->is_int && fabs(a->d) < EPS_INT)) {
        if ((!b->is_int && b->d < 0.0) || (b->is_int && b->i < 0)) { error_at(b->pos); return res; }
    }

    if (a->is_int && b->is_int) {
        long long base = a->i;
        long long expn = b->i;
        if (expn < 0) {
            errno = 0;
            double pd = pow((double)base, (double)expn);
            if (errno != 0) { error_at(b->pos); return res; }
            res.d = pd; res.is_int = 0; return res;
        } else {
            long long out = 1;
            long long p = base;
            long long e = expn;
            while (e > 0) {
                if (e & 1) out = out * p;
                e >>= 1;
                if (e) p = p * p;
            }
            res.is_int = 1; res.i = out; res.d = (double)out; return res;
        }
    }

    errno = 0;
    double pd = pow(to_double(a), to_double(b));
    if (errno != 0) { error_at(b->pos); return res; }
    res.d = pd;
    double rounded = (double) my_llround(pd);
    if (fabs(pd - rounded) < EPS_INT && rounded <= (double)LLONG_MAX && rounded >= (double)LLONG_MIN) {
        res.is_int = 1; res.i = (long long)rounded; res.d = (double)res.i;
    } else res.is_int = 0;
    return res;
}

/* --------- Parser (recursive descent) --------- */

static Value parse_expr(void);
static Value parse_term(void);
static Value parse_power(void);
static Value parse_unary(void);
static Value parse_primary(void);

static Value parse_expr(void) {
    Value v = parse_term();
    while (!g_have_error && (g_tok.type == TOK_PLUS || g_tok.type == TOK_MINUS)) {
        TokenType op = g_tok.type;
        next_token();
        Value rhs = parse_term();
        if (g_have_error) return v;
        v = (op == TOK_PLUS) ? apply_add(&v, &rhs, 1) : apply_add(&v, &rhs, 0);
    }
    return v;
}

static Value parse_term(void) {
    Value v = parse_power();
    while (!g_have_error && (g_tok.type == TOK_MUL || g_tok.type == TOK_DIV)) {
        TokenType op = g_tok.type;
        next_token();
        Value rhs = parse_power();
        if (g_have_error) return v;
        v = (op == TOK_MUL) ? apply_mul(&v, &rhs) : apply_div(&v, &rhs);
    }
    return v;
}

static Value parse_power(void) {
    Value left = parse_unary();
    if (g_have_error) return left;
    if (g_tok.type == TOK_POW) {
        next_token();
        Value right = parse_power();
        if (g_have_error) return left;
        left = apply_pow(&left, &right);
    }
    return left;
}

static Value parse_unary(void) {
    if (g_tok.type == TOK_PLUS) {
        next_token();
        return parse_unary();
    } else if (g_tok.type == TOK_MINUS) {
        next_token();
        Value v = parse_unary();
        if (g_have_error) return v;
        if (v.is_int) { v.i = -v.i; v.d = (double)v.i; }
        else { v.d = -v.d; double rounded = (double) my_llround(v.d); if (fabs(v.d - rounded) < EPS_INT) { v.is_int = 1; v.i = (long long)rounded; v.d = (double)v.i; } else v.is_int = 0; }
        return v;
    } else return parse_primary();
}

static Value parse_primary(void) {
    if (g_tok.type == TOK_NUM) {
        Token t = g_tok;
        Value v = value_from_token(&t);
        next_token();
        return v;
    } else if (g_tok.type == TOK_LPAREN) {
        next_token();
        Value v = parse_expr();
        if (g_have_error) return v;
        if (g_tok.type != TOK_RPAREN) { error_at(g_tok.start); return v; }
        next_token();
        return v;
    } else {
        error_at(g_tok.start);
        Value z; z.d = 0.0; z.i = 0; z.is_int = 1; z.pos = g_tok.start; return z;
    }
}

/* --------- File I/O helpers --------- */

static int read_file_into_buffer(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return -1; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return -1; }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return -1; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    if (got != (size_t)sz) { free(buf); fclose(f); return -1; }
    buf[got] = '\0';
    fclose(f);
    if (g_buf) free(g_buf);
    g_buf = buf; g_len = got; g_i = 0;
    return 0;
}

static void filename_base(const char *path, char *out, size_t outlen) {
    const char *p = path;
    const char *last_slash = strrchr(path, '/');
    if (last_slash) p = last_slash + 1;
    char tmp[PATH_MAX];
    strncpy(tmp, p, sizeof(tmp) - 1); tmp[sizeof(tmp)-1] = '\0';
    char *dot = strrchr(tmp, '.'); if (dot) *dot = '\0';
    strncpy(out, tmp, outlen - 1); out[outlen - 1] = '\0';
}

static int ensure_dir(const char *dirpath) {
    struct stat st;
    if (stat(dirpath, &st) == 0) {
        if (S_ISDIR(st.st_mode)) return 0;
        return -1;
    }
    if (mkdir(dirpath, 0755) != 0) return -1;
    return 0;
}

static int write_output_file(const char *outdir, const char *inbase, const char *content) {
    char outpath[PATH_MAX];
    /* Changed filename pattern: prefix with 'tsk' */
    if (snprintf(outpath, sizeof(outpath), "%s/%s_%s_%s_%s.txt",
                 outdir, inbase, STU_NAME, STU_LAST, STU_ID) >= (int)sizeof(outpath)) return -1;
    FILE *f = fopen(outpath, "wb");
    if (!f) return -1;
    size_t len = strlen(content);
    if (fwrite(content, 1, len, f) != len) { fclose(f); return -1; }
    if (fwrite("\n", 1, 1, f) != 1) { fclose(f); return -1; }
    fclose(f);
    return 0;
}

static char *evaluate_buffer_and_format_result(void) {
    /* Build a multi-line result: one line per expression in the input buffer */
    size_t cap = 512;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    out[0] = '\0';
    size_t out_len = 0;

    g_i = 0;
    g_have_error = 0;
    g_error_pos = 0;

    /* Prime first token */
    next_token();

    for (;;) {
        /* Skip any leading whitespace/comments between expressions */
        while (g_tok.type == TOK_EOF) break;
        /* If at EOF -> done */
        if (g_tok.type == TOK_EOF) break;

        /* Reset error state for this expression */
        g_have_error = 0;
        g_error_pos = 0;

        /* Parse one expression starting at current g_tok */
        Value v = parse_expr();

        /* Format one-line result */
        char linebuf[256];
        if (g_have_error) {
            size_t pos1 = g_error_pos + 1;
            snprintf(linebuf, sizeof(linebuf), "ERROR:%zu", pos1);

            /* Recover: skip to end of current line in the buffer and prime next token */
            while (g_i < g_len && g_buf[g_i] != '\n') g_i++;
            if (g_i < g_len && g_buf[g_i] == '\n') g_i++;
            g_tok = get_token_internal();
        } else {
            int print_as_int = 0;
            long long ival = 0;
            if (v.is_int) { print_as_int = 1; ival = v.i; }
            else {
                double rounded = (double) my_llround(v.d);
                if (fabs(v.d - rounded) < EPS_INT && rounded <= (double)LLONG_MAX && rounded >= (double)LLONG_MIN) {
                    print_as_int = 1; ival = (long long)rounded;
                }
            }
            if (print_as_int) snprintf(linebuf, sizeof(linebuf), "%lld", (long long)ival);
            else snprintf(linebuf, sizeof(linebuf), "%.15g", v.d);
            /* g_tok already points to the next token after the parsed expression */
        }

        /* Append line (with newline) to output buffer */
        size_t need = strlen(linebuf) + 1; /* +1 for newline */
        if (out_len + need + 1 > cap) {
            while (out_len + need + 1 > cap) cap *= 2;
            char *nx = (char *)realloc(out, cap);
            if (!nx) { free(out); return NULL; }
            out = nx;
        }
        memcpy(out + out_len, linebuf, strlen(linebuf));
        out_len += strlen(linebuf);
        out[out_len++] = '\n';
        out[out_len] = '\0';

        /* If token is EOF, stop; otherwise continue parsing next expression (g_tok is already set) */
        if (g_tok.type == TOK_EOF) break;
    }

    /* If nothing produced, return empty line */
    if (out_len == 0) {
        free(out);
        out = my_strdup("");
    }
    return out;
}

/* --------- CLI / main --------- */

static void usage(const char *prog) {
    fprintf(stderr, "Usage: %s [-d DIR | --dir DIR] [-o OUTDIR | --output-dir OUTDIR] input.txt\n", prog);
}

int main(int argc, char **argv) {
    const char *dir_mode = NULL;
    const char *outdir = NULL;
    const char *positional = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dir") == 0) {
            if (i + 1 >= argc) { usage(argv[0]); return 1; }
            dir_mode = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output-dir") == 0) {
            if (i + 1 >= argc) { usage(argv[0]); return 1; }
            outdir = argv[++i];
        } else if (argv[i][0] == '-') {
            usage(argv[0]); return 1;
        } else positional = argv[i];
    }

    /* Default: when no args given, scan the folder containing this source file for .txt files.
       This makes the program operate on all .txt files located alongside calc.c. */
    if (!dir_mode && !positional) {
        char srcdir[PATH_MAX];
        strncpy(srcdir, __FILE__, sizeof(srcdir) - 1);
        srcdir[sizeof(srcdir)-1] = '\0';
        char *slash = strrchr(srcdir, '/');
        if (slash) *slash = '\0';
        else strcpy(srcdir, ".");
        dir_mode = my_strdup(srcdir);
        if (!dir_mode) { fprintf(stderr, "Out of memory\n"); return 1; }
    }

    char *files[1024];
    size_t files_n = 0;
    char input_base_for_default[PATH_MAX];

    if (dir_mode) {
        DIR *d = opendir(dir_mode);
        if (!d) { perror("opendir"); return 1; }
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
            char full[PATH_MAX];
            if (snprintf(full, sizeof(full), "%s/%s", dir_mode, ent->d_name) >= (int)sizeof(full)) continue;
            struct stat st;
            if (stat(full, &st) != 0) continue;
            if (!S_ISREG(st.st_mode)) continue;
            const char *dot = strrchr(ent->d_name, '.');
            if (!dot) continue;
            if (strcmp(dot, ".txt") != 0) continue;
            files[files_n] = my_strdup(full);
            if (!files[files_n]) { closedir(d); return 1; }
            files_n++;
            if (files_n >= sizeof(files)/sizeof(files[0])) break;
        }
        closedir(d);
        const char *dname = dir_mode;
        const char *slash = strrchr(dname, '/');
        if (slash) dname = slash + 1;
        strncpy(input_base_for_default, dname, sizeof(input_base_for_default) - 1);
        input_base_for_default[sizeof(input_base_for_default)-1] = '\0';
    } else {
        char resolved_path[PATH_MAX];
        if (strchr(positional, '/') == NULL) {
            if (snprintf(resolved_path, sizeof(resolved_path), "src/%s", positional) >= (int)sizeof(resolved_path)) {
                fprintf(stderr, "Resolved path too long\n"); return 1;
            }
        } else {
            strncpy(resolved_path, positional, sizeof(resolved_path) - 1);
            resolved_path[sizeof(resolved_path)-1] = '\0';
        }
        files[0] = my_strdup(resolved_path);
        if (!files[0]) return 1;
        files_n = 1;
        filename_base(files[0], input_base_for_default, sizeof(input_base_for_default));
    }

    if (files_n == 0) { fprintf(stderr, "No .txt files found.\n"); return 1; }

    char outdir_buf[PATH_MAX];
    if (outdir) {
        strncpy(outdir_buf, outdir, sizeof(outdir_buf)-1);
        outdir_buf[sizeof(outdir_buf)-1] = '\0';
    } else {
        /* Use fixed Tasks directory as requested */
        strncpy(outdir_buf, "Tasks", sizeof(outdir_buf)-1);
        outdir_buf[sizeof(outdir_buf)-1] = '\0';
    }

    if (ensure_dir(outdir_buf) != 0) { perror("mkdir/ensure"); return 1; }

    for (size_t fi = 0; fi < files_n; ++fi) {
        const char *inpath = files[fi];
        if (read_file_into_buffer(inpath) != 0) {
            fprintf(stderr, "Failed to read %s\n", inpath);
            free(files[fi]);
            continue;
        }
        char inbase[PATH_MAX];
        filename_base(inpath, inbase, sizeof(inbase));
        char *result = evaluate_buffer_and_format_result();
        if (!result) { fprintf(stderr, "Evaluation failed for %s\n", inpath); free(files[fi]); continue; }
        if (write_output_file(outdir_buf, inbase, result) != 0) {
            fprintf(stderr, "Failed to write output for %s\n", inpath);
        }
        free(result);
        free(files[fi]);
        if (g_buf) { free(g_buf); g_buf = NULL; g_len = 0; g_i = 0; }
    }

    return 0;
}