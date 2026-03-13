#ifndef INDEX_H
#define INDEX_H

#include "structures.h"

void index_init(void);
void index_free(void);

void insert_word(char* word, int docID);
Posting* get_postings(char* word);

#endif

