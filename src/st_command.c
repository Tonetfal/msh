/* C89 support of new features */
#define _XOPEN_SOURCE 500 /* snprintf */
#define _DEFAULT_SOURCE /* wait4 */

#include "io_utility.h"
#include "st_command.h"
#include "st_dll_string.h"
#include "st_token_item.h"
#include "tokens_utility.h"
#include "utility.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

st_command *cmds_head = NULL;

int is_token_arg(const st_token_item *item)
{
	return item->type == arg;
}

int is_token_not_arg(const st_token_item *item)
{
	return item->type != arg;
}

int is_command_terminator(const st_token_item *item)
{
	char *str = item->str;
	return
		strcmp(str, ";") == 0 ||
		strcmp(str, "&") == 0 ||
		strcmp(str, "&&") == 0 ||
		strcmp(str, "||") == 0;
}

size_t count_command_tokens(const st_token_item *head)
{
	size_t i = 0ul;
	for (; head; head = head->next, i++)
	{
		if (is_command_terminator(head))
			break;
	}
	if (head)
		i++;
	return i;
}

const st_token_item *find_command_tail(const st_token_item *head)
{
	st_token_item_iterate(&head, count_command_tokens(head) - 1ul);
	return head;
}

void on_identify(const st_token_item *token, char ***argv, st_command *cmd)
{
	char *str;
	switch (token->type)
	{
		case arg:
			str = (char *) malloc(strlen(token->str));
			strcpy(str, token->str);
			**argv = str;
			(*argv)++;
			break;
		case bg_process:
			cmd->is_bg_process = 1;
			break;
		case separator:
			/* do nothing */
			break;
		/* TO IMPLEMENT */
		/* case or_op: */
		/* 	break; */
		/* case and_op: */
		/* 	break; */
		default:
			fprintf(stderr, "A non implemented token has been passed - " \
				"String: [%s] Type: [%d]\n", token->str, token->type);
	}
}

size_t count_commands(const st_token_item *head)
{
	size_t count = st_token_item_count(head, &is_token_not_arg);
	if (count == 0ul) /* user may input a command without any special token */
	{
		count += !!head; /* if head is not null add 1 */
	}
	else
	{
		while (head->next)
			head = head->next;
		count += is_token_arg(head);
		/* if after last special token there is an arg add 1 */
	}

	return count;
}

char *form_command_string(const st_command *cmd)
{
	int i = 0, argc = cmd->argc;
	char *str, **argv = cmd->argv;

	size_t len = 0ul, totlen = get_argv_total_length(argc, argv) + argc;
	str = (char *) malloc(totlen);
#ifdef DEBUG
	fprintf(stderr, "form_command_string() - argc %d totlen %lu\n",
		argc, totlen);
#endif
	for (; i < argc; i++)
		len += sprintf(str + len, "%s ", argv[i]);
	str[totlen - 1] = '\0';
#ifdef DEBUG
	fprintf(stderr, "form_command_string() - Formed string [%s]\n", str);
#endif
	return str;
}

void command_compute_arguments(const st_token_item *head,
	const st_token_item *tail, int *argc, char ***argv)
{
	*argc = (int) st_token_range_item_count(head, tail, &is_token_arg);
	*argv = (char **) malloc((*argc + 1) * sizeof(char *));
#ifdef DEBUG
	fprintf(stderr, "command_compute_arguments() - argc %i allocated bytes " \
		"%lu\n", *argc, (*argc + 1) * sizeof(char *));
#endif
}

void command_identify_arguments(const st_token_item *head,
	const st_token_item *tail, st_command *cmd, char **argv)
{
	while (tail->next != head)
	{
#ifdef DEBUG
		fprintf(stderr, "command_identify_arguments() - [%s]\n", head->str);
#endif
		on_identify(head, &argv, cmd);
		head = head->next;
	}
}

st_command *st_command_create_empty()
{
	st_command *cmd = (st_command *) malloc(sizeof(st_command));
	cmd->cmd_str = NULL;
	cmd->argc = 0;
	cmd->argv = NULL;
	cmd->is_bg_process = 0;
	cmd->pid = -1;
	cmd->eid = -1;
	cmd->next = NULL;
	return cmd;
}

st_command *st_command_create(const st_token_item *head)
{
	int argc;
	char **argv;
	const st_token_item *tail = find_command_tail(head);
	st_command *cmd = st_command_create_empty();
	if (!head)
		return cmd;

	command_compute_arguments(head, tail, &argc, &argv);
	command_identify_arguments(head, tail, cmd, argv);

	argv[argc] = NULL;
	cmd->argc = argc;
	cmd->argv = argv;
	cmd->cmd_str = form_command_string(cmd);

#ifdef DEBUG
	fputs("st_command_create() - New command has been created\n", stderr);
	st_command_print(cmd);
#endif

	return cmd;
}

st_command **st_commands_create(const st_token_item *head)
{
	size_t cmdc, tokenc, i;
	st_command **cmds;
	const st_token_item *it = head;

	/* may count more than there actually are due some non arg tokens which
	   don't call any program */
	cmdc = count_commands(head);
	cmds = (st_command **) malloc((cmdc + 1) * (sizeof(st_command *)));

#ifdef DEBUG
	fprintf(stderr, "st_commands_create() - cmdc %lu\n", cmdc);
#endif
	for (i = 0ul; it; i++)
	{
		cmds[i] = st_command_create(it);
		tokenc = count_command_tokens(it);
		st_token_item_iterate(&it, tokenc);

		/* overwrite an empty on text iteration */
		if (cmds[i]->argc == 0ul)
			i--;
	}
	cmds[i] = NULL;
#ifdef DEBUG
	if (cmdc - i > 0ul)
		printf("st_command_create() - cmdc - i = %lu (amount of unused " \
			"pointers) \n", cmdc - i);
#endif

	return cmds;
}

