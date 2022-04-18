#ifndef ST_TOKEN_ITEM_H
#define ST_TOKEN_ITEM_H

#include <sys/types.h>

/*
   A token represents a single unit of the input, it can be a program name,
   its argument, metacharacters or control operators.
   Main tokens linked list is stored and handled only by main function.
   Tokens are read, analyzed and unified if required immediately after input.
   After these operations took place multiple commands are formed up which
   will contain all the information in order to call the actual program and
   contain a string that represents all the tokens that were used to create it.

   List of supported tokens:
   - argument
   - & (background process)
   - ; (command separator)
*/

typedef struct st_token_item
{
	enum { arg, bg_process, file_redirection, process_redirection,
		separator, and_op, or_op, unknown } type;
	enum { left, right, none } act_on;
	size_t id;
	char *str;
	struct st_token_item *next;
} st_token_item;

st_token_item *st_token_item_create_empty();
st_token_item *st_token_item_create(const char *str);
void st_token_item_delete(st_token_item *item);
void st_token_item_push_back(st_token_item **head, st_token_item *new_item);
int st_token_item_remove(st_token_item **head, st_token_item *item);
void st_token_item_print(const st_token_item *head);
void st_token_item_clear(st_token_item *head);
size_t st_token_item_size(const st_token_item *head);
size_t st_token_item_count(const st_token_item *head,
	int (*callback)(const st_token_item *));
size_t st_token_range_item_count(const st_token_item *head,
	const st_token_item *tail, int (*callback)(const st_token_item *));
void st_token_item_determine_type(st_token_item *item);

#endif /* !ST_TOKEN_ITEM_H */
