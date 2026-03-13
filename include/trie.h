#ifndef TRIE_H
#define TRIE_H

#include "structures.h"

void trie_init(void);
void trie_free(void);

void insert_trie(char* word);
void autocomplete(char* prefix);

#endif

