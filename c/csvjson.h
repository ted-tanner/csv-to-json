#ifndef __CSVJSON_H
#define __CSVJSON_H

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assert.h"
#include "dynarray.h"

typedef enum _lexer_states
{
    NEW_LEXEME,

    UNQUOTED_STRING,
	
    INSIDE_QUOTES,
    QUOTE_INSIDE_QUOTE,

    BOOL_T,
    BOOL_TR,
    BOOL_TRU,
    BOOL_TRUE,
    BOOL_F,
    BOOL_FA,
    BOOL_FAL,
    BOOL_FALS,
    BOOL_FALSE,

    NUM_INTEGER,
    NUM_FLOAT,

    ERROR,
} LexerState;

typedef enum _token_type
{
    REGULAR_STRING,
    QUOTED_STRING,
    BOOLEAN,
    INTEGER,
    FLOAT,
    LINE_BREAK,
    EMPTY,
} TokenType;

typedef struct _token {
    TokenType type;
    char* data_start;
    char* data_end;
} Token;

typedef struct _lexed_csv {
    bool has_err;
    char* error_msg;
    DynArray tokens;
} LexedCsv;

#if defined(WIN32) || defined(__WIN32)

__declspec(dllimport) char* csv_to_json(char* csv, size_t length);
__declspec(dllimport) void free_json(void* csv_ptr);

#else

char* csv_to_json(char* csv, size_t length);
void free_json(void* csv_ptr);

#endif

LexedCsv _lex_csv(char* csv, size_t length);

#endif
