#include "io_utility.h"
#include "log.h"
#include "mt_errno.h"
#include "st_command.h"
#include "st_dll_string.h"
#include "st_invite_msg.h"
#include "st_token.h"
#include "tokens_utility.h"

#include <stdio.h>
#include <unistd.h>

/*
	TOIMPLEMENT
	Left/Right key arrows to modify the written command in any position.
	Top/Down key arrows to leaf through command history.
	Tab to get suggestions.
*/

#define HANDLE_ERROR(code, str, head) \
	if (code == MT_ERR) \
	{ \
		mt_perror(NULL); \
		st_token_clear(head); \
		continue; \
	}

void chld_hdl(int s)
{
	signal(SIGCHLD, &chld_hdl);
	TRACEE("Signal %d has arrived\n", s);
	check_zombie();
	TRACEL("\n");
	UNUSED(s);
}

int main()
{
	int res, cont= 1;
	size_t i;
	char buf[4096], *str;
	st_invite_msg *invite_msg = NULL;
	st_token *tokens = NULL;
	st_command *cmds, *cmd;

	signal(SIGCHLD, &chld_hdl);
	st_command_init();
	clear_post_execution_msg();
	while (cont)
	{
		st_invite_msg_form(&invite_msg);
		st_invite_msg_print(invite_msg);
		str = read_string(buf, sizeof(buf));
		tokens = form_token_list(str);
		if (tokens)
		{
			remove_quotes(&tokens);
			unify_multiple_tokens(tokens);
#ifdef _TRACE
			printf("Tokens - {\n");
			st_token_print(tokens, TAB);
			printf("}\n");
#endif
			analyze_token_types(tokens);
			res = check_syntax(tokens);
			HANDLE_ERROR(res, NULL, &tokens);

			cmds = st_commands_create(tokens);
#ifdef _TRACE
			for (i = 0ul, cmd = cmds; cmd; i++, cmd = cmd->next)
			{
				printf("Command %lu - {\n", i + 1);
				st_command_print(cmd, TAB);
				printf("}\n");
			}
#endif
			st_command_pass_ownership(cmds);
			cmds = NULL;
			if (execute_commands() == -1)
			{
				cont = 0;
				break;
			}

			st_token_clear(&tokens);
		}

		res = printf("%s", post_execution_msg);
		clear_post_execution_msg();
		if (res)
			putchar(10);
	}
	printf("exit\n");
	st_invite_msg_delete(&invite_msg);
	st_commands_free();

	/* avoid warning on release build */
	UNUSED(cmd);
	UNUSED(i);
	return 0;
}

