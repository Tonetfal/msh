#include "st_token_item.h"
#include "st_redirector.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

st_token_item *st_token_item_create_empty()
{
	static size_t id = 0ul;
	st_token_item *node = (st_token_item *) malloc(sizeof(st_token_item));
	node->type = tk_unk;
	node->id = id;
	node->str = NULL;
	node->redir = NULL;
	node->next = NULL;
	id++;
	return node;
}

st_token_item *st_token_item_create(const char *str)
{
	st_token_item *node = st_token_item_create_empty();
	if (!str)
		return node;

	node->str = (char *) malloc(strlen(str) + 1);
	strcpy(node->str, str);

	return node;
}

void st_token_item_delete(st_token_item *item)
{
	if (!item)
		return;
	if (item->str)
		free(item->str);
	if (item->redir)
		st_redirector_delete(item->redir);
	free(item);
}

void st_token_item_push_back(st_token_item **head, st_token_item *new_item)
{
	if (!(*head))
		*head = new_item;
	else
	{
		st_token_item *tmp = *head;
		while (tmp->next)
			tmp = tmp->next;
		tmp->next = new_item;
	}
}

int st_token_item_remove(st_token_item **head, st_token_item *item)
{
	st_token_item *it;
	if (!(*head) || !item)
		return 0;
	if (*head == item)
	{
		*head = (*head)->next;
		st_token_item_delete(item);
		return 1;
	}

	it = *head;
	while (it->next && it->next != item)
		it = it->next;
	if (!it->next)
		return 0;

	it->next = item->next;
	st_token_item_delete(item);
	return 1;
}

void st_token_item_print(const st_token_item *head)
{
	int empty = head == NULL;
	for (; head; head = head->next)
		printf("[%s] ", head->str);
	if (!empty)
		putchar(10);
}

void st_token_item_clear(st_token_item *head)
{
	while (head)
	{
		st_token_item *tmp = head;
		head = head->next;
		if (tmp->str)
			free(tmp->str);
		free(tmp);
	}
}

size_t st_token_item_size(const st_token_item *head)
{
	size_t i = 0ul;
	for (; head; head = head->next)
		i++;
	return i;
}

size_t st_token_item_count(const st_token_item *head,
	int (*callback)(const st_token_item *))
{
	return st_token_range_item_count(head, NULL, callback);
}

size_t st_token_range_item_count(const st_token_item *head,
	const st_token_item *tail,	int (*callback)(const st_token_item *))
{
#define FOR_BODY \
	do { \
		if (!callback || (callback)(head)) \
			count++; \
	} while(0);

	size_t count = 0ul;
	assert(head);
	if (!tail)
		for (; head; head = head->next)
			FOR_BODY
	else
		for (; head != tail->next; head = head->next)
			FOR_BODY

	return count;
#undef FOR_BODY
}

size_t st_token_range_str_length(const st_token_item *head,
	const st_token_item *tail)
{
#define FOR_BODY len += strlen(head->str);

	size_t len = 0ul;
	assert(head);
	if (!tail)
		for (; head; head = head->next)
			FOR_BODY
	else
		for (; head != tail->next; head = head->next)
			FOR_BODY

	return len;
#undef FOR_BODY
}
