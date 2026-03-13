#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "query_parser.h"
#include "tokenizer.h"

static int streq_ci(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a++;
        unsigned char cb = (unsigned char)*b++;
        if (tolower(ca) != tolower(cb)) return 0;
    }
    return *a == '\0' && *b == '\0';
}

static QueryTokenType classify_token(const char* raw) {
    if (!raw || !raw[0]) return QUERY_TOKEN_TERM;
    if (streq_ci(raw, "AND")) return QUERY_TOKEN_AND;
    if (streq_ci(raw, "OR")) return QUERY_TOKEN_OR;
    if (streq_ci(raw, "NOT")) return QUERY_TOKEN_NOT;
    return QUERY_TOKEN_TERM;
}

static void free_tokens(QueryToken* toks, int n) {
    if (!toks) return;
    for (int i = 0; i < n; i++) free(toks[i].text);
    free(toks);
}

int query_parse(const char* input, Query* out_query) {
    if (!out_query) return 0;
    out_query->tokens = NULL;
    out_query->count = 0;
    if (!input) return 1;

    int cap = 16;
    QueryToken* toks = (QueryToken*)malloc((size_t)cap * sizeof(QueryToken));
    if (!toks) return 0;
    int n = 0;

    /* Scan tokens separated by whitespace; preserve raw token for operator detection. */
    const char* p = input;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        char raw[256];
        int rlen = 0;
        while (*p && !isspace((unsigned char)*p)) {
            if (rlen < (int)sizeof(raw) - 1) raw[rlen++] = *p;
            p++;
        }
        raw[rlen] = '\0';

        QueryTokenType tt = classify_token(raw);
        if (n >= cap) {
            cap *= 2;
            QueryToken* grown = (QueryToken*)realloc(toks, (size_t)cap * sizeof(QueryToken));
            if (!grown) {
                free_tokens(toks, n);
                return 0;
            }
            toks = grown;
        }

        toks[n].type = tt;
        toks[n].text = NULL;

        if (tt == QUERY_TOKEN_TERM) {
            /* Normalize term using tokenizer rules. */
            char tmp[WORD_MAX_LEN];
            strncpy(tmp, raw, WORD_MAX_LEN - 1);
            tmp[WORD_MAX_LEN - 1] = '\0';
            normalize_token(tmp);
            if (tmp[0] == '\0') continue;

            toks[n].text = (char*)malloc(strlen(tmp) + 1);
            if (!toks[n].text) {
                free_tokens(toks, n + 1);
                return 0;
            }
            strcpy(toks[n].text, tmp);
        }

        n++;
    }

    out_query->tokens = toks;
    out_query->count = n;
    return 1;
}

void query_free(Query* q) {
    if (!q) return;
    free_tokens(q->tokens, q->count);
    q->tokens = NULL;
    q->count = 0;
}

