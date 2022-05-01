#include "st_token.h"
#include "st_redirector.h"
#include "utility.h"
#include "log.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

st_token *st_token_create_empty()
{
	static size_t id = 0ul;
	st_token *node = (st_token *) malloc(sizeof(st_token));
	node->type = tk_unk;
	node->id = id;
	node->str = NULL;
	node->redir = NULL;
	node->next = NULL;
	id++;
	return node;
}

st_token *st_token_create(const char *str)
{
	st_token *node = NULL;
	TRACEE("Passed str [%s]\n", str);
	node = st_token_create_empty();
	if (!str)
		return node;

	node->str = strdup(str);
	TRACEL("Created token ptr %p its str [%s]\n",
		(void *) node, node->str);
	return node;
}

void st_token_delete(st_token **item)
{
	LOGFNPP(item);
	if (item && *item)
	{
		FREE_IFEX(&(*item)->str);
		st_redirector_delete(&(*item)->redir);
		FREE(*item);
	}
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
		FREE_IFEX(&(*head)->str);
		/* TODO free redirector ? */
		FREE(*head);
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
	/* TRACE("st_token_compare() - item->type %d == %d\n", item->type, *(int *) type); */
	/* return (int) item->type == *(int *) type; */
	TRACE("item->type %d == %ld\n", item->type, (long) type);
	return (int) item->type == (long) type;
}

int st_token_compare_not(st_token *item, void *type)
{
	return (int) item->type != (long) type;
}

