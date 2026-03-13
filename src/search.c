#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "search.h"
#include "tokenizer.h"
#include "index.h"
#include "ranking.h"
#include "query_parser.h"

static int is_or_term(const char* t) {
    return (t && strcmp(t, "or") == 0);
}

static int search_and_terms(char** terms, int term_count, SearchResult** out_results, int* out_count) {
    *out_results = NULL;
    *out_count = 0;
    if (term_count <= 0) return 1;

    Posting** lists = (Posting**)malloc((size_t)term_count * sizeof(Posting*));
    Posting** iters = (Posting**)malloc((size_t)term_count * sizeof(Posting*));
    if (!lists || !iters) {
        free(lists);
        free(iters);
        return 0;
    }

    const RankingMode mode = ranking_get_mode();
    const int n_docs = get_document_count();

    double* idf = NULL;
    if (mode == RANK_TFIDF) {
        idf = (double*)malloc((size_t)term_count * sizeof(double));
        if (!idf) {
            free(lists);
            free(iters);
            return 0;
        }
        for (int i = 0; i < term_count; i++) {
            const int df = get_document_frequency(terms[i]);
            if (df <= 0 || n_docs <= 0) {
                idf[i] = 0.0;
            } else {
                idf[i] = log((double)n_docs / (double)df);
            }
        }
    }

    for (int i = 0; i < term_count; i++) {
        lists[i] = get_postings(terms[i]);
        if (!lists[i]) {
            free(lists);
            free(iters);
            free(idf);
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
    for (int i = 0; i < term_count; i++) iters[i] = lists[i];

    int cap = 32;
    SearchResult* results = (SearchResult*)malloc((size_t)cap * sizeof(SearchResult));
    if (!results) {
        free(lists);
        free(iters);
        return 0;
    }
    int count = 0;

    for (Posting* p0 = base; p0; p0 = p0->next) {
        int doc = p0->docID;
        double score = (mode == RANK_TFIDF && idf) ? ((double)p0->frequency * idf[best]) : (double)p0->frequency;
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
            if (mode == RANK_TFIDF && idf) score += (double)cur->frequency * idf[t];
            else score += (double)cur->frequency;
        }

        if (ok) {
            if (count >= cap) {
                cap *= 2;
                SearchResult* grown = (SearchResult*)realloc(results, (size_t)cap * sizeof(SearchResult));
                if (!grown) {
                    free(results);
                    free(lists);
                    free(iters);
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
    free(lists);
    free(iters);
    free(idf);
    return 1;
}

static int search_or_terms(char** terms, int term_count, SearchResult** out_results, int* out_count) {
    *out_results = NULL;
    *out_count = 0;
    if (term_count <= 0) return 1;

    const RankingMode mode = ranking_get_mode();
    int n_docs = get_document_count();
    if (n_docs <= 0) return 1;

    double* scores = (double*)calloc((size_t)(n_docs + 1), sizeof(double));
    if (!scores) return 0;

    double* idf = NULL;
    if (mode == RANK_TFIDF) {
        idf = (double*)malloc((size_t)term_count * sizeof(double));
        if (!idf) {
            free(scores);
            return 0;
        }
        for (int i = 0; i < term_count; i++) {
            const int df = get_document_frequency(terms[i]);
            if (df <= 0 || n_docs <= 0) idf[i] = 0.0;
            else idf[i] = log((double)n_docs / (double)df);
        }
    }

    for (int i = 0; i < term_count; i++) {
        Posting* p = get_postings(terms[i]);
        while (p) {
            if (p->docID >= 1 && p->docID <= n_docs) {
                if (mode == RANK_TFIDF && idf) scores[p->docID] += (double)p->frequency * idf[i];
                else scores[p->docID] += (double)p->frequency;
            }
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
        if (scores[doc] > 0.0) {
            if (count >= cap) {
                cap *= 2;
                SearchResult* grown = (SearchResult*)realloc(results, (size_t)cap * sizeof(SearchResult));
                if (!grown) {
                    free(results);
                    free(scores);
                    free(idf);
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
    free(idf);
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

    /* Detect operators (future boolean parser will use this). */
    Query q;
    if (!query_parse(query, &q)) return 0;
    int has_or = 0;
    for (int i = 0; i < q.count; i++) {
        if (q.tokens[i].type == QUERY_TOKEN_OR) {
            has_or = 1;
            break;
        }
    }
    query_free(&q);

    /* Tokenize normalized terms (keeps current stopword behavior). */
    char** raw_terms = NULL;
    int raw_n = 0;
    if (!tokenize_text_dynamic(query, &raw_terms, &raw_n)) return 0;
    if (raw_n <= 0) {
        tokenize_free_tokens(raw_terms, raw_n);
        return 1;
    }

    if (has_or) {
        /* Remove the OR operator token from term list (matches old behavior). */
        int kept = 0;
        for (int i = 0; i < raw_n; i++) if (!is_or_term(raw_terms[i])) kept++;
        if (kept == 0) {
            tokenize_free_tokens(raw_terms, raw_n);
            return 1;
        }
        char** terms = (char**)malloc((size_t)kept * sizeof(char*));
        if (!terms) {
            tokenize_free_tokens(raw_terms, raw_n);
            return 0;
        }
        int n = 0;
        for (int i = 0; i < raw_n; i++) {
            if (is_or_term(raw_terms[i])) continue;
            terms[n++] = raw_terms[i];
            raw_terms[i] = NULL; /* transfer ownership */
        }
        /* Free remaining (e.g., "or") and container */
        for (int i = 0; i < raw_n; i++) free(raw_terms[i]);
        free(raw_terms);

        int ok = search_or_terms(terms, n, out_results, out_count);
        tokenize_free_tokens(terms, n);
        return ok;
    }

    {
        int ok = search_and_terms(raw_terms, raw_n, out_results, out_count);
        tokenize_free_tokens(raw_terms, raw_n);
        return ok;
    }
}

