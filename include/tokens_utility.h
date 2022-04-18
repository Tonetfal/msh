#ifndef TOKENS_UTILITY_H
#define TOKENS_UTILITY_H

#include <stddef.h>

struct st_token_item;

struct st_token_item *form_token_list(const char *str);
void remove_quotes(struct st_token_item **head);
void unify_multiple_tokens(struct st_token_item *head);
void analyze_token_types(struct st_token_item *head);
int check_syntax(const struct st_token_item *head);
void st_token_item_iterate(const struct st_token_item **head, size_t times);

#endif /* !TOKENS_UTILITY_H */
