#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "trie.h"

static TrieNode* g_root = NULL;

static TrieNode* trie_node_create(void) {
    TrieNode* n = (TrieNode*)malloc(sizeof(TrieNode));
    if (!n) return NULL;
    for (int i = 0; i < 26; i++) n->children[i] = NULL;
    n->isEndOfWord = 0;
    return n;
}

void trie_init(void) {
    if (!g_root) g_root = trie_node_create();
}

static void trie_free_node(TrieNode* node) {
    if (!node) return;
    for (int i = 0; i < 26; i++) {
        trie_free_node(node->children[i]);
        node->children[i] = NULL;
    }
    free(node);
}

void trie_free(void) {
    trie_free_node(g_root);
    g_root = NULL;
}

void insert_trie(char* word) {
    if (!word || !word[0]) return;
    if (!g_root) trie_init();

    TrieNode* cur = g_root;
    for (int i = 0; word[i] != '\0'; i++) {
        char c = word[i];
        if (c < 'a' || c > 'z') continue;
        int idx = c - 'a';
        if (!cur->children[idx]) {
            cur->children[idx] = trie_node_create();
            if (!cur->children[idx]) return;
        }
        cur = cur->children[idx];
    }
    cur->isEndOfWord = 1;
}

static void dfs_print(TrieNode* node, char* buf, int depth, int* printed, int max_print) {
    if (!node || *printed >= max_print) return;
    if (node->isEndOfWord) {
        buf[depth] = '\0';
        printf("%s\n", buf);
        (*printed)++;
        if (*printed >= max_print) return;
    }
    for (int i = 0; i < 26; i++) {
        if (node->children[i]) {
            buf[depth] = (char)('a' + i);
            dfs_print(node->children[i], buf, depth + 1, printed, max_print);
            if (*printed >= max_print) return;
        }
    }
}

void autocomplete(char* prefix) {
    if (!prefix) return;
    if (!g_root) trie_init();

    TrieNode* cur = g_root;
    char buf[WORD_MAX_LEN];
    int depth = 0;

    for (int i = 0; prefix[i] != '\0'; i++) {
        char c = prefix[i];
        if (c < 'a' || c > 'z') continue;
        int idx = c - 'a';
        if (!cur->children[idx]) {
            printf("(no suggestions)\n");
            return;
        }
        buf[depth++] = c;
        cur = cur->children[idx];
        if (depth >= WORD_MAX_LEN - 1) break;
    }

    int printed = 0;
    dfs_print(cur, buf, depth, &printed, 10);
    if (printed == 0) printf("(no suggestions)\n");
}

