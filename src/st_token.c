#include "st_token.h"
#include "st_redirector.h"
#include "utility.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

st_token *st_token_create_empty()
{
	static size_t id = 0ul;
	st_token *item = (st_token *) malloc(sizeof(st_token));
	ATRACEE(item);
	item->type = tk_unk;
	item->id = id;
	item->str = NULL;
	item->redir = NULL;
	item->next = NULL;
	id++;
	TRACEL("\n");
	return item;
}

st_token *st_token_create(const char *str)
{
	st_token *item = NULL;
	TRACEE("Passed str [%s]\n", str);
	item = st_token_create_empty();
	if (!str)
	{
		TRACEL("An empty token has been created due null str\n");
		return item;
	}

	item->str = strdup(str);
	item->redir = st_redirector_create_empty();
	TRACEL("Created token ptr %p its str [%s]\n", (void *) item, item->str);
	return item;
}

void st_token_delete(st_token **head)
{
	LOGFNPP(head);
	FREE_IFEX((*head)->str);
	st_redirector_delete(&(*head)->redir);
	FREE(*head);
	TRACEL("\n");
}

void st_token_push_back(st_token **head, st_token *new_item)
{
	if (!(*head))
		*head = new_item;
	else if (!(*head)->next)
		(*head)->next = new_item;
	else
		st_token_push_back(&(*head)->next, new_item);
}

int st_token_remove(st_token **head, st_token *item)
{
	if (!(*head) || !item)
		return 0;
	if (*head == item)
	{
		*head = (*head)->next;
		st_token_delete(&item);
		return 1;
	}
	return st_token_remove(&(*head)->next, item);
}

void st_token_print(const st_token *head, const char *prefix)
{
	printf("%s", prefix ? prefix : "");
	for (; head; head = head->next)
		printf("[%s] ", head->str);
	printf("%p\n", (void *) head);
}

void st_token_clear(st_token **head)
{
	LOGCFNPP(head);
	if (*head)
	{
		st_token_clear(&(*head)->next);
		st_token_delete(head);
	}
	TRACELC("\n");
}

void st_token_iterate(const st_token **head, size_t times)
{
	for (; times != 0ul; times--)
		(*head) = (*head)->next;
}

void st_token_traverse(st_token *head, tkn_vcallback_t callback, void *userdata)
{
	st_token_traverse_range(head, NULL, callback, userdata);
}

void st_token_traverse_range(st_token *head, st_token *tail,
	tkn_vcallback_t callback, void *userdata)
{
	if (!head)
		return;
	(*callback)(head, userdata);
	if (head == tail)
		return;
	st_token_traverse_range(head->next, tail, callback, userdata);
}

st_token *st_token_find(st_token *head, tkn_scallback_t callback, void *userdata)
{
	return st_token_find_range(head, NULL, callback, userdata);
}

st_token *st_token_find_range(st_token *head, st_token *tail,
	tkn_scallback_t callback, void *userdata)
{
	st_token *res;
	if (!head)
		return NULL;
	res = callback(head, userdata);
	if (res)
		return res;
	if (head == tail)
		return NULL;
	return st_token_find(head->next, callback, userdata);
}

size_t st_token_count(const st_token *head, tkn_icallback_t callback,
	void *userdata)
{
	return st_token_count_range(head, NULL, callback, userdata);
}

size_t st_token_count_range(const st_token *head, const st_token *tail,
	tkn_icallback_t callback, void *userdata)
{
	size_t c = 0ul;
	for (; head; head = head->next)
	{
		/* if no callback has been passed return total amount of elements */
		if (!callback || (*callback)((st_token *) head, userdata))
			c++;
		if (head == tail)
			return c;
	}
	return c;
}

st_token *st_token_contains(st_token *item, void *type)
{
	return (int) item->type == (long) type ? item : NULL ;
}

int st_token_compare(st_token *item, void *type)
{
	return (int) item->type == (long) type;
}

int st_token_compare_not(st_token *item, void *type)
{
	return (int) item->type != (long) type;
}

