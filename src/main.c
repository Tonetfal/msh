#include "io_utility.h"
#include "mt_errno.h"
#include "st_command.h"
#include "st_dll_string.h"
#include "st_invite_msg.h"
#include "st_token_item.h"
#include "tokens_utility.h"

#include <stddef.h>
#include <stdio.h>

#define HANDLE_ERROR(code, str, head) \
	if (code == MT_ERR) \
	{ \
		mt_perror(NULL); \
		st_token_item_clear(head); \
		continue; \
	}

int main()
{
	int res;
	size_t i;
	pid_t pid;
	char buf[4096], *str;
	st_invite_msg *invite_msg = NULL;
	st_token_item *tokens = NULL;
	st_command **cmds;

	for (;;)
	{
		st_invite_msg_form(&invite_msg);
		st_invite_msg_print(invite_msg);

		str = read_string(buf, sizeof(buf));
		if (!str)
			break;

		tokens = form_token_list(str);
		if (tokens)
		{
#ifdef DEBUG
			printf("Tokens - ");
			st_token_item_print(tokens);
#endif
			remove_quotes(&tokens);
			unify_multiple_tokens(tokens);
#ifdef DEBUG
			printf("Tokens - ");
			st_token_item_print(tokens);
#endif
			analyze_token_types(tokens);
			res = check_syntax(tokens);
			HANDLE_ERROR(res, NULL, tokens);

			cmds = st_commands_create(tokens);
#ifdef DEBUG
			for (i = 0ul; cmds[i]; i++)
			{
				printf("Command %lu - {\n", i + 1);
				st_command_print(cmds[i]);
				printf("}\n");
			}
#endif
			for (i = 0ul; cmds[i]; i++)
			{
				pid = execute_command(cmds[i]);
				if (pid == -1)
					return 0;
			}
			st_token_item_clear(tokens);
		}

		check_zombies();
		cmds = NULL;
	}
	putchar(10);

	return 0;
}

