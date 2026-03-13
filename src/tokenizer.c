#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "tokenizer.h"
#include "index.h"
#include "trie.h"

#define MAX_DOCS 2048
#define MAX_LINE 8192

static int g_doc_count = 0;
static char g_doc_labels[MAX_DOCS][DOC_LABEL_MAX_LEN];
static char* g_doc_texts[MAX_DOCS];

#ifdef _WIN32
/* MinGW may not declare popen/pclose in strict C99 mode. */
FILE* popen(const char* command, const char* mode);
int pclose(FILE* stream);
#endif

static char* read_entire_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long n = ftell(f);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    char* buf = (char*)malloc((size_t)n + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

static int is_stopword(const char* w) {
    /* Optional stopword filtering (small, safe list for legal text). */
    static const char* stop[] = {
        "the","a","an","and","of","to","in","for","on","with","by","at","from","as","is","are","was","were",
        "be","been","being","that","this","these","those","it","its"
    };
    int n = (int)(sizeof(stop) / sizeof(stop[0]));
    for (int i = 0; i < n; i++) {
        if (strcmp(w, stop[i]) == 0) return 1;
    }
    return 0;
}

int tokenize_text(const char* text, char tokens[][WORD_MAX_LEN], int max_tokens) {
    if (!text || !tokens || max_tokens <= 0) return 0;
    int out = 0;

    char cur[WORD_MAX_LEN];
    int len = 0;

    for (int i = 0; ; i++) {
        unsigned char ch = (unsigned char)text[i];
        int done = (ch == 0);

        if (!done && (isalnum(ch) || ch == '_')) {
            if (len < WORD_MAX_LEN - 1) {
                cur[len++] = (char)tolower(ch);
            }
        } else {
            if (len > 0) {
                cur[len] = '\0';
                if (!is_stopword(cur)) {
                    strncpy(tokens[out], cur, WORD_MAX_LEN - 1);
                    tokens[out][WORD_MAX_LEN - 1] = '\0';
                    out++;
                    if (out >= max_tokens) return out;
                }
                len = 0;
            }
            if (done) break;
        }
    }

    return out;
}

static void free_docs(void) {
    for (int i = 1; i <= g_doc_count; i++) {
        free(g_doc_texts[i]);
        g_doc_texts[i] = NULL;
    }
    g_doc_count = 0;
}

int get_document_count(void) {
    return g_doc_count;
}

const char* get_document_label(int docID) {
    if (docID <= 0 || docID > g_doc_count) return NULL;
    return g_doc_labels[docID];
}

const char* get_document_text(int docID) {
    if (docID <= 0 || docID > g_doc_count) return NULL;
    return g_doc_texts[docID];
}

static void rtrim_newline(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

static void index_doc_text(const char* label, const char* text) {
    if (!text || !text[0]) return;
    if (g_doc_count + 1 >= MAX_DOCS) return;

    g_doc_count++;
    int docID = g_doc_count;

    if (label && label[0]) {
        strncpy(g_doc_labels[docID], label, DOC_LABEL_MAX_LEN - 1);
        g_doc_labels[docID][DOC_LABEL_MAX_LEN - 1] = '\0';
    } else {
        snprintf(g_doc_labels[docID], DOC_LABEL_MAX_LEN, "DOC%d", docID);
    }

    g_doc_texts[docID] = (char*)malloc(strlen(text) + 1);
    if (g_doc_texts[docID]) strcpy(g_doc_texts[docID], text);

    char toks[4096][WORD_MAX_LEN];
    int n = tokenize_text(text, toks, 4096);
    for (int i = 0; i < n; i++) {
        insert_word(toks[i], docID);
        insert_trie(toks[i]);
    }
}

void tokenize_document(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("Error: cannot open %s\n", filename);
        return;
    }

    free_docs();

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        rtrim_newline(line);
        if (line[0] == '\0') continue;

        char* colon = strchr(line, ':');
        if (!colon) continue;

        if (g_doc_count + 1 >= MAX_DOCS) break;
        g_doc_count++;
        int docID = g_doc_count;

        /* label: everything before ':' */
        int label_len = (int)(colon - line);
        if (label_len <= 0) {
            snprintf(g_doc_labels[docID], DOC_LABEL_MAX_LEN, "DOC%d", docID);
        } else {
            int copy = label_len;
            if (copy > DOC_LABEL_MAX_LEN - 1) copy = DOC_LABEL_MAX_LEN - 1;
            memcpy(g_doc_labels[docID], line, (size_t)copy);
            g_doc_labels[docID][copy] = '\0';
        }

        /* store full text (after ':') for display/snippets if desired */
        const char* body = colon + 1;
        while (*body == ' ' || *body == '\t') body++;
        g_doc_texts[docID] = (char*)malloc(strlen(body) + 1);
        if (g_doc_texts[docID]) strcpy(g_doc_texts[docID], body);

        char toks[2048][WORD_MAX_LEN];
        int n = tokenize_text(body, toks, 2048);
        for (int i = 0; i < n; i++) {
            insert_word(toks[i], docID);
            insert_trie(toks[i]);
        }
    }

    fclose(f);
}

void tokenize_directory(const char* root_dir) {
    if (!root_dir || !root_dir[0]) return;

    free_docs();

#ifdef _WIN32
    /*
    Windows recursive crawl: `dir /b /s root_dir\*.txt`
    Uses only stdio/stdlib/string; relies on the OS shell for directory listing.
    */
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "dir /b /s \"%s\\*.txt\"", root_dir);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        printf("Error: unable to list files under %s\n", root_dir);
        return;
    }

    char path[1024];
    while (fgets(path, (int)sizeof(path), pipe)) {
        rtrim_newline(path);
        if (path[0] == '\0') continue;

        /* Avoid indexing the old manifest-style documents file as a normal document. */
        {
            const char* leaf = strrchr(path, '\\');
            leaf = leaf ? (leaf + 1) : path;
            if (strcmp(leaf, "documents.txt") == 0) continue;
        }

        char* content = read_entire_file(path);
        if (!content) continue;

        /* Use the full path as label (shortened into DOC_LABEL_MAX_LEN) */
        index_doc_text(path, content);
        free(content);
    }

    pclose(pipe);

#else
    /*
    Portable fallback (no directory traversal in standard C):
    Put a manifest in `data/documents.txt` (DOCx: text) or extend the program later
    with platform-specific APIs.
    */
    (void)root_dir;
    printf("Directory indexing is only implemented on Windows builds.\n");
#endif
}

