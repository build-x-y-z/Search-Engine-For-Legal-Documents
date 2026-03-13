#ifndef RANKING_H
#define RANKING_H

#include "structures.h"

typedef enum {
    RANK_TF = 0,
    RANK_TFIDF = 1
} RankingMode;

/* Default mode is RANK_TF to preserve current behavior. */
void ranking_set_mode(RankingMode mode);
RankingMode ranking_get_mode(void);

/* Stable entrypoint used by search. */
void rank_results(SearchResult* results, int count);
/* Future-ready: rank with an explicit mode. */
void rank_results_mode(SearchResult* results, int count, RankingMode mode);

#endif

