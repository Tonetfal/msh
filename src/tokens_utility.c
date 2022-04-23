#include "st_token_item.h"
#include "tokens_utility.h"
#include "utility.h"
#include "mt_errno.h"

#include <assert.h>
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
#ifdef DEBUG
	fprintf(stderr, "--------read_token() start str [%s] last_ch [%c] " \
		"read_bytes [%lu] inside_quotes [%d]\n", str, *last_ch, *read_bytes,
		*inside_quotes);
#endif

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
			/* Don't drop the flag if quote is not the first character*/
			if (*str == '"' && i == 0ul)
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
	token = (char *) malloc(i + 1);
	strcpy(token, buf);
#ifdef DEBUG
	fprintf(stderr, "--------read_token() end token [%s] last_ch [%c] " \
		"read_bytes [%lu] inside_quotes [%d]\n", token, *last_ch, *read_bytes,
		*inside_quotes);
#endif
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
	if (strcmp(item->str, "&") == 0)
		return bg_process;
	else if (strcmp(item->str, ";") == 0)
		return separator;
	else if (
		strcmp(item->str, "<") == 0 ||
		strcmp(item->str, ">") == 0 ||
		strcmp(item->str, ">>") == 0)
		return file_redirector;

	/* else if (strcmp(item->str, "|") == 0) */
	/* 	return unknown; */
	/* else if (strcmp(item->str, "||") == 0) */
	/* 	return or_op; */
	/* else if (strcmp(item->str, "&&") == 0) */
	/* 	return and_op; */
	/* Probably won't be supported */
	/* else if (strcmp(item->str, "(") == 0) */
	/* 	return unknown; */
	/* else if (strcmp(item->str, ")") == 0) */
	/* 	return unknown; */
	else
		return arg;
}

void analyze_redirector(const char *str, int *fd, int *app, void *dir_v)
{
	int *dir = (int *) dir_v;
	if (strcmp(str, "<") == 0)
	{
		*fd = 0; /* stdin */
		*app = 0;
		*dir = rd_in;
	}
	else if (strcmp(str, ">") == 0)
	{
		*fd = 1; /* stdout */
		*app = 0;
		*dir = rd_out;
	}
	else if (strcmp(str, ">>") == 0)
	{
		*fd = 1; /* stdout */
		*app = 1;
		*dir = rd_out;
	}
}

void setup_redirector(st_token_item *item)
{
	char *redir_arg;
	st_redirector *redir = st_redirector_create_empty();

	/* use 'if' instead of 'assert' because syntax control is not yet done */
	if (item->next)
	{
		redir_arg = (char *) malloc(strlen(item->next->str) + 1);
		redir->path = strcpy(redir_arg, item->next->str);
	}
	analyze_redirector(item->str, &redir->fd, &redir->app, &redir->dir);
	assert(redir->dir != rd_ukw);

	item->redir = redir;
}

void analyze_token_types(st_token_item *head)
{
	int type, last_type, expr_tokens = 0;
	for (; head; head = head->next)
	{
		type = analyze_type(head);
		if (type == arg)
		{
			if (expr_tokens == 0)
			{
				type = program;
#ifdef DEBUG
				printf("New expression is being analyzed\n");
#endif
			}
			else if (last_type == file_redirector)
			{
				type = redirector_path;
#ifdef DEBUG
				printf("A redirector path has been found - [%s]\n",
					head->str);
#endif
			}
		}
		else if (type == file_redirector)
			setup_redirector(head);
		else if (is_terminator_token(head))
		{
#ifdef DEBUG
			printf("Last expression is formed up from %d tokens\n", expr_tokens);
#endif
			expr_tokens = -1; /* new expression reading must start with 0 */
		}

		head->type = type;
		last_type = type;
		expr_tokens++;
	}
#ifdef DEBUG
	if (expr_tokens > 0)
		printf("Last expression is formed up from %d tokens\n", expr_tokens);
#endif
}

int check_quotes(const st_token_item *head)
{
	size_t count;
	count = st_token_item_count(head, &is_quote);
	return (count % 2ul) == 1ul ? SC_MSQUO : MT_OK;
}

int is_reserved(const st_token_item *item)
{
	if (!item)
		return 0;
	return
		/* strcmp(item->str, "|")  == 0 || */
		strcmp(item->str, "&")  == 0 ||
		strcmp(item->str, ";")  == 0 ||
		/* strcmp(item->str, "(")  == 0 || */
		/* strcmp(item->str, ")")  == 0 || */
		strcmp(item->str, "<")  == 0 ||
		strcmp(item->str, ">")  == 0 ||
		strcmp(item->str, ">>") == 0;
		/* strcmp(item->str, "||") == 0 || */
		/* strcmp(item->str, "&&") == 0; */
}

int check_reserved_tokens(const st_token_item *head)
{
	size_t i = 0ul;
	int reserved;
	for (; head && head->next; head = head->next, i++)
	{
		/* ignore ';' in this case, it's a standalone token */
		reserved = strcmp(head->str, ";") != 0 && is_reserved(head);
		if (strcmp(head->str, "&") == 0 && strcmp(head->next->str, ";") == 0)
		{
			head = head->next;
			continue;
		}
		if ((reserved && i == 0ul) || (reserved && is_reserved(head->next)))
		{
			sprintf(mt_errstr, "Syntax error near unexpected token '%s'.",
				head->str);
			return SC_MSRTK;
		}
	}
	return MT_OK;
}

int check_redirector_tokens(const st_token_item *head)
{
	int flags = 0, fd, last_type = -1;
	st_redirector *redir;
	for (; head; head = head->next)
	{
		if (head->type == file_redirector)
		{
			redir = head->redir;
			if ((unsigned int) redir->fd >= sizeof(int))
				return SC_USPFD;
			fd = 1 << redir->fd;
			if (flags & fd)
				return SC_MSRDT;
			flags |= fd;
		}
		else if (last_type == file_redirector &&
			head->type != redirector_path)
		{
			return SC_MSREP;
		}
		else if (is_terminator_token(head))
		{
			flags = 0;
		}
		last_type = head->type;
	}
	return MT_OK;
}

int check_syntax(const st_token_item *head)
{
	mt_errno = MT_OK;

	if (mt_errno == MT_OK)
		mt_errno = check_quotes(head);
	if (mt_errno == MT_OK)
		mt_errno = check_reserved_tokens(head);
	if (mt_errno == MT_OK)
		mt_errno = check_redirector_tokens(head);

	return mt_errno == MT_OK ? 0 : -1;
}

void st_token_item_iterate(const st_token_item **head, size_t times)
{
	for (; times != 0ul; times--)
		*head = (*head)->next;
}

int is_token_arg(const st_token_item *item)
{
	return item->type == arg || item->type == program;
}

int is_token_not_arg(const st_token_item *item)
{
	return !is_token_arg(item);
}

int is_terminator_token(const st_token_item *item)
{
	char *str = item->str;
	return
		strcmp(str, ";") == 0 ||
		strcmp(str, "&") == 0 ||
		strcmp(str, "&&") == 0 ||
		strcmp(str, "||") == 0;
}

int is_token_redirector(const st_token_item *item)
{
	return item->type == file_redirector;
}
