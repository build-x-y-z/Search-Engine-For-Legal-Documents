#ifndef STRUCTURES_H
#define STRUCTURES_H

/*
Core data structures used by the search engine.

Complexity notes (high-level):
- Inverted index lookup: expected near O(1) via hash table buckets.
- Posting list intersection: O(total postings scanned) for AND queries.
- Trie autocomplete: O(|prefix| + number_of_suggestions_traversed).
*/

#define WORD_MAX_LEN 64
#define DOC_LABEL_MAX_LEN 256

typedef struct Posting {
    int docID;
    int frequency;
    struct Posting* next;
} Posting;

typedef struct WordEntry {
    char word[WORD_MAX_LEN];
    Posting* postingList;
    struct WordEntry* next; /* chained in hash bucket */
} WordEntry;

typedef struct TrieNode {
    struct TrieNode* children[26];
    int isEndOfWord;
} TrieNode;

typedef struct SearchResult {
    int docID;
    int score; /* TF-based score: sum of term frequencies */
} SearchResult;

#endif

