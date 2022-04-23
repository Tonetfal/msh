/* C89 support */
#define _XOPEN_SOURCE 500 /* snprintf */
#define _DEFAULT_SOURCE /* wait4 */

#include "io_utility.h"
#include "st_command.h"
#include "st_dll_string.h"
#include "st_token_item.h"
#include "tokens_utility.h"
#include "utility.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

st_command *cmds_head = NULL;
char post_execution_msg[16368];

size_t count_command_tokens(const st_token_item *head)
{
	size_t i = 0ul;
	for (; head; head = head->next, i++)
	{
		if (is_terminator_token(head))
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

size_t count_redirectors(const st_command *cmd)
{
	size_t count = 0ul, i = 0ul;
	st_redirector **redir = cmd->file_redirectors;
#ifdef DEBUG
	fprintf(stderr, "--------count_redirectors() - start\n");
#endif
	for (; redir[i]; i++)
		count++;
#ifdef DEBUG
	fprintf(stderr, "--------count_redirectors() - end count %lu\n", count);
#endif
	return count;
}

char *form_command_string(const st_token_item *head, const st_token_item *tail)
{
	char *str;
	size_t len = 0ul,
		tokens_len = st_token_range_str_length(head, tail),
		token_c = st_token_range_item_count(head, tail, NULL),
		totlen = tokens_len + token_c;
	str = (char *) malloc(totlen + 1);
#ifdef DEBUG
	fprintf(stderr, "form_command_string() - tokens_len %lu token_c %lu " \
		"totlen %lu\n", tokens_len, token_c, totlen);
#endif
	for (; head != tail->next; head = head->next)
		len += sprintf(str + len, "%s ", head->str);
	str[totlen - 1] = '\0';
#ifdef DEBUG
	fprintf(stderr, "form_command_string() - Formed string [%s]\n", str);
#endif
	return str;
}

void allocate_arguments_memory(const st_token_item *head,
	const st_token_item *tail, int *argc, char ***argv)
{
	*argc = (int) st_token_range_item_count(head, tail, &is_token_arg);
	*argv = (char **) malloc((*argc + 1) * sizeof(char *));
#ifdef DEBUG
	fprintf(stderr, "allocate_arguments_memory() - argc %i allocated bytes " \
		"%lu\n", *argc, (*argc + 1) * sizeof(char *));
#endif
}

void allocate_redirectors_memory(const st_token_item *head,
	const st_token_item *tail, st_command *cmd)
{
	size_t i, redir_c = st_token_range_item_count(head, tail,
		&is_token_redirector);
	cmd->file_redirectors = (st_file_redirector **) malloc(
		(redir_c + 1) * sizeof(st_file_redirector *));
	for (i = 0ul; i < redir_c + 1; i++)
		cmd->file_redirectors[i] = NULL;
#ifdef DEBUG
	fprintf(stderr, "allocate_redirectors_memory() - redir_c %lu allocated " \
		"bytes %lu\n", redir_c, (redir_c + 1) * sizeof(st_file_redirector *));
#endif
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
#ifdef DEBUG
	fprintf(stderr, "count_commands() - count %lu\n", count);
#endif

	return count;
}

void handle_command_redirector(const st_token_item *token, st_command *cmd)
{
	size_t redir_c = count_redirectors(cmd);
	st_redirector *redir = token->redir;
	assert(is_token_redirector(token));
	assert(token->next);
#ifdef DEBUG
	fprintf(stderr, "handle_command_redirector() - start redir_c %lu\n",
		redir_c);
	fprintf(stderr, "handle_command_redirector() - A new file redirector " \
		"has been added to a command \n\t path [%s] fd %d app %d dir %d\n",
		redir->path, redir->fd, redir->app, redir->dir);
#endif

	cmd->file_redirectors[redir_c] = redir;
	cmd->file_redirectors[redir_c + 1] = NULL; /* terminating null */
#ifdef DEBUG
	fprintf(stderr, "handle_command_redirector() - end\n");
#endif
}

void on_token_handle(const st_token_item *token, char ***argv, st_command *cmd)
{
	char *str;
#ifdef DEBUG
	fprintf(stderr, "on_token_handle() - start token - [%s] type - [%d]\n",
		token->str, token->type);
#endif
	switch (token->type)
	{
		case program:
		case arg:
			str = (char *) malloc(strlen(token->str) + 1);
			strcpy(str, token->str);
			**argv = str;
			(*argv)++;
			break;
		case bg_process:
			cmd->is_bg_process = 1;
			break;
		case separator:
		case redirector_path: /* is handled by file_redirector case */
			/* do nothing */
			break;
		case file_redirector:
			handle_command_redirector(token, cmd);
			break;
		default:
			fprintf(stderr, "A non implemented token has been passed - " \
				"String: [%s] Type: [%d]\n", token->str, token->type);
	}
#ifdef DEBUG
	fprintf(stderr, "on_token_handle() - end\n");
#endif
}

void handle_command_tokens(const st_token_item *head,
	const st_token_item *tail, st_command *cmd, char **argv)
{
	for (; tail->next != head; head = head->next)
		on_token_handle(head, &argv, cmd);
}

st_command *st_command_create_empty()
{
	st_command *cmd = (st_command *) malloc(sizeof(st_command));
	cmd->cmd_str = NULL;
	cmd->argc = 0;
	cmd->argv = NULL;
	cmd->is_bg_process = 0;
	cmd->file_redirectors = NULL;
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

	allocate_arguments_memory(head, tail, &argc, &argv);
	allocate_redirectors_memory(head, tail, cmd);
	handle_command_tokens(head, tail, cmd, argv);

	argv[argc] = NULL;
	cmd->argc = argc;
	cmd->argv = argv;
	cmd->cmd_str = form_command_string(head, tail);

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
	st_file_redirector **redir = cmd->file_redirectors;
	printf("\tcmd_str: [%s]\n", cmd->cmd_str);
	printf("\targc: %d\n", cmd->argc);

	printf("\targv: [ \n");
	for (i = 0ul; cmd->argv[i]; i++)
		printf("\t\t[%s], \n", cmd->argv[i]);
	printf("\t\t[%s]\n\t]\n", cmd->argv[i]);

	printf("\tis_bg_process: [%d]\n", cmd->is_bg_process);
	printf("\tpid: [%d]\n", cmd->pid);
	printf("\teid: [%d]\n", cmd->eid);

	printf("\tfile_redirectors: [ \n");
	for (i = 0ul; redir[i]; i++)
	{
		printf("\t\t[");
		st_redirector_print(redir[i]);
		printf("],\n");
	}
	printf("\t\t%p\n\t]\n", (void *) redir[i]);
}

void st_redirectors_delete(st_redirector **redirectors)
{
	for (; *redirectors; redirectors++)
		st_redirector_delete(*redirectors);
}

void st_command_delete(st_command *cmd)
{
	if (!cmd)
		return;
	if (cmd->argv)
		free(cmd->argv);
	if (cmd->file_redirectors)
	{
		st_redirectors_delete(cmd->file_redirectors);
		free(cmd->file_redirectors);
	}
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

int redirect_streams(st_command *cmd)
{
	int fd;
	size_t i;
	st_redirector **redic = cmd->file_redirectors;
	for (i = 0ul; redic[i]; i++)
	{
#ifdef DEBUG
		fprintf(stderr, "redirect_streams() - new iteration. redirector: ");
		st_redirector_print(redic[i]);
		putchar(10);
#endif
		fd = open(redic[i]->path,
			(redic[i]->dir == rd_out ? O_WRONLY : O_RDONLY) |
			(O_CREAT) |
			(redic[i]->app ? O_APPEND : redic[i]->dir == rd_in ? 0 : O_TRUNC),
			0666);
		if (fd == -1)
		{
			fprintf(stderr, "redirect_streams(%s): ", redic[i]->path);
			perror("");
			return 0;
		}
		/* redirect a stream */
		dup2(fd, redic[i]->fd);
	}
	return 1;
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
		if (!redirect_streams(cmd))
			return pid;
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
	pid_t pid;
#ifdef DEBUG
	fprintf(stderr, "--------handle_process() - start cmd [%s]\n", cmd->cmd_str);
#endif
	pid = call_command(cmd);

	cmd->pid = pid;
	cmd->eid = eid;
	if (!cmd->is_bg_process)
	{
		wait4(pid, &status, 0, NULL);
		output_exec_result(status);
	}
	else
		st_command_push_back(&cmds_head, cmd);

#ifdef DEBUG
	fprintf(stderr, "--------handle_process() - end\n");
#endif
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
		form_post_execution_msg(cmd);
		status = st_command_erase_item(&cmds_head, cmd);
#ifdef DEBUG
		fprintf(stderr, "check_zombies() - A command has ");
		if (!status)
			fprintf(stderr, "check_zombies() - not");
		fprintf(stderr, "check_zombies() - been erased from the list.\n");
#endif
	}
}

void form_post_execution_msg(const st_command *cmd)
{
	char *target = post_execution_msg;
	size_t maxlen = sizeof(post_execution_msg);
	size_t len = strlen(target);

	if (len > 0ul)
		len += snprintf(target + len, maxlen, "\n");
	snprintf(target + len, maxlen, "msh: Job %d, '%s' has ended",
		cmd->eid, cmd->cmd_str);
}

void clear_post_execution_msg()
{
	post_execution_msg[0] = '\0';
}
