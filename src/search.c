#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "search.h"
#include "tokenizer.h"
#include "index.h"
#include "ranking.h"

static int is_or_token(const char* t) {
    return (t && (strcmp(t, "or") == 0 || strcmp(t, "OR") == 0));
}

static int contains_or(const char* query) {
    if (!query) return 0;
    char toks[256][WORD_MAX_LEN];
    int n = tokenize_text(query, toks, 256);
    for (int i = 0; i < n; i++) {
        if (is_or_token(toks[i])) return 1;
    }
    /* Also handle raw " OR " that tokenizer would normalize away (kept for clarity). */
    return 0;
}

static int search_and_terms(char terms[][WORD_MAX_LEN], int term_count, SearchResult** out_results, int* out_count) {
    *out_results = NULL;
    *out_count = 0;
    if (term_count <= 0) return 1;

    Posting* lists[64];
    if (term_count > 64) term_count = 64;

    for (int i = 0; i < term_count; i++) {
        lists[i] = get_postings(terms[i]);
        if (!lists[i]) {
            return 1; /* empty intersection */
        }
    }

    /* Intersect by scanning the smallest list first (cheap heuristic: pick by length estimate). */
    int best = 0;
    int best_len = 2147483647;
    for (int i = 0; i < term_count; i++) {
        int len = 0;
        for (Posting* p = lists[i]; p && len < best_len; p = p->next) len++;
        if (len < best_len) {
            best_len = len;
            best = i;
        }
    }

    Posting* base = lists[best];
    Posting* iters[64];
    for (int i = 0; i < term_count; i++) iters[i] = lists[i];

    int cap = 32;
    SearchResult* results = (SearchResult*)malloc((size_t)cap * sizeof(SearchResult));
    if (!results) return 0;
    int count = 0;

    for (Posting* p0 = base; p0; p0 = p0->next) {
        int doc = p0->docID;
        int score = p0->frequency;
        int ok = 1;

        for (int t = 0; t < term_count; t++) {
            if (t == best) continue;
            Posting* cur = iters[t];
            while (cur && cur->docID < doc) cur = cur->next;
            iters[t] = cur;
            if (!cur || cur->docID != doc) {
                ok = 0;
                break;
            }
            score += cur->frequency;
        }

        if (ok) {
            if (count >= cap) {
                cap *= 2;
                SearchResult* grown = (SearchResult*)realloc(results, (size_t)cap * sizeof(SearchResult));
                if (!grown) {
                    free(results);
                    return 0;
                }
                results = grown;
            }
            results[count].docID = doc;
            results[count].score = score;
            count++;
        }
    }

    if (count > 0) rank_results(results, count);
    *out_results = results;
    *out_count = count;
    return 1;
}

static int search_or_terms(char terms[][WORD_MAX_LEN], int term_count, SearchResult** out_results, int* out_count) {
    *out_results = NULL;
    *out_count = 0;
    if (term_count <= 0) return 1;

    int n_docs = get_document_count();
    if (n_docs <= 0) return 1;

    int* scores = (int*)calloc((size_t)(n_docs + 1), sizeof(int));
    if (!scores) return 0;

    for (int i = 0; i < term_count; i++) {
        Posting* p = get_postings(terms[i]);
        while (p) {
            if (p->docID >= 1 && p->docID <= n_docs) scores[p->docID] += p->frequency;
            p = p->next;
        }
    }

    int cap = 32;
    SearchResult* results = (SearchResult*)malloc((size_t)cap * sizeof(SearchResult));
    if (!results) {
        free(scores);
        return 0;
    }

    int count = 0;
    for (int doc = 1; doc <= n_docs; doc++) {
        if (scores[doc] > 0) {
            if (count >= cap) {
                cap *= 2;
                SearchResult* grown = (SearchResult*)realloc(results, (size_t)cap * sizeof(SearchResult));
                if (!grown) {
                    free(results);
                    free(scores);
                    return 0;
                }
                results = grown;
            }
            results[count].docID = doc;
            results[count].score = scores[doc];
            count++;
        }
    }

    free(scores);
    if (count > 0) rank_results(results, count);
    *out_results = results;
    *out_count = count;
    return 1;
}

int search_query(const char* query, SearchResult** out_results, int* out_count) {
    if (!out_results || !out_count) return 0;
    *out_results = NULL;
    *out_count = 0;

    if (!query) return 1;

    /* Tokenize (normalized terms) */
    char raw[512][WORD_MAX_LEN];
    int raw_n = tokenize_text(query, raw, 512);
    if (raw_n <= 0) return 1;

    /* Split by OR token (very simple boolean support). */
    if (contains_or(query)) {
        char terms[512][WORD_MAX_LEN];
        int n = 0;
        for (int i = 0; i < raw_n; i++) {
            if (is_or_token(raw[i])) continue;
            strncpy(terms[n], raw[i], WORD_MAX_LEN - 1);
            terms[n][WORD_MAX_LEN - 1] = '\0';
            n++;
        }
        return search_or_terms(terms, n, out_results, out_count);
    }

    return search_and_terms(raw, raw_n, out_results, out_count);
}

