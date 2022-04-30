#ifndef TOKENS_UTILITY_H
#define TOKENS_UTILITY_H

#include "def.h"

/*
   Functions to analyze user's inputs
*/

int is_hard_delim(const st_token *item);
st_token *find_hard_delim(const st_token *head);
st_token *form_token_list(const char *str);
void remove_quotes(st_token **head);
void unify_multiple_tokens(st_token *head);
void analyze_token_types(st_token *head);
int check_syntax(const st_token *head);
/* void st_token_determine_type(st_token *item); */

#endif /* !TOKENS_UTILITY_H */
