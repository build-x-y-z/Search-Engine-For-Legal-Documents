#ifndef QUERY_PARSER_H
#define QUERY_PARSER_H

/*
Lightweight query tokenization module.

Today:
- Tokenizes query into terms and operator tokens (AND/OR/NOT).
- Does NOT build a boolean AST or implement precedence/grouping yet.

Future:
- Replace with full parser for advanced boolean queries and phrase search.
*/

typedef enum {
    QUERY_TOKEN_TERM = 0,
    QUERY_TOKEN_AND = 1,
    QUERY_TOKEN_OR = 2,
    QUERY_TOKEN_NOT = 3
} QueryTokenType;

typedef struct QueryToken {
    QueryTokenType type;
    char* text; /* only for TERM; NULL for operators */
} QueryToken;

typedef struct Query {
    QueryToken* tokens;
    int count;
} Query;

int query_parse(const char* input, Query* out_query);
void query_free(Query* q);

#endif

