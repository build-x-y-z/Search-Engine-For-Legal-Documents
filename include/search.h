#ifndef SEARCH_H
#define SEARCH_H

#include "structures.h"

/*
Search the inverted index for a query string.
- Default operator: AND (intersection).
- Also supports simple boolean OR if the query contains token "or" / "OR".

Returns:
- 1 on success, 0 on failure (allocation, etc.)
*/
int search_query(const char* query, SearchResult** out_results, int* out_count);

#endif

