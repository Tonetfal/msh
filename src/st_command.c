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
char post_execution_msg[16368] = {0};

size_t count_command_tokens(const st_token_item *head)
{
	size_t i = 0ul;
	for (; head; head = head->next, i++)
	{
		if (st_token_item_is_hard_delim(head))
			break;
	}
	if (head)
		i++;
	return i;
}

const st_token_item *find_command_tail(const st_token_item *head)
{
	size_t token_c = count_command_tokens(head);
	st_token_item_iterate(&head, token_c - 1ul);
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

void handle_process_redirector_presence(size_t *tokens_len, size_t *token_c,
	size_t *totlen)
{
	*tokens_len	-= 1ul;
	(*token_c)--;
	*totlen -= 2ul;
}

char *form_command_string(const st_token_item *head, const st_token_item *tail)
{
	int passed_tail = 0;
	char *str;
	size_t len = 0ul,
		tokens_len = st_token_range_str_length(head, tail),
		token_c = st_token_range_item_count(head, tail, NULL),
		totlen = tokens_len + token_c;
	if (st_token_item_range_contains(head, tail, process_redirector))
		handle_process_redirector_presence(&tokens_len, &token_c, &totlen);
	str = (char *) malloc(totlen + 1);
#ifdef DEBUG
	fprintf(stderr, "form_command_string() - tokens_len %lu token_c %lu " \
		"totlen %lu\n", tokens_len, token_c, totlen);
#endif
	for (; !passed_tail; head = head->next)
	{
		len += sprintf(str + len, "%s ", head->str);
		if (head == tail)
			passed_tail = 1;
	}
	str[totlen - 1ul] = '\0';
#ifdef DEBUG
	fprintf(stderr, "form_command_string() - Formed string [%s]\n", str);
#endif
	return str;
}

void allocate_arguments_memory(const st_token_item *head,
	const st_token_item *tail, int *argc, char ***argv)
{
	size_t bytes;
	*argc = (int) st_token_range_item_count(head, tail, &is_token_arg);
	bytes = (*argc + 1) * sizeof(char *);
	*argv = (char **) malloc(bytes);
#ifdef DEBUG
	fprintf(stderr, "allocate_arguments_memory() - argc %i allocated bytes " \
		"%lu\n", *argc, bytes);
#endif
}

void allocate_redirectors_memory(const st_token_item *head,
	const st_token_item *tail, st_command *cmd)
{
	size_t i,
		redir_c = st_token_range_item_count(head, tail, &is_token_redirector),
		bytes = (redir_c + 1) * sizeof(st_file_redirector *);
	cmd->file_redirectors = (st_file_redirector **) malloc(bytes);
	for (i = 0ul; i < redir_c + 1; i++)
		cmd->file_redirectors[i] = NULL;
#ifdef DEBUG
	fprintf(stderr, "allocate_redirectors_memory() - redir_c %lu allocated " \
		"bytes %lu\n", redir_c, bytes);
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

void handle_arg(const st_token_item *token, int *argc, char **argv)
{
	char *str;
	str = (char *) malloc(strlen(token->str) + 1ul);
	strcpy(str, token->str);
	argv[*argc] = str;
	argv[(*argc) + 1] = NULL;
	(*argc)++;
}

void handle_pipe(st_command *cmd)
{
	int pipefd[2];
	pipe(pipefd);
	cmd->pipefd[1] = pipefd[1];
	cmd->is_hidden_bg_process = 1;
}

void on_token_handle(const st_token_item *token, int *argc, char **argv,
	st_command *cmd)
{
#ifdef DEBUG
	fprintf(stderr, "on_token_handle() - start token - [%s] type - [%d]\n",
		token->str, token->type);
#endif
	switch (token->type)
	{
		case program:
		case arg:
			handle_arg(token, argc, argv);
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
		case process_redirector:
			handle_pipe(cmd);
			break;
		default:
			fprintf(stderr, "A non implemented token has been passed - " \
				"String: [%s] Type: [%d]\n", token->str, token->type);
			sleep(1); /* Just for debug purposes */
	}
#ifdef DEBUG
	fprintf(stderr, "on_token_handle() - end\n");
#endif
}

void handle_command_tokens(const st_token_item *head,
	const st_token_item *tail, st_command *cmd, char **argv)
{
	int argc = 0, passed_tail = 0;
	for (; !passed_tail; head = head->next)
	{
		on_token_handle(head, &argc, argv, cmd);
		if (head == tail)
			passed_tail = 1;
	}
}

void check_for_pipe(st_command *cmd, const st_command *prev_cmd)
{
	if (!prev_cmd || prev_cmd->pipefd[1] == -1)
		return;
	cmd->pipefd[0] = prev_cmd->pipefd[1] - 1;
}

st_command *st_command_create_empty()
{
	st_command *cmd = (st_command *) malloc(sizeof(st_command));
	cmd->cmd_str = NULL;
	cmd->argc = 0;
	cmd->argv = NULL;
	cmd->is_bg_process = 0;
	cmd->is_hidden_bg_process = 0;
	cmd->file_redirectors = NULL;
	cmd->pipefd[0] = -1;
	cmd->pipefd[1] = -1;
	cmd->pid = -1;
	cmd->eid = -1;
	cmd->next = NULL;
	cmd->prev = NULL;
	return cmd;
}

st_command *st_command_create(const st_token_item *head, st_command *prev_cmd)
{
	int argc;
	char **argv;
	const st_token_item *tail = find_command_tail(head);
	st_command *cmd = st_command_create_empty();
	if (!head)
		return cmd;

	allocate_arguments_memory(head, tail, &argc, &argv);
	allocate_redirectors_memory(head, tail, cmd);
	check_for_pipe(cmd, prev_cmd);
	handle_command_tokens(head, tail, cmd, argv);

	cmd->argc = argc;
	cmd->argv = argv;
	cmd->cmd_str = form_command_string(head, tail);
	cmd->prev = prev_cmd;

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
		cmds[i] = st_command_create(it, i > 0ul ? cmds[i - 1ul] : NULL);
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
	printf("\tpipefd: %d %d\n", cmd->pipefd[0], cmd->pipefd[1]);
	printf("\tNext: %p Prev: %p\n", (void *) cmd->next, (void *) cmd->prev);
}

void argv_delete(char **argv)
{
	int i = 0;
	if (!argv)
		return;
	for (; argv[i]; i++)
		free(argv[i]);
}

void st_redirectors_delete(st_redirector **redirectors)
{
	size_t i = 0ul;
	if (!redirectors)
		return;
	for (; redirectors[i]; i++)
		st_redirector_delete(redirectors[i]);
	free(redirectors);
}

void st_command_delete(st_command *cmd)
{
	if (!cmd)
		return;
	free_if_exists(cmd->cmd_str);
	argv_delete(cmd->argv);
	st_redirectors_delete(cmd->file_redirectors);
	free(cmd);
}

void st_commands_delete(st_command **cmds)
{
	size_t i = 0ul;
	if (!cmds)
		return;
	for (; cmds[i]; i++)
		st_command_delete(cmds[i]);
}

int output_exec_result(int status)
{
	if (status != 0)
	{
		/* if (WIFEXITED(status)) */
		/* 	printf("Terminated with %d", WEXITSTATUS(status)); */
		if (WIFSIGNALED(status))
			printf("Signal code %d", WTERMSIG(status));
	}
	return status != 0;
}

char *get_exec_result(char *buf, size_t len, int status)
{
	if (status != 0)
	{
		if (WIFEXITED(status))
			snprintf(buf, len, "Terminated with %d", WEXITSTATUS(status));
		if (WIFSIGNALED(status))
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

int redirect_file_stream(st_command *cmd)
{
	int fd;
	size_t i;
	st_redirector **redic = cmd->file_redirectors;
	for (i = 0ul; redic[i]; i++)
	{
#ifdef DEBUG
		fprintf(stderr, "redirect_file_stream() - new iteration. redirector: ");
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
			fprintf(stderr, "redirect_file_stream(%s): ", redic[i]->path);
			perror("Failed to open");
			return 1;
		}
		/* redirect a stream */
		dup2(fd, redic[i]->fd);
	}
	return 0;
}

#define ERROR() \
	if (res == -1) \
	{ \
		perror("dup2"); \
		return 1; \
	}

int redirect_process_stream(st_command *cmd)
{
	int res = 0;
	if (cmd->pipefd[0] != -1)
		res = dup2(cmd->pipefd[0], 0);
	ERROR();
	if (cmd->pipefd[1] != -1)
		res = dup2(cmd->pipefd[1], 1);
	ERROR();
	return 0;
}
#undef ERROR

int redirect_streams(st_command *cmd)
{
	if (cmd->file_redirectors && redirect_file_stream(cmd))
		return 1;
	if ((cmd->pipefd[0] != -1 || cmd->pipefd[1] != -1) &&
		redirect_process_stream(cmd))
		return 1;
	return 0;
}

pid_t call_command(st_command *cmd)
{
	pid_t pid = fork();
	if (pid == -1)
		perror("Failed to fork");
	else if (pid == 0)
	{
		cmd->pid = pid;
		if (redirect_streams(cmd))
		{
			fprintf(stderr, "call_command() - '%s' exit on " \
				"redirect_streams()", cmd->cmd_str);
			exit(1);
		}
		execvp(cmd->argv[0], cmd->argv);
		perror(cmd->cmd_str);
		return -1;
	}
	return pid;
}

void close_used_descriptors(st_command *cmd)
{
	int fd = cmd->pipefd[0];
	if (fd == -1)
		return;
	assert(cmd->prev);
	close(fd);
	close(fd - 1);
	cmd->pipefd[0] = -2;
	cmd->prev->pipefd[0] = -2;
}

pid_t handle_process(st_command *cmd)
{
	int status = 0, res;
#ifdef DEBUG
	fprintf(stderr, "--------handle_process() - start cmd [%s]\n",
		cmd->cmd_str);
#endif

	if (cmd->is_bg_process)
		cmd->eid =acquire_eid();
	cmd->pid = call_command(cmd);
	if (cmd->pid == -1)
	{
#ifdef DEBUG
	fprintf(stderr, "--------handle_process() - end by if pid == -1\n");
#endif
		st_command_delete(cmd);
		return 0;
	}
	if (!cmd->is_bg_process && !cmd->is_hidden_bg_process)
	{
		wait4(cmd->pid, &status, 0, NULL);
		res = output_exec_result(status);
		if (res)
			putchar(10);
	}
	else
		st_command_push_back(&cmds_head, cmd);
	close_used_descriptors(cmd);

#ifdef DEBUG
	fprintf(stderr, "--------handle_process() - end\n");
#endif
	return cmd->pid;
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
		new_item->prev = it;
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
	if (!(*head) || !item)
		return 0;
	if (*head == item)
	{
		*head = (*head)->next;
		if (*head)
			(*head)->prev = NULL;
	}
	else
	{
		item->prev->next = item->next;
		item->next->prev = item->prev;
	}
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
			fprintf(stderr, "not");
		fprintf(stderr, "been erased from the list.\n");
#endif
	}
}

void form_post_execution_msg(const st_command *cmd)
{
	char *target = post_execution_msg;
	size_t maxlen = sizeof(post_execution_msg);
	size_t len = strlen(target);

	/* a bg process can have eid -1 in case of pipe line and message about
	   its ending shouldn't be shown */
	if (cmd->eid == -1)
		return;

	if (len > 0ul)
		len += snprintf(target + len, maxlen, "\n");
	snprintf(target + len, maxlen, "msh: Job %d, '%s' has ended",
		cmd->eid, cmd->cmd_str);
}

void clear_post_execution_msg()
{
	post_execution_msg[0] = '\0';
}

void st_commands_free()
{
	st_commands_delete(&cmds_head);
}
