#ifndef ST_TOKEN_ITEM_H
#define ST_TOKEN_ITEM_H

#include "def.h"

/*
   A token represents a single unit of the input, it can be a program name,
   its argument, metacharacters or control operators.
   Main tokens linked list is stored and handled only by main function.
   Tokens are read, analyzed and unified if required immediately after input.
   After these operations took place multiple commands are formed up which
   will contain all the information (in order to call the actual program) and
   a string that represents all the tokens that were used to create it.

   List of supported tokens:
   - argument
   - & (background process)
   - ; (command separator)
   - >, <, >> (redirection only into files)
   - | (pipe line)
*/

struct st_token
{
	enum
	{
		program,
		arg,
		bg_process,
		file_redirector,
		process_redirector,
		redirector_path,
		separator,
		tk_unk
	} type;
	size_t id;
	char *str;
	st_redirector *redir;
	st_token *next;
};

typedef void (*tkn_vcallback_t)(st_token *, void *);
typedef st_token *(*tkn_scallback_t)(st_token *, void *);
typedef int (*tkn_icallback_t)(st_token *, void *);

st_token *st_token_create_empty();
st_token *st_token_create(const char *str);
void st_token_delete(st_token **item);
void st_token_push_back(st_token **head, st_token *new_item);
int st_token_remove(st_token **head, st_token *item);
void st_token_print(const st_token *head, const char *prefix);
void st_token_clear(st_token **head);
void st_token_iterate(const st_token **head, size_t times);

void st_token_traverse(st_token *head, tkn_vcallback_t callback, void *userdata);
void st_token_traverse_range(st_token *head, st_token *tail,
	tkn_vcallback_t callback, void *userdata);
st_token *st_token_find(st_token *head, tkn_scallback_t callback, void *userdata);
st_token *st_token_find_range(st_token *head, st_token *tail,
	tkn_scallback_t callback, void *userdata);
size_t st_token_count(const st_token *head, tkn_icallback_t callback,
	void *userdata);
size_t st_token_count_range(const st_token *head, const st_token *tail,
	tkn_icallback_t callback, void *userdata);

st_token *st_token_contains(st_token *item, void *type);
int st_token_compare(st_token *item, void *type);
int st_token_compare_not(st_token *item, void *type);

#endif /* !ST_TOKEN_ITEM_H */
