#ifndef TOKENS_UTILITY_H
#define TOKENS_UTILITY_H

#include <stddef.h>

struct st_token_item;

int st_token_item_is_hard_delim(const struct st_token_item *item);
struct st_token_item *st_token_item_find_hard_delim(
	const struct st_token_item *head);
struct st_token_item *form_token_list(const char *str);
void remove_quotes(struct st_token_item **head);
void unify_multiple_tokens(struct st_token_item *head);
void analyze_token_types(struct st_token_item *head);
int check_syntax(const struct st_token_item *head);
void st_token_item_iterate(const struct st_token_item **head, size_t times);
int st_token_item_contains(const struct st_token_item *head, int token_type);
struct st_token_item *st_token_item_find_type(struct st_token_item *head,
	int token_type);
int st_token_item_range_contains(const struct st_token_item *head,
	const struct st_token_item *tail, int token_type);
struct st_token_item *st_token_item_range_find_type(struct st_token_item *head,
	struct st_token_item *tail, int token_type);

#endif /* !TOKENS_UTILITY_H */
