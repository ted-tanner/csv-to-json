#include "csvjson.h"

// ALLOCATED: json_buffer - Python must call free function free_json
#if defined(WIN32) || defined(__WIN32)
__declspec(dllimport) char *csv_to_json(char *csv_buffer, size_t length)
#else
char *csv_to_json(char *csv_buffer, size_t length)
#endif
{
	LexedCsv lexed_csv = _lex_csv(csv_buffer, length);
	DynArray json_buffer = new_dynarr(length + 256, char);

	if (lexed_csv.has_err)
	{
		size_t message_length = 1;
		for (char *curr = lexed_csv.error_msg; *curr != '\0'; ++curr, ++message_length)
			;
		dynarr_push_multiple(&json_buffer, lexed_csv.error_msg, message_length);

		free_dynarr(&lexed_csv.tokens);
		free(lexed_csv.error_msg);

		dynarr_shrink(&json_buffer);

		return json_buffer.arr;
	}

	size_t largest_title_size = 0;

	size_t curr_pos = 0;
	for (Token *curr = (Token *)lexed_csv.tokens.arr; curr->type != LINE_BREAK; ++curr, ++curr_pos)
	{
		if (curr->data_end - curr->data_start > largest_title_size)
		{
			largest_title_size = curr->data_end - curr->data_start;
		}
	}

	// Increment curr_pos to consume line break
	size_t column_count = curr_pos++;

	// If the number of tokens isn't divisible by the number of fields
	if (!(lexed_csv.tokens.size % (column_count + 1) - 1))
	{
		char error_msg[] = "Invalid CSV: One or more records is missing a field";

		size_t message_length = 1;
		for (char *curr = error_msg; *curr != '\0'; ++curr, ++message_length)
			;

		dynarr_push_multiple(&json_buffer, error_msg, message_length);

		free_dynarr(&lexed_csv.tokens);
		free(lexed_csv.error_msg);

		dynarr_shrink(&json_buffer);

		return json_buffer.arr;
	}

	char column_titles[column_count][largest_title_size + 1];
	size_t column_title_lengths[column_count];

	for (size_t i = 0; i < column_count; ++i)
	{
		Token *token = dynarr_get_ptr(&lexed_csv.tokens, i, Token);

		if (*(token->data_end - 1) == '\r')
		{
			--token->data_end;
		}

		memcpy(column_titles[i], token->data_start, token->data_end - token->data_start);
		column_title_lengths[i] = token->data_end - token->data_start;
	}

	size_t records_remaining = lexed_csv.tokens.size / (column_count + 1) - 1;

	char next_char = '[';
	dynarr_push(&json_buffer, next_char);

	if (records_remaining > 0)
	{
		next_char = '{';
		dynarr_push(&json_buffer, next_char);
	}

	for (size_t title_idx = 0; curr_pos < lexed_csv.tokens.size; ++curr_pos)
	{
		Token *token = dynarr_get_ptr(&lexed_csv.tokens, curr_pos, Token);

		if (token->type == LINE_BREAK)
		{
			next_char = '}';
			dynarr_push(&json_buffer, next_char);

			if (lexed_csv.tokens.size - curr_pos >= column_count)
			{
				next_char = ',';
				dynarr_push(&json_buffer, next_char);

				next_char = '{';
				dynarr_push(&json_buffer, next_char);
			}
		}
		else
		{
			// Allow for incorrectly formed CSV where every record has an extra, empty field
			if (!title_idx
			    && token->type == EMPTY && curr_pos + 1 < lexed_csv.tokens.size
			    && dynarr_get(&lexed_csv.tokens, curr_pos + 1, Token).type == LINE_BREAK)
			{
				continue;
			}

			next_char = '"';
			dynarr_push(&json_buffer, next_char);

			dynarr_push_multiple(&json_buffer, column_titles[title_idx], column_title_lengths[title_idx]);

			if (++title_idx == column_count)
			{
				title_idx = 0;
			}

			next_char = '"';
			dynarr_push(&json_buffer, next_char);

			next_char = ':';
			dynarr_push(&json_buffer, next_char);

			if (*(token->data_end - 1) == '\r')
			{
				--token->data_end;
			}

			switch (token->type)
			{
			case REGULAR_STRING:
				next_char = '"';
				dynarr_push(&json_buffer, next_char);

				dynarr_push_multiple(&json_buffer, token->data_start, token->data_end - token->data_start);

				next_char = '"';
				dynarr_push(&json_buffer, next_char);
				break;
			case QUOTED_STRING:
				next_char = '"';
				dynarr_push(&json_buffer, next_char);

				bool prev_was_quotes = false;
				for (size_t i = 0; i < token->data_end - token->data_start; ++i)
				{
					char curr_char = *(token->data_start + i);

					// Multi-line strings are allowed in CSV, but not in JSON
					if (curr_char == '\n')
					{
						next_char = '\\';
						dynarr_push(&json_buffer, next_char);

						next_char = 'n';
						dynarr_push(&json_buffer, next_char);

						prev_was_quotes = false;

						continue;
					}

					// The lexer doesn't remove escape quotes, so that needs to be done here
					if (curr_char != '"')
					{
						dynarr_push(&json_buffer, curr_char);
						prev_was_quotes = false;
					}
					else if (!prev_was_quotes)
					{
						next_char = '\\';
						dynarr_push(&json_buffer, next_char);

						dynarr_push(&json_buffer, curr_char);
						prev_was_quotes = true;
					}
					else
					{
						prev_was_quotes = false;
					}
				}

				next_char = '"';
				dynarr_push(&json_buffer, next_char);
				break;
			case BOOLEAN:
			case INTEGER:
			case FLOAT:
				dynarr_push_multiple(&json_buffer, token->data_start, token->data_end - token->data_start);
				break;
			case LINE_BREAK:
				assert(false, "This assertion should be unreachable");
				break;
			case EMPTY:
			{
				char null_chars[] = "null";
				dynarr_push_multiple(&json_buffer, null_chars, 4);
				break;
			}
			}

			if (title_idx)
			{
				next_char = ',';
				dynarr_push(&json_buffer, next_char);
			}
		}
	}

	next_char = ']';
	dynarr_push(&json_buffer, next_char);

	next_char = '\0';
	dynarr_push(&json_buffer, next_char);

	free_dynarr(&lexed_csv.tokens);
	free(lexed_csv.error_msg);

	dynarr_shrink(&json_buffer);

	return json_buffer.arr;
}