void st_command_print(const st_command *cmd)
{
	size_t i;
	printf("cmd_str [%s]\n", cmd->cmd_str);
	printf("argc: %d\n", cmd->argc);
	printf("argv: [ ");
	for (i = 0ul; cmd->argv[i]; i++)
		printf("[%s] ", cmd->argv[i]);
	printf("[%s] ]\n", cmd->argv[i]);
	printf("is_bg_process: %d\n", cmd->is_bg_process);
	printf("pid: [%d]\n", cmd->pid);
	printf("eid: [%d]\n", cmd->eid);
}

void st_command_delete(st_command *cmd)
{
	if (!cmd)
		return;
	if (cmd->argv)
		free(cmd->argv);
	free(cmd);
}

void st_commands_delete(st_command **cmds)
{
	for (; *cmds; cmds++)
		st_command_delete(*cmds);
}

void output_exec_result(int status)
{
	if (status != 0)
	{
		/* if (WIFEXITED(status)) */
		/* 	printf("Terminated with %d", WEXITSTATUS(status)); */
		if (WIFSIGNALED(status))
		{
			printf("Signal code %d", WTERMSIG(status));
			putchar(10);
		}
	}
}

char *get_exec_result(char *buf, size_t len, int status)
{
	if (status != 0)
	{
		if (WIFEXITED(status))
			snprintf(buf, len, "Terminated with %d", WEXITSTATUS(status));
		else
			snprintf(buf, len, "Signal code %d", WTERMSIG(status));
	}
	return buf;
}

st_dll_string *find_higher(st_dll_string *to_iterate, st_dll_string *to_insert)
{
	while (to_iterate && stricmp(to_iterate->str, to_insert->str) < 0)
		to_iterate = to_iterate->next;
	return to_iterate;
}

void print_sorted_environ()
{
	st_dll_string *node, *head = NULL;
	size_t i;
	for (i = 0ul; __environ[i]; i++)
	{
		node = st_dll_string_create(__environ[i]);
		st_dll_insert_on_callback(&head, node, &find_higher);
	}
	st_dll_string_print(head);
	st_dll_string_clear(head);
}

void handle_export(const st_command *cmd)
{
	if (cmd->argc == 1)
		print_sorted_environ();
}

void handle_cd(const st_command *cmd)
{
	int res;
	const char *envvar = NULL;

	if (cmd->argc > 1)
		res = chdir(cmd->argv[1]);
	else
	{
		envvar = getenv("HOME");
		if (!envvar)
		{
			perror("Failed to get an environment variable\n");
			exit(1);
		}
		res = chdir(envvar);
	}

	if (res == -1)
		perror("Failed to change directory");
}

pid_t call_command(st_command *cmd)
{
	char prg[256];
	pid_t pid = fork();
	if (pid == -1)
	{
		perror("Failed to fork");
		exit(1);
	}
	if (pid == 0)
	{
		cmd->pid = pid;
		strcpy(prg, cmd->argv[0]);
		execvp(prg, cmd->argv);
		perror(prg);
		exit(1);
	}
	return pid;
}

pid_t handle_process(st_command *cmd)
{
	int status;
	eid_t eid = acquire_eid();
	pid_t pid = call_command(cmd);

	cmd->pid = pid;
	cmd->eid = eid;
	if (!cmd->is_bg_process)
	{
		wait4(pid, &status, 0, NULL);
		output_exec_result(status);
	}
	else
		st_command_push_back(&cmds_head, cmd);

	return pid;
}

void st_command_push_back(st_command **head, st_command *new_item)
{
	if (!(*head))
		*head = new_item;
	else
	{
		st_command *it = *head;
		while (it->next)
			it = it->next;
		it->next = new_item;
	}
}

st_command *st_command_find(st_command *head, pid_t pid)
{
	for (; head; head = head->next)
	{
		if (head->pid == pid)
			return head;
	}
	return NULL;
}

int st_command_erase_item(st_command **head, st_command *item)
{
	st_command *it;
	if (!(*head) || !item)
		return 0;
	if (*head == item)
	{
		*head = (*head)->next;
		st_command_delete(item);
		return 1;
	}

	it = *head;
	while (it->next && it->next != item)
		it = it->next;
	if (!it->next)
		return 0;

	it->next = item->next;
	st_command_delete(item);
	return 1;
}

pid_t execute_command(st_command *cmd)
{
	pid_t pid = 0;
	if (strcmp(cmd->argv[0], "cd") == 0)
		handle_cd(cmd);
	else if (strcmp(cmd->argv[0], "export") == 0)
		handle_export(cmd);
	else if (strcmp(cmd->argv[0], "exit") == 0)
		pid = -1;
	else
		pid = handle_process(cmd);
	return pid;
}

void print_terminate_message(const st_command *cmd)
{
	printf("msh: Job %d, '", cmd->eid);
	print_argv(cmd->argv);
	printf("' has ended\n");
}

void check_zombies()
{
	int status;
	char buf[512];
	pid_t pid;
	st_command *cmd;
	while ((pid = wait4(-1, &status, WNOHANG, NULL)) > 0)
	{
		cmd = st_command_find(cmds_head, pid);
		assert(cmd);

		get_exec_result(buf, sizeof(buf), status);
		print_terminate_message(cmd);

		status = st_command_erase_item(&cmds_head, cmd);
#ifdef DEBUG
		fprintf(stderr, "A command has ");
		if (!status)
			fprintf(stderr, "not ");
		fprintf(stderr, "been erased from the list.\n");
#endif
	}
}
