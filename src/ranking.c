#include <string.h>

#include "ranking.h"

static RankingMode g_mode = RANK_TF;

void ranking_set_mode(RankingMode mode) {
    g_mode = mode;
}

RankingMode ranking_get_mode(void) {
    return g_mode;
}

static void swap(SearchResult* a, SearchResult* b) {
    SearchResult t = *a;
    *a = *b;
    *b = t;
}

/* In-place quicksort by score desc, then docID asc. */
static void qsort_results(SearchResult* arr, int left, int right) {
    int i = left;
    int j = right;
    SearchResult pivot = arr[left + (right - left) / 2];

    while (i <= j) {
        while (arr[i].score > pivot.score ||
               (arr[i].score == pivot.score && arr[i].docID < pivot.docID)) i++;
        while (arr[j].score < pivot.score ||
               (arr[j].score == pivot.score && arr[j].docID > pivot.docID)) j--;
        if (i <= j) {
            swap(&arr[i], &arr[j]);
            i++;
            j--;
        }
    }
    if (left < j) qsort_results(arr, left, j);
    if (i < right) qsort_results(arr, i, right);
}

void rank_results(SearchResult* results, int count) {
    rank_results_mode(results, count, g_mode);
}

void rank_results_mode(SearchResult* results, int count, RankingMode mode) {
    if (!results || count <= 1) return;
    (void)mode; /* scoring happens in search; ranking only sorts */
    qsort_results(results, 0, count - 1);
}