// Used by Python to free allocated memory
#if defined(WIN32) || defined(__WIN32)
__declspec(dllimport) void free_json(void *json_ptr)
#else
void free_json(void *json_ptr)
#endif
{
	free(json_ptr);
}

// ALLOCATED: csv_tokens
// ALLOCATED: error_msg
LexedCsv _lex_csv(char *csv, size_t length)
{
	// Trim whitespace at front
	while (isspace(*csv))
	{
		++csv;
		--length;
	}

	// Trim whitespace at back
	while (isspace(*(csv + length - 1)))
	{
		--length;
	}

	DynArray csv_tokens = new_dynarr(length / 4, Token);

	char *curr_lexeme_start = csv;
	LexerState state = NEW_LEXEME;

	// Assuming error messages are below 255 chars
	char *error_msg = malloc(256 * sizeof(char));

	if (!error_msg)
	{
		fprintf(stderr, "MALLOC FAILURE (%s:%d)", __FILE__, __LINE__);
		abort();
	}

	error_msg[0] = '\0';

	uint32_t line_number = 1;
	uint32_t character_number = 1;

	for (char *pos = csv; pos <= csv + length; ++pos, ++character_number)
	{
		bool was_ending_char_carriage_return = false;

		char character = pos == csv + length ? '\n' : *pos;

		switch (character)
		{
		case '"':
			switch (state)
			{
			case NEW_LEXEME:
			case QUOTE_INSIDE_QUOTE:
				state = INSIDE_QUOTES;
				break;
			case INSIDE_QUOTES:
				state = QUOTE_INSIDE_QUOTE;
				break;
			default:
				state = ERROR;
				sprintf(error_msg,
						"Invalid CSV (%d:%d): Invalid placement of double-quote",
						line_number,
						character_number);
			}
			break;
		case 't':
			switch (state)
			{
			case NEW_LEXEME:
				state = BOOL_T;
				break;
			case UNQUOTED_STRING:
			case INSIDE_QUOTES:
				// Retain state
				break;
			case QUOTE_INSIDE_QUOTE:
				state = ERROR;
				sprintf(error_msg,
						"Invalid CSV (%d:%d): Invalid character outside of quotes",
						line_number,
						character_number);
				break;
			default:
				state = UNQUOTED_STRING;
			}
			break;
		case 'r':
			switch (state)
			{
			case INSIDE_QUOTES:
				// Retain state
				break;
			case QUOTE_INSIDE_QUOTE:
				state = ERROR;
				sprintf(error_msg,
						"Invalid CSV (%d:%d): Invalid character outside of quotes",
						line_number,
						character_number);
				break;
			case BOOL_T:
				state = BOOL_TR;
				break;
			default:
				state = UNQUOTED_STRING;
			}
			break;
		case 'u':
			switch (state)
			{
			case INSIDE_QUOTES:
				// Retain state
				break;
			case QUOTE_INSIDE_QUOTE:
				state = ERROR;
				sprintf(error_msg,
						"Invalid CSV (%d:%d): Invalid character outside of quotes",
						line_number,
						character_number);
				break;
			case BOOL_TR:
				state = BOOL_TRU;
				break;
			default:
				state = UNQUOTED_STRING;
			}
			break;
		case 'e':
			switch (state)
			{
			case INSIDE_QUOTES:
				// Retain state
				break;
			case QUOTE_INSIDE_QUOTE:
				state = ERROR;
				sprintf(error_msg,
						"Invalid CSV (%d:%d): Invalid character outside of quotes",
						line_number,
						character_number);
				break;
			case BOOL_TRU:
				state = BOOL_TRUE;
				break;
			case BOOL_FALS:
				state = BOOL_FALSE;
				break;
			default:
				state = UNQUOTED_STRING;
			}
			break;
		case 'f':
			switch (state)
			{
			case NEW_LEXEME:
				state = BOOL_F;
				break;
			case UNQUOTED_STRING:
			case INSIDE_QUOTES:
				// Retain state
				break;
			case QUOTE_INSIDE_QUOTE:
				state = ERROR;
				sprintf(error_msg,
						"Invalid CSV (%d:%d): Invalid character outside of quotes",
						line_number,
						character_number);
				break;
			default:
				state = UNQUOTED_STRING;
			}
			break;
		case 'a':
			switch (state)
			{
			case INSIDE_QUOTES:
				// Retain state
				break;
			case QUOTE_INSIDE_QUOTE:
				state = ERROR;
				sprintf(error_msg,
						"Invalid CSV (%d:%d): Invalid character outside of quotes",
						line_number,
						character_number);
				break;
			case BOOL_F:
				state = BOOL_FA;
				break;
			default:
				state = UNQUOTED_STRING;
			}
			break;
		case 'l':
			switch (state)
			{
			case INSIDE_QUOTES:
				// Retain state
				break;
			case QUOTE_INSIDE_QUOTE:
				state = ERROR;
				sprintf(error_msg,
						"Invalid CSV (%d:%d): Invalid character outside of quotes",
						line_number,
						character_number);
				break;
			case BOOL_FA:
				state = BOOL_FAL;
				break;
			default:
				state = UNQUOTED_STRING;
			}
			break;
		case 's':
			switch (state)
			{
			case INSIDE_QUOTES:
				// Retain state
				break;
			case QUOTE_INSIDE_QUOTE:
				state = ERROR;
				sprintf(error_msg,
						"Invalid CSV (%d:%d): Invalid character outside of quotes",
						line_number,
						character_number);
				break;
			case BOOL_FAL:
				state = BOOL_FALS;
				break;
			default:
				state = UNQUOTED_STRING;
			}
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			switch (state)
			{
			case NEW_LEXEME:
				state = NUM_INTEGER;
				break;
			case UNQUOTED_STRING:
			case INSIDE_QUOTES:
			case NUM_INTEGER:
			case NUM_FLOAT:
				// Retain state
				break;
			case QUOTE_INSIDE_QUOTE:
				state = ERROR;
				sprintf(error_msg,
						"Invalid CSV (%d:%d): Invalid character outside of quotes",
						line_number,
						character_number);
				break;
			default:
				state = UNQUOTED_STRING;
			}
			break;
		case '.':
			switch (state)
			{
			case NEW_LEXEME:
				state = NUM_FLOAT;
				break;
			case UNQUOTED_STRING:
			case INSIDE_QUOTES:
				// Retain state
				break;
			case QUOTE_INSIDE_QUOTE:
				state = ERROR;
				sprintf(error_msg,
						"Invalid CSV (%d:%d): Invalid character outside of quotes",
						line_number,
						character_number);
				break;
			case NUM_INTEGER:
				state = NUM_FLOAT;
				break;
			default:
				state = UNQUOTED_STRING;
			}
			break;
		case ' ':
		case '\t':
			// Being somewhat permissive of invalid CSV here to avoid drastically increasing complexity
			// Also, not allowing space AFTER most lexemes for the same reason, only spaces before will
			// be ignored. In other cases, the lexeme will be considered an UNQUOTED_STRING or give an
			// error
			switch (state)
			{
			case NEW_LEXEME:
				// Don't include preceding spaces
				// This is the most likely reason someone would put spaces outside of a string
				++curr_lexeme_start;
			case UNQUOTED_STRING:
			case INSIDE_QUOTES:
				// Retain state
				break;
			case QUOTE_INSIDE_QUOTE:
				state = ERROR;
				sprintf(error_msg,
						"Invalid CSV (%d:%d): Spaces after quoted strings not supported",
						line_number,
						character_number);
			default:
				state = UNQUOTED_STRING;
			}
			break;
		case '\r':
			if (state == INSIDE_QUOTES)
			{
				break;
			}

			was_ending_char_carriage_return = true;

			// Assume the next character is '\n' and swallow it
			++pos;
		case '\n':
			++line_number;
			character_number = 0;
		case ',':
		{
			bool is_lexeme_complete = true;
			Token token;

			switch (state)
			{
			case NEW_LEXEME:
			{
				token.type = EMPTY;
				token.data_start = curr_lexeme_start;
				token.data_end = curr_lexeme_start;

				break;
			}
			case UNQUOTED_STRING:
			case BOOL_T:
			case BOOL_TR:
			case BOOL_TRU:
			case BOOL_F:
			case BOOL_FA:
			case BOOL_FAL:
			case BOOL_FALS:
			{
				token.type = REGULAR_STRING;
				token.data_start = curr_lexeme_start;
				token.data_end = pos;

				break;
			}
			case INSIDE_QUOTES:
				// Retain state
				is_lexeme_complete = false;
				break;
			case QUOTE_INSIDE_QUOTE:
			{
				token.type = QUOTED_STRING;
				// Don't include quotes in data
				token.data_start = curr_lexeme_start + 1;
				token.data_end = was_ending_char_carriage_return ? pos - 2 : pos - 1;

				break;
			}
			case BOOL_TRUE:
			{
				token.type = BOOLEAN;
				token.data_start = curr_lexeme_start;
				token.data_end = was_ending_char_carriage_return ? pos - 1 : pos;

				break;
			}
			case BOOL_FALSE:
			{
				token.type = BOOLEAN;
				token.data_start = curr_lexeme_start;
				token.data_end = was_ending_char_carriage_return ? pos - 1 : pos;

				break;
			}
			case NUM_INTEGER:
			{
				token.type = INTEGER;
				token.data_start = curr_lexeme_start;
				token.data_end = was_ending_char_carriage_return ? pos - 1 : pos;

				break;
			}
			case NUM_FLOAT:
			{
				token.type = FLOAT;
				token.data_start = curr_lexeme_start;
				token.data_end = was_ending_char_carriage_return ? pos - 1 : pos;

				break;
			}
			default:
				assert(false, "This case should be unreachable");
				state = ERROR;
				is_lexeme_complete = false;
			}

			if (is_lexeme_complete)
			{
				dynarr_push(&csv_tokens, token);

				if (character == '\n' || character == '\r')
				{
					Token line_break_token = {
						.type = LINE_BREAK,
						.data_start = 0,
						.data_end = 0,
					};

					dynarr_push(&csv_tokens, line_break_token);
				}

				curr_lexeme_start = pos + 1;
				state = NEW_LEXEME;
			}

			break;
		}
		default:
			switch (state)
			{
			case UNQUOTED_STRING:
			case INSIDE_QUOTES:
				// Retain state
				break;
			case QUOTE_INSIDE_QUOTE:
				state = ERROR;
				sprintf(error_msg,
						"Invalid CSV (%d:%d): Invalid character outside of quotes",
						line_number,
						character_number);
				break;
			default:
				state = UNQUOTED_STRING;
			}
		}

		if (state == ERROR)
		{
			break;
		}
	}

	// If escaped double-quotes exist, will leave the escaped quotes AND the *escaping* quotes
	// to avoid drastically increasing complexity. These will need to be taken care of in a
	// later stage
	LexedCsv lexed_csv = {
		.has_err = state == ERROR,
		.error_msg = error_msg,
		.tokens = csv_tokens,
	};

	return lexed_csv;
}
