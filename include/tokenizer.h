#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "structures.h"

/* Index all documents contained in the input file. */
void tokenize_document(const char* filename);

/* Index all .txt files under a root directory (recursively on Windows). */
void tokenize_directory(const char* root_dir);

/* Tokenize arbitrary text into normalized terms (lowercase, punctuation removed). */
int tokenize_text(const char* text, char tokens[][WORD_MAX_LEN], int max_tokens);

/* Document metadata populated by tokenize_document(). */
int get_document_count(void);
const char* get_document_label(int docID);
const char* get_document_text(int docID);

#endif

