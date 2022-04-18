#include "st_token_item.h"
#include "tokens_utility.h"
#include "utility.h"
#include "mt_errno.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char is_limiter(char c)
{
	const char arr[] = {
		' ', '"', '&', ';',
		'<', '>', '`', '|',
		'(', ')'
	};
	size_t i;

	for (i = 0ul; i < sizeof(arr); i++)
	{
		if (arr[i] == c)
			return 1;
	}
	return 0;
}

char *read_token(const char *str, char *last_ch, size_t *read_bytes,
	int *inside_quotes)
{
	size_t i;
	char *token, buf[4096];
	int backslash = 0;

	*read_bytes = 0ul;
	for (i = 0ul; str && *str != '\0'; str++, i++, (*read_bytes)++)
	{
		if (backslash)
		{
			i--;
			backslash = 0;
		}
		else if (*str == '\\')
			backslash = 1;
		else if (*str != '"' && *inside_quotes)
		{ /* Ignore special characters if inside quotes */ }
		else if (is_limiter(*str))
		{
			if (*str == '"')
				*inside_quotes = !(*inside_quotes);
			if (i == 0)
			{
				buf[i] = *str;
				*last_ch = *str;
				i++;
				(*read_bytes)++;
			}
			break;
		}

		if (!backslash)
			buf[i] = *str;
		*last_ch = *str;
	}

	buf[i] = '\0';
	token = (char *) malloc(i);
	strcpy(token, buf);
	return token;
}

st_token_item *form_token_list(const char *str)
{
	st_token_item *head = NULL, *item;
	char *token, last_ch;
	size_t read_bytes;
	int exp_sp = 0;

	while (str)
	{
		str += skip_spaces(str);
		if (*str == '\0')
			break;

		token = read_token(str, &last_ch, &read_bytes, &exp_sp);
		if (read_bytes == 0ul)
			break;
		str += read_bytes;

		item = st_token_item_create(token);
		st_token_item_push_back(&head, item);
		free(token);

		if (*str == '\0')
			break;
	}

	return head;
}

int is_quote(const st_token_item *item)
{
	if (!item)
		return 0;
	return strcmp(item->str, "\"") == 0;
}

void remove_quotes(st_token_item **head)
{
	st_token_item *it = *head, *tmp;
	while (it)
	{
		if (is_quote(it))
		{
			tmp = it->next;
			st_token_item_remove(head, it);
			it = tmp;
		}
		else
			it = it->next;
	}
}

int can_be_unified(const st_token_item *item)
{
	return
		strcmp(item->str, "&") == 0 ||
		strcmp(item->str, "|") == 0 ||
		strcmp(item->str, ">") == 0;
}

void unify_multiple_tokens(st_token_item *head)
{
	while (head && head->next)
	{
		if (can_be_unified(head) && can_be_unified(head->next))
		{
			head->str = (char *) malloc(3);
			head->str[0] = *head->next->str;
			head->str[1] = *head->next->str;
			head->str[2] = '\0';
			st_token_item_remove(&head, head->next);
		}
		head = head->next;
	}
}

int analyze_type(const st_token_item *item)
{
	if (strcmp(item->str, "|")  == 0)
		return unknown;
	else if (strcmp(item->str, "&") == 0)
		return bg_process;
	else if (strcmp(item->str, ";") == 0)
		return separator;
	else if (strcmp(item->str, "<") == 0)
		return unknown;
	else if (strcmp(item->str, ">") == 0)
		return unknown;
	else if (strcmp(item->str, ">>") == 0)
		return unknown;
	else if (strcmp(item->str, "||") == 0)
		return or_op;
	else if (strcmp(item->str, "&&") == 0)
		return and_op;
	/* Probably won't be supported */
	/* else if (strcmp(item->str, "(") == 0) */
	/* 	return unknown; */
	/* else if (strcmp(item->str, ")") == 0) */
	/* 	return unknown; */
	else
		return arg;
}

void analyze_token_types(st_token_item *head)
{
	while (head)
	{
		head->type = analyze_type(head);
		head = head->next;
	}
}

int check_quotes(const st_token_item *head)
{
	size_t count;
	count = st_token_item_count(head, &is_quote);
	return (count % 2ul) == 1ul;
}

int is_reserved(const st_token_item *item)
{
	if (!item)
		return 0;
	return
		strcmp(item->str, "|")  == 0 ||
		strcmp(item->str, "&")  == 0 ||
		strcmp(item->str, ";")  == 0 ||
		strcmp(item->str, "(")  == 0 ||
		strcmp(item->str, ")")  == 0 ||
		strcmp(item->str, "<")  == 0 ||
		strcmp(item->str, ">")  == 0 ||
		strcmp(item->str, ">>") == 0 ||
		strcmp(item->str, "||") == 0 ||
		strcmp(item->str, "&&") == 0;
}

int check_reserved_tokens(const st_token_item *head)
{
	size_t i = 0ul;
	int reserved;
	for (; head; i++)
	{
		/* ignore ';' in this case, it's a standalone token */
		reserved = strcmp(head->str, ";") != 0 && is_reserved(head);
		if ((reserved && i == 0ul) || (reserved && is_reserved(head->next)))
		{
			sprintf(mt_errstr, "Syntax error near unexpected token '%s'.",
				head->str);
			return 1;
		}
		head = head->next;
	}
	return 0;
}

int check_syntax(const st_token_item *head)
{
	mt_errno = MT_OK;

	if (check_quotes(head))
		mt_errno = SC_MSQUO;
	if (mt_errno == MT_OK && check_reserved_tokens(head))
		mt_errno = SC_MSRTK;

	return mt_errno == MT_OK ? 0 : -1;
}

void st_token_item_iterate(const st_token_item **head, size_t times)
{
	for (; times != 0ul; times--)
		*head = (*head)->next;
}
