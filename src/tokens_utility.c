#include "mt_errno.h"
#include "st_redirector.h"
#include "st_token.h"
#include "tokens_utility.h"
#include "utility.h"
#include "log.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HDL_ERR(err, fmt, args) \
	do { \
		sprintf(mt_errstr, fmt, args); \
		return err; \
	} while (0)

#define TRACE_ERR(err, fmt, args) \
	do { \
		TRACEL("Return " #err "\n"); \
		HDL_ERR(err, fmt, args); \
	} while (0)

#define ERR_MSQUO(args) \
	TRACE_ERR(SC_MSQUO, "Mismatching quotes near '%s'.", args)
#define ERR_MSRTK(args) \
	TRACE_ERR(SC_MSRTK, "Syntax error near unexpected token '%s'.", args)
#define ERR_USPFD(args) \
	TRACE_ERR(SC_USPFD, "Unsupported file descriptor near '%s'.", args)
#define ERR_MSRDT(args) \
	TRACE_ERR(SC_MSRDT, "Mismatching redirector token near '%s'.", args)
#define ERR_MSREP(args) \
	TRACE_ERR(SC_MSREP, "Missing redirector path near '%s'.", args)

char is_special_character_ch(char c)
{
	const char arr[] = {
		' ', '"', '&', ';',
		'<', '>', '`', '|',
		'(', ')'
	};
	size_t i = 0ul;

	for (; i < sizeof(arr); i++)
	{
		if (arr[i] == c)
			return 1;
	}
	return 0;
}

char is_special_character_type(int type)
{
	const int types[] = {
		bg_process,
		file_redirector,
		process_redirector,
		separator
	};
	size_t i = 0ul;

	for (; i < sizeof(types) / sizeof(int); i++)
	{
		if (types[i] == type)
			return 1;
	}
	return 0;
}

int is_cmd_delim_str(const char *str)
{
	/* modify the row's length in case of adding a longer string
	   (don't forget about the terminating zero) */
#define LEN 3
	char cmd_delims[][LEN] = {
		"|",
		";",
		"&",
		/* "&&", */
		/* "||" */
	};
	size_t i = 0ul;

	for (; i < sizeof(cmd_delims) / LEN; i++)
		if (!strcmp(str, cmd_delims[i]))
			return 1;
	return 0;
}

int is_cmd_delim(const st_token *item)
{
	const int cmd_delims[] = {
		process_redirector,
		separator,
		bg_process,
		/*
		and_op,
		or_op
		*/
	};
	size_t i = 0ul;

	for (; i < sizeof(cmd_delims) / sizeof(int); i++)
	{
		if ((char) item->type == cmd_delims[i])
			return 1;
	}
	return 0;
}

st_token *find_cmd_delim(const st_token *item)
{
	if (!item)
		return NULL;
	if (is_cmd_delim(item))
		return (st_token *) item;
	return find_cmd_delim(item->next);
}

char *read_token(const char *str, char *last_ch, size_t *read_bytes,
	int *inside_quotes)
{
	size_t i;
	char *token, buf[4096];
	int backslash = 0;
	TRACEE("Passed str [%s] last_ch [%c] read_bytes [%lu] inside_quotes " \
		"[%d]\n", str, *last_ch, *read_bytes, *inside_quotes);

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
		else if (is_special_character_ch(*str))
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
	token = strdup(buf);
	TRACEL("Token [%s] last_ch [%c] read_bytes [%lu] inside_quotes [%d]\n",
		token, *last_ch, *read_bytes, *inside_quotes);
	return token;
}

st_token *form_token_list(const char *str)
{
	int exp_sp = 0;
	char *token, last_ch = '\0';
	size_t read_bytes = 0ul;
	st_token *head = NULL, *item;

	TRACEE("Str [%s]\n", str);
	while (str)
	{
		str += skip_spaces(str);
		if (*str == '\0')
			break;

		token = read_token(str, &last_ch, &read_bytes, &exp_sp);
		if (read_bytes == 0ul)
			break;
		str += read_bytes;

		item = st_token_create(token);
		st_token_push_back(&head, item);
		free(token);

		if (*str == '\0')
			break;
	}
	TRACEL("\n");

	return head;
}

void remove_quotes(st_token **head)
{
	st_token *it = *head, *tmp;

	TRACEE("\n");
	while (it)
	{
		if (!strcmp(it->str, "\""))
		{
			tmp = it->next;
			st_token_remove(head, it);
			it = tmp;
		}
		else
			it = it->next;
	}
	TRACEL("\n");
}

int can_be_unified(const st_token *item)
{
	const char tokens[] = {
		'&', '|', '>',
	};
	size_t i = 0ul;

	for (; i < sizeof(tokens); i++)
	{
		if (item->str[0] == tokens[i] && item->str[1] == '\0')
			return 1;
	}
	return 0;
}

void unify_multiple_tokens(st_token *head)
{
	TRACEE("\n");
	for (; head && head->next; head = head->next)
	{
		if (strcmp(head->str, head->next->str))
			continue;
		if (can_be_unified(head) && can_be_unified(head->next))
		{
			TRACE("Unify %s with %s\n",
				head->str, head->next->str);
			head->str = (char *) malloc(3);
			head->str[0] = *head->str;
			head->str[1] = *head->next->str;
			head->str[2] = '\0';
			st_token_remove(&head, head->next);
		}
	}
	TRACEL("\n");
}

int analyze_type(const st_token *item)
{
	if (!strcmp(item->str, "&"))
		return bg_process;
	else if (!strcmp(item->str, ";"))
		return separator;
	else if (
		!strcmp(item->str, "<") ||
		!strcmp(item->str, ">") ||
		!strcmp(item->str, ">>"))
		return file_redirector;
	else if (!strcmp(item->str, "|"))
		return process_redirector;
	/* else if (!strcmp(item->str, "||")) */
	/* 	return or_op; */
	/* else if (!strcmp(item->str, "&&")) */
	/* 	return and_op; */
	/* Probably won't be supported */
	/* else if (!strcmp(item->str, "(")) */
	/* 	return unknown; */
	/* else if (!strcmp(item->str, ")")) */
	/* 	return unknown; */
	else
		return arg;
}

void analyze_redirector(const char *str, int *fd, int *app, void *dir_v)
{
	int *dir = (int *) dir_v;
	if (!strcmp(str, "<"))
	{
		*fd = 0; /* stdin */
		*app = 0;
		*dir = rd_in;
	}
	else if (!strcmp(str, ">"))
	{
		*fd = 1; /* stdout */
		*app = 0;
		*dir = rd_out;
	}
	else if (!strcmp(str, ">>"))
	{
		*fd = 1; /* stdout */
		*app = 1;
		*dir = rd_out;
	}
}

void setup_redirector(st_token *item)
{
	st_redirector *redir = st_redirector_create_empty();

	/* use 'if' instead of 'assert' because syntax control is not yet done */
	if (item->next)
		redir->path = strdup(item->next->str);
	analyze_redirector(item->str, &redir->fd, &redir->app, &redir->dir);
	assert(redir->dir != rd_ukw);

	item->redir = redir;
}

void analyze_token_types(st_token *head)
{
	int type, last_type, expr_tokens = 0;
	TRACEE("\n");
	for (; head; head = head->next)
	{
		type = analyze_type(head);
		if (type == arg)
		{
			if (expr_tokens == 0)
			{
				type = program;
				TRACE("New expression is being analyzed\n");
			}
			else if (last_type == file_redirector)
			{
				type = redirector_path;
				TRACE("A redirector path has been found - [%s]\n", head->str);
			}
		}
		else if (type == file_redirector)
			setup_redirector(head);
		else if (is_cmd_delim_str(head->str))
		{
			TRACE("Last expression is formed up from %d tokens\n",
				expr_tokens);
			expr_tokens = -1; /* new expression reading must start with 0 */
		}

		head->type = type;
		last_type = type;
		expr_tokens++;
	}
	if (expr_tokens > 0)
		TRACE("Last expression is formed up from %d tokens\n", expr_tokens);
	TRACEL("\n");
}

int is_quote(st_token *item, void *data)
{
	UNUSED(data);
	return item->str[0] == '\'' && !item->str[1];
}

int check_quotes(const st_token *head)
{
	int res;
	size_t c;
	TRACEE("\n");
	c = st_token_count((st_token *) head, &is_quote, NULL);
	res = c % 2ul == 1ul;
	if (res)
	{
		TRACE("Quotes counter: %lu\n", c);
		ERR_MSQUO(head->str);
	}
	TRACEL("Quotes counter: %lu. Test was passed\n", c);
	return MT_OK;
}

int is_reserved(const st_token *item)
{
	if (!item)
		return 0;
	return
		!strcmp(item->str, "|") ||
		!strcmp(item->str, "&") ||
		!strcmp(item->str, ";") ||
		/* !strcmp(item->str, "(") || */
		/* !strcmp(item->str, ")") || */
		!strcmp(item->str, "<") ||
		!strcmp(item->str, ">") ||
		!strcmp(item->str, ">>");
		/* !strcmp(item->str, "||")|| */
		/* !strcmp(item->str, "&&"); */
}

int check_reserved_tokens(const st_token *head)
{
	int reserved;
	size_t i = 0ul;
	TRACEE("\n");
	for (; head; head = head->next, i++)
	{
		/* ignore ';' in this case, it's a standalone token */
		reserved = is_reserved(head) && strcmp(head->str, ";") != 0;
		TRACE("Token '%s'\n", head->str);
		if (reserved && i == 0ul)
			ERR_MSRTK(head->str);
		if (!reserved)
			continue;
		if (!head->next)
			break;

		/* No reserved token can be nearby any other one, except semi colon */
		if (!strcmp(head->str, "&") && !strcmp(head->next->str, ";"))
		{
			TRACE("Skip [%s]\n", head->str);
			head = head->next;
			continue;
		}
		if (head->next && is_reserved(head->next))
			ERR_MSRTK(head->str);
	}
	TRACEL("Test was passed\n");
	return MT_OK;
}

int check_file_redirectors(const st_token *head)
{
	int flags = 0, fd, last_type = -1;
	st_redirector *redir;
	const st_token *last_token = NULL;
	TRACEE("\n");
	for (; head; head = head->next)
	{
		if (head->type == file_redirector)
		{
			TRACE("Found a file redirector\n");
			redir = head->redir;
			if ((unsigned int) redir->fd >= sizeof(int))
				ERR_USPFD(head->str);
			fd = 1 << redir->fd;
			if (flags & fd)
				ERR_MSRDT(head->str);
			flags |= fd;
		}
		else if (last_type == file_redirector && head->type != redirector_path)
			ERR_MSREP(head->str);
		else if (is_cmd_delim(head))
		{
			TRACE("Found a cmd delimeter\n");
			flags = 0;
		}
		last_type = head->type;
		last_token = head;
	}
	if (last_type == file_redirector)
		ERR_MSREP(last_token->str);
	TRACEL("Test was passed\n");
	return MT_OK;
}

int check_process_redirectors(const st_token *head)
{
	int demand_program = 0, found_prg;
	st_token *cmd_delim_it;
	TRACEE("\n");
	for (; head; head = head->next)
	{
		cmd_delim_it = find_cmd_delim(head);
		if (!cmd_delim_it || (int) cmd_delim_it->type != process_redirector)
		{
			found_prg = !!st_token_find_range((st_token *) head,
				cmd_delim_it, st_token_contains, (void *) program);
			if (demand_program && !found_prg)
				ERR_MSRTK(head->str);
			if (cmd_delim_it)
				head = cmd_delim_it;
			demand_program = 0;
		}
		else
		{
			for (; head != cmd_delim_it; head = head->next)
				if (is_special_character_type((int) head->type))
					ERR_MSRTK(head->str);
			/* may become null and then outer for loop will do
			   head = head->next and cause a segm fault*/
			demand_program = 1;
		}
	}
	if (demand_program)
		ERR_MSRTK(head ? head->str : cmd_delim_it->str);
	TRACEL("Test was passed\n");
	return MT_OK;
}

int check_syntax(const st_token *head)
{
	TRACEE("\n");
	mt_errno = MT_OK;

	if (mt_errno == MT_OK)
		mt_errno = check_quotes(head);
	if (mt_errno == MT_OK)
		mt_errno = check_reserved_tokens(head);
	if (mt_errno == MT_OK)
		mt_errno = check_file_redirectors(head);
	if (mt_errno == MT_OK)
		mt_errno = check_process_redirectors(head);

	TRACEL("Syntax is right mt_errno %d\n", mt_errno);
	return mt_errno == MT_OK ? 0 : -1;
}
