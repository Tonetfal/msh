#include "st_dll_string.h"
#include "utility.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

st_dll_string *st_dll_string_create_empty()
{
	st_dll_string *head = (st_dll_string *) malloc(sizeof(st_dll_string));
	head->str = NULL;
	head->next = NULL;
	head->prev = NULL;
	return head;
}

st_dll_string *st_dll_string_create(const char *str)
{
	st_dll_string *head = st_dll_string_create_empty();
	if (!str)
		return head;

	head->str = (char *) malloc(strlen(str) + 1);
	strcpy(head->str, str);

	return head;
}

void st_dll_string_push_back(st_dll_string **head, st_dll_string *new_item)
{
	if (!(*head))
		*head = new_item;
	else
	{
		st_dll_string *it = *head;
		while (it->next)
			it = it->next;
		it->next = new_item;
		new_item->prev = it;
	}
}

void st_dll_string_push_front(st_dll_string **head, st_dll_string *new_item)
{
	if (!(*head))
		*head = new_item;
	else
	{
		st_dll_string *it = *head;
		while (it->prev)
			it = it->prev;
		it->prev = new_item;
		new_item->next = it;
	}
}

void st_dll_insert_on_callback(st_dll_string **head, st_dll_string *new_item,
	st_dll_string *(*callback)(st_dll_string*, st_dll_string*))
{
	if (!(*head))
		*head = new_item;
	else
	{
		st_dll_string *found = (callback)(*head, new_item);

		if (found)
		{
			if (found->prev)
				found->prev->next = new_item;
			new_item->next = found;
			new_item->prev = found->prev;
			found->prev = new_item;
			if (*head == found)
				*head = (*head)->prev;
		}
		else
			st_dll_string_push_back(head, new_item);
	}
}

void st_dll_string_print(const st_dll_string *head)
{
	while (head)
	{
		printf("%s\n", head->str);
		head = head->next;
	}
}

void st_dll_string_clear(st_dll_string *head)
{
	while (head)
	{
		st_dll_string *it = head;
		head = head->next;
		if (it->str)
			free(it->str);
		free(it);
	}
}

void swap(char **lhs, char **rhs)
{
	char *tmp = *lhs;
	*lhs = *rhs;
	*rhs = tmp;
}

/* Bubble sort */
void st_dll_string_sort(st_dll_string **head)
{
	size_t i, j, size;
	st_dll_string *it;
	size = st_dll_string_size(*head);

	for (i = 0ul; i < size; i++)
	{
		it = *head;
		for (j = i; j < size - 1; j++)
		{
			if (stricmp(it->str, it->next->str) > 0)
				swap(&it->str, &it->next->str);
			it = it->next;
		}
	}
}

size_t st_dll_string_size(const st_dll_string *head)
{
	size_t i = 0ul;
	while (head)
	{
		head = head->next;
		i++;
	}
	return i;
}
