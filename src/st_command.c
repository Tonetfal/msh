#define _XOPEN_SOURCE 500 /* snprintf, usleep */
#define _DEFAULT_SOURCE /* wait4 */

#include "io_utility.h"
#include "st_command.h"
#include "st_dll_string.h"
#include "st_redirector.h"
#include "st_token.h"
#include "tokens_utility.h"
#include "utility.h"
#include "log.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define SKIP_TIME_SLICE() usleep(1000)
#define WAIT_FOR(flag) while (flag) SKIP_TIME_SLICE()
#define RAISE_ZGUARD() TRACE("\n"); zombie_guard = 1
#define DROP_ZGUARD() TRACE("\n"); zombie_guard = 0
#define CLOSE_FD(fd) \
	do \
	{ \
		if (close(fd) == -1) \
		{ \
			TRACE("CLOSE_FD %d: ", fd); \
			perror(""); \
		} \
		else \
		{ \
			TRACE("Closed fd %d\n", fd); \
		} \
	} while(0)

volatile int continue_main_process = 1;
volatile int zombie_guard = 0;
char post_execution_msg[16368] = {0};
volatile size_t blocking_processes = 0ul;
st_command *cmds_head = NULL, *recent_cmds = NULL;

size_t count_command_tokens(const st_token *head)
{
	size_t i = 0ul;
	for (; head; head = head->next, i++)
	{
		if (is_hard_delim(head))
			break;
	}
	if (head)
		i++;
	return i;
}

const st_token *find_command_tail(const st_token *head)
{
	size_t token_c = count_command_tokens(head);
	st_token_iterate(&head, token_c - 1ul);
	return head;
}

size_t count_redirectors(const st_command *cmd)
{
	size_t count = 0ul, i = 0ul;
	st_redirector **redir = cmd->file_redirectors;
	TRACEE("\n");
	for (; redir[i]; i++)
		count++;
	TRACEL("Count %lu\n", count);
	return count;
}

void handle_process_redir_presence(const st_token *head,
	const st_token *tail, size_t *tokens_len, size_t *token_c, size_t *totlen)
{
	if (st_token_find_range((st_token *) head,
		(st_token *) tail, &st_token_contains, (void *) process_redirector))
	{
		*tokens_len	-= 1ul;
		(*token_c)--;
		*totlen -= 2ul;
	}
}

void token_strlen(st_token *item, void *length)
{
	*(size_t *) length += strlen(item->str);
}

void inc(st_token *item, void *data)
{
	UNUSED(item);
	(*(int *) data)++;
}

void app_token_str(char *dest, const st_token *item, const st_token *tail)
{
	if (!item)
		return;
	if (!is_hard_delim(item) || item->type == bg_process)
	{
		if (dest[0] != '\0')
			strcat(dest, " ");
		strcat(dest, item->str);
	}
	if (item == tail)
		return;
	app_token_str(dest, item->next, tail);
}

char *form_command_string(const st_token *head, const st_token *tail)
{
	char *str;
	size_t tokens_len = 0ul, token_c = 0ul, totlen;
	TRACEE("head %p tail %p\n", (void *) head, (void *) tail);
	st_token_traverse((st_token *) head, &token_strlen, (void *) &tokens_len);
	st_token_traverse((st_token *) head, &inc, (void *) &token_c);
	totlen = tokens_len + token_c;
	handle_process_redir_presence(head, tail, &tokens_len, &token_c, &totlen);
	str = (char *) calloc(totlen + 1, sizeof(char));
	TRACE("tokens_len %lu token_c %lu totlen %lu\n", tokens_len, token_c,
		totlen);
	app_token_str(str, head, tail);
	TRACEL("Formed string [%s]\n", str);
	assert(!str[totlen]);
	return str;
}

void callback_is_arg(st_token *item, void *counter)
{
	int type = (int) item->type;
	if (type == arg || type == program)
		(*(int *) counter)++;
}

void allocate_arguments_memory(const st_token *head,
	const st_token *tail, int *argc, char ***argv)
{
	TRACEE("head %p tail %p\n", (void *) head, (void *) tail);
	*argc = 0;
	st_token_traverse_range((st_token *) head, (st_token *) tail,
		&callback_is_arg, (void *) argc);
	*argv = (char **) calloc(*argc + 1, sizeof(char *));
	TRACEL("argc %i allocated bytes %lu\n", *argc,
		(*argc + 1) * sizeof(char *));
}

void allocate_redirectors_memory(const st_token *head,
	const st_token *tail, st_command *cmd)
{
	size_t redir_c;
	TRACEE("\n");
	redir_c = st_token_count_range(head, tail, &st_token_compare,
		(void *) file_redirector);
	cmd->file_redirectors = (st_file_redirector **)
		calloc(redir_c + 1, sizeof(st_file_redirector *));

	TRACEL("redir_c %lu allocated bytes %lu\n", redir_c,
		(redir_c + 1) * sizeof(st_file_redirector *));
}

size_t count_commands(const st_token *head)
{
	size_t count = st_token_count(head, &st_token_compare_not, (void *) arg);
	TRACEE("Count %lu\n", count);
	if (count == 0ul) /* user may input a command without any special token */
	{
		count += !!head; /* if head is not null add 1 */
	}
	else
	{
		while (head->next)
			head = head->next;
		count += head->type == arg;
		/* if after last special token there is an arg add 1 */
	}
	TRACEL("Count %lu\n", count);

	return count;
}

void handle_command_redirector(const st_token *token, st_command *cmd)
{
	size_t redir_c = count_redirectors(cmd);
	st_redirector *redir = token->redir;
	assert(token->type == file_redirector || token->type == process_redirector);
	assert(token->next);
	TRACEE("redir_c %lu\n", redir_c);
	TRACE("A new file redirector has been added to a command \n\t " \
		"path [%s] fd %d app %d dir %d\n",
		redir->path, redir->fd, redir->app, redir->dir);

	cmd->file_redirectors[redir_c] = redir;
	cmd->file_redirectors[redir_c + 1] = NULL; /* terminating null */
	TRACEL("\n");
}

void handle_arg(const st_token *token, int *argc, char **argv)
{
	argv[*argc] = strdup(token->str);
	(*argc)++;
}

void handle_bg_process(st_command *cmd)
{
	cmd->flags |= CMD_BG;
	cmd->flags &= ~CMD_BLOCKING;
}

void handle_pipe(st_command *cmd)
{
	cmd->flags |= CMD_PIPED;
}

void on_token_handle(const st_token *token, int *argc, char **argv,
	st_command *cmd)
{
	TRACEE("Token str [%s] Type [%d]\n", token->str, token->type);
	switch (token->type)
	{
		case program:
		case arg:
			handle_arg(token, argc, argv);
			break;
		case bg_process:
			handle_bg_process(cmd);
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
			TRACE("A non implemented token has been passed - " \
				"String: [%s] Type: [%d]\n", token->str, token->type);
	}
	TRACEL("\n");
}

void handle_command_tokens(const st_token *head,
	const st_token *tail, st_command *cmd, char **argv)
{
	int argc = 0, passed_tail = 0;
	TRACEE("\n");
	for (; !passed_tail; head = head->next)
	{
		on_token_handle(head, &argc, argv, cmd);
		if (head == tail)
			passed_tail = 1;
	}
	TRACEL("\n");
}

st_command *st_command_create_empty()
{
	st_command *cmd = (st_command *) malloc(sizeof(st_command));
	cmd->cmd_str = NULL;
	cmd->argc = 0;
	cmd->argv = NULL;
	cmd->flags = CMD_BLOCKING;
	cmd->file_redirectors = NULL;
	cmd->pipefd[0] = -1;
	cmd->pipefd[1] = -1;
	cmd->pid = 0;
	cmd->eid = 0;
	cmd->next = NULL;
	cmd->prev = NULL;
	return cmd;
}

st_command *st_command_create(const st_token *head)
{
	int argc;
	char **argv;
	const st_token *tail = find_command_tail(head);
	st_command *cmd = st_command_create_empty();
	if (!head)
		return cmd;

	allocate_arguments_memory(head, tail, &argc, &argv);
	allocate_redirectors_memory(head, tail, cmd);
	handle_command_tokens(head, tail, cmd, argv);
	assert(!argv[argc]);

	cmd->argc = argc;
	cmd->argv = argv;
	cmd->cmd_str = form_command_string(head, tail);

#ifdef _TRACE
	TRACE("New command has been created\n");
	st_command_print(cmd, TAB);
#endif

	return cmd;
}

st_command *get_tail(st_command *head)
{
	while (head && head->next)
		head = head->next;
	return head;
}

int is_output_piped(const st_command *cmd)
{
	return cmd->flags & CMD_PIPED;
}

void determine_blocking(st_command *head)
{
	int blocking = 0;
	st_command *tail = NULL;
	TRACEE("\n");
	tail = get_tail(head);
	for (; tail; tail = tail->prev)
	{
		TRACE("'%s'\n", tail->cmd_str);
		if (!blocking && is_output_piped(tail))
		{
			tail->flags &= ~CMD_BLOCKING;
			TRACE("'%s' became unblocking\n", tail->cmd_str);
		}
		blocking = !!(tail->flags & CMD_BLOCKING);
	}
	TRACEL("\n");
}

st_command *st_commands_create(const st_token *tokens)
{
	size_t tokenc = 0ul;
	st_command *head = NULL, *cmd = NULL;

	TRACEE("\n");
	while (tokens)
	{
		cmd = st_command_create(tokens);
		st_command_push_back(&head, cmd);
		tokenc = count_command_tokens(tokens);
		TRACE("New commands has been created addr %p tokenc %lu\n",
			cmd, tokenc);
		st_token_iterate(&tokens, tokenc);

		/* drop an empty command */
		if (cmd->argc == 0ul)
		{
			st_command_erase_item(&head, cmd);
			cmd = NULL;
		}
	}
	determine_blocking(head);
	TRACEL("\n");

	return head;
}

void *st_command_traverse(st_command *cmd, callback_t callback, void *userdata)
{
	void *res;
	if (!cmd)
		return NULL;
	res = callback(cmd, userdata);
	if (res)
		return res;
	return st_command_traverse(cmd->next, callback, userdata);
}

#define PREF (prefix ? prefix : "")

/* Single prefix print */
#define P_PRINT(...) \
	do { \
		printf("%s", PREF); \
		printf(__VA_ARGS__); \
	} while (0);

/* Double prefix print */
#define PP_PRINT(...) \
	do { \
		printf("%s%s", PREF, PREF); \
		printf(__VA_ARGS__); \
	} while (0);

void print_literal_flags(const st_command *cmd, const char *prefix)
{
#define CONTAINS(var) \
	if (cmd->flags & (var)) \
		P_PRINT("%s", #var)

	CONTAINS(CMD_BG);
	CONTAINS(CMD_PIPED);
	CONTAINS(CMD_BLOCKING);

#undef CONTAINS
}

void st_command_print(const st_command *cmd, const char *prefix)
{
	size_t i;
	st_file_redirector **redir = cmd->file_redirectors;
	P_PRINT("cmd addr %p\n", (void *) cmd);
	P_PRINT("cmd_str: [%s]\n", cmd->cmd_str);
	P_PRINT("argc: %d\n", cmd->argc);

	P_PRINT("argv: [ \n");
	for (i = 0ul; cmd->argv[i]; i++)
		PP_PRINT("[%s], \n", cmd->argv[i]);
	PP_PRINT("[%s]\n", cmd->argv[i]);
	P_PRINT("]\n");

	P_PRINT("flags: ");
	print_flags((void *) &cmd->flags, sizeof(cmd->flags));
	P_PRINT("\n");
	print_literal_flags(cmd, prefix);
	P_PRINT("\n");

	P_PRINT("pid: [%d]\n", cmd->pid);
	P_PRINT("eid: [%d]\n", cmd->eid);

	P_PRINT("file_redirectors: [ \n");
	for (i = 0ul; redir[i]; i++)
	{
		PP_PRINT("[");
		st_redirector_print(redir[i]);
		PP_PRINT("],\n");
	}
	PP_PRINT("%p\n", (void *) redir[i]);
	P_PRINT("]\n");
	P_PRINT("pipefd: %d %d\n", cmd->pipefd[0], cmd->pipefd[1]);
	P_PRINT("Next: %p Prev: %p\n", (void *) cmd->next, (void *) cmd->prev);
}
#undef PP_PRINT
#undef PP_PRINT
#undef PREF

void argv_delete(char **argv)
{
	int i = 0;
	TRACEE("\n");
	if (!argv)
		return;
	for (; argv[i]; i++)
		free(argv[i]);
	TRACEL("\n");
}

void st_redirectors_delete(st_redirector **redirectors)
{
	size_t i = 0ul;
	TRACEE("\n");
	if (!redirectors)
		return;
	for (; redirectors[i]; i++)
		st_redirector_delete(redirectors[i]);
	free(redirectors);
	TRACEL("\n");
}

void st_command_delete(st_command *cmd)
{
	TRACEE("cmd addr %p\n", (void *) cmd);
	if (!cmd)
	{
		TRACEL("cmd is null \n");
		return;
	}
	free_if_exists(cmd->cmd_str);
	argv_delete(cmd->argv);
	st_redirectors_delete(cmd->file_redirectors);
	/* fds are closed in execute_commands */
	release_eid(cmd->eid);
	free(cmd);
	TRACEL("\n");
}

void st_commands_clear(st_command *head)
{
	if (!head)
		return;
	st_commands_clear(head->next);
	st_command_delete(head);
}

int output_exec_result(int status)
{
	if (status != 0)
	{
		/* if (WIFEXITED(status)) */
		/* 	printf("Terminated with %d", WEXITSTATUS(status)); */
		if (WIFSIGNALED(status))
			return WTERMSIG(status);
	}
	return 0;
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

#define REDIRECT(from, to) \
	if (dup2(from, to) == -1) \
	{ \
		TRACE("dup2(%d, %d): ", from, to); \
		perror(""); \
		return 1; \
	}

int redirect_file_stream(st_command *cmd)
{
	int fd;
	size_t i;
	st_redirector **redic = cmd->file_redirectors;
	TRACEE("\n");
	for (i = 0ul; redic[i]; i++)
	{
#ifdef _TRACE
		TRACE("New iteration. redirector: ");
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
			TRACE("Failed to open '%s': ", redic[i]->path);
			perror("");
			return 1;
		}
		/* redirect a stream */
		REDIRECT(fd, redic[i]->fd);
		TRACE("Redirected a file stream (%d) to %d\n", fd, redic[i]->fd);
	}
	TRACEL("\n");
	return 0;
}

int redirect_process_stream(st_command *cmd)
{
	int i, fd;
	TRACEE("\n");
	for (i = 0; i < 2; i++)
	{
		fd = cmd->pipefd[i];
		if (fd != -1)
		{
			REDIRECT(fd, i);
			assert(!i ? cmd->prev : cmd->next);
			TRACE("Substitute %s's fd with %d, %s of '%s'\n",
				!i ? "stdin" : "stdout",
				cmd->pipefd[i],
				!i ? "stdout" : "stdin",
				!i ? cmd->prev->cmd_str : cmd->next->cmd_str);
		}
	}
	TRACEL("\n");
	return 0;
}

int redirect_streams(st_command *cmd)
{
	if (cmd->file_redirectors && redirect_file_stream(cmd))
		return -1;
	if ((cmd->pipefd[0] != -1 || cmd->pipefd[1] != -1) &&
		redirect_process_stream(cmd))
		return -1;
	return 0;
}

void open_pipe(st_command *cmd)
{
	int pipefd[2] = {0};
	TRACEE("\n");
	if (cmd->flags & CMD_PIPED)
	{
		pipe(pipefd);
		assert(cmd->next);
		cmd->next->pipefd[0] = pipefd[0];
		cmd->pipefd[1] = pipefd[1];
		TRACE("cmd->next->pipefd[0] = %d cmd->pipdfd[1] = %d\n",
			cmd->next->pipefd[0], cmd->pipefd[1]);
	}
	TRACEL("\n");
}

void close_pipe_fd(int pipefd[2])
{
	TRACEE("Pipe: %d %d\n", pipefd[0], pipefd[1]);
	if (pipefd[0] != -1)
	{
		CLOSE_FD(pipefd[0]);
		CLOSE_FD(pipefd[1]);
	}
	TRACEL("\n");
}

void close_pipe_cmd(st_command *cmd)
{
	int pipefd[2];
	TRACEE("'%s'\n", cmd->cmd_str);
	assert(cmd->prev);
	pipefd[0] = cmd->pipefd[0];
	pipefd[1] = cmd->prev->pipefd[1];
	cmd->pipefd[0] = -1;
	cmd->prev->pipefd[1] = -1;
	close_pipe_fd(pipefd);
	TRACEL("\n");
}

void close_opp_pipe_side(const st_command *cmd)
{
	const int *fds = cmd->pipefd;
	TRACEE("cmd '%s' pipefd[0] %d pipefd[1] %d\n", cmd->cmd_str,
		fds[0], fds[1]);
	if (fds[0] != -1)
		CLOSE_FD(fds[0] + 1);
	if (fds[1] != -1)
		CLOSE_FD(fds[1] - 1);
	TRACEL("\n");
}

pid_t cmd_fork(st_command *cmd)
{
	pid_t pid;
	TRACEE("\n");
	pid = fork();
	if (pid == -1)
	{
		TRACE("Failed to fork: ");
		perror("");
	}
	else if (pid == 0)
	{
		close_opp_pipe_side(cmd);
		if (redirect_streams(cmd) == -1)
		{
			TRACE("'%s' exit\n", cmd->cmd_str);
			exit(1);
		}
		TRACE("execvp '%s'\n", cmd->cmd_str);
		execvp(cmd->argv[0], cmd->argv);
		perror(cmd->cmd_str);
		exit(1);
	}
	TRACEL("\n");
	return pid;
}

pid_t call_command(st_command *cmd)
{
	TRACEE("\n");

	open_pipe(cmd);
	cmd->pid = cmd_fork(cmd);

	TRACEL("pid %d\n", cmd->pid);
	return cmd->pid;
}

pid_t handle_process(st_command *cmd)
{
	TRACEE("cmd [%s] addr %p\n", cmd->cmd_str, (void *) cmd);

	if (cmd->flags & CMD_BG)
		cmd->eid = acquire_eid();
	cmd->pid = call_command(cmd);
	if (cmd->pid == -1)
	{
		TRACEL("if pid == -1\n");
		st_command_delete(cmd);
		return 0;
	}
	if (cmd->flags & CMD_BLOCKING)
	{
		blocking_processes++;
		TRACE("blocking_processes++ %lu\n", blocking_processes);
	}
	if (cmd->flags == CMD_BLOCKING)
	{
		/* as soon as possible (after launching a piped process) disallow
		   main process to work while pipe's work isn't terminated */
		TRACE("continue_main_process = 0\n");
		continue_main_process = 0;

		if (zombie_guard)
			TRACE("Wait for zombie guard\n");
		WAIT_FOR(zombie_guard);
		RAISE_ZGUARD();
		if (cmd->pipefd[0] != -1)
			close_pipe_cmd(cmd);
		DROP_ZGUARD();

		WAIT_FOR(!continue_main_process);
	}
	TRACEL("\n");
	return cmd->pid;
}

void st_command_push_back(st_command **head, st_command *new_item)
{
	st_command *it;
	TRACEE("new item %p\n", (void *) new_item);
	if (!(*head))
		*head = new_item;
	else
	{
		it = *head;
		while (it->next)
			it = it->next;
		it->next = new_item;
		new_item->prev = it;
		TRACE("it->next = %p new_item->prev = %p\n", (void *) new_item,
			(void *) it);
	}
	TRACEL("\n");
}

st_command *st_command_find(st_command *head, callback_t callback,
	void *userdata)
{
	if (!head)
		return NULL;
	if (*(int *) callback(head, userdata))
		return head;
	return st_command_find(head->next, callback, userdata);
}

int st_command_erase_item(st_command **head, st_command *item)
{
	TRACEE("head %p *head %p item %p \n",
		(void *) head, (void *) *head, (void *) item);
	if (!(*head) || !item)
	{
		TRACEL("some ptr is null\n");
		return 0;
	}
	if (*head == item)
	{
		TRACE("Move head\n");
		*head = (*head)->next;
		if (*head)
			(*head)->prev = item->prev;
	}
	else
	{
		TRACE("Edit item's prev and next ptrs\n");
		if (item->prev)
			item->prev->next = item->next;
		if (item->next)
			item->next->prev = item->prev;
	}
	st_command_delete(item);
	TRACEL("\n");
	return 1;
}

int execute_commands()
{
	int res;
	st_command *cmd;
	TRACEE("\n");
	for (; recent_cmds; recent_cmds = recent_cmds->next)
	{
		cmd = recent_cmds;
		if (!strcmp(cmd->argv[0], "cd"))
			handle_cd(cmd);
		else if (!strcmp(cmd->argv[0], "export"))
			handle_export(cmd);
		else if (!strcmp(cmd->argv[0], "exit"))
			res = -1;
		else
			res = handle_process(cmd);

		/* close a pipe as soon as its second process starts */
		if (cmd->pipefd[0] != -1)
			close_pipe_cmd(cmd);

		if (res == -1)
		{
			TRACEL("res -1\n");
			return -1;
		}
	}
	TRACEL("\n");
	return 0;
}

void clean_zombie_data(st_command *cmd)
{
	TRACEE("About to clean up '%s'\n", cmd->cmd_str);
	if (cmd->flags & CMD_BG)
		form_post_execution_msg(cmd);
	st_command_erase_item(&cmds_head, cmd);
	TRACEL("A command has been erased from the list.\n");
}

void *compare_pid(st_command *cmd, void *pid)
{
	int res = cmd->pid == *(pid_t *) pid;
	return res ? cmd : NULL;
}

void print_term_msg_sig(const st_command *cmd, int no)
{
	if (no == 0)
		return;
	/* TODO write an actual sig title instead of sig number */
	printf(SHELL ": Job %d, '%s' terminated by signal %d\n",
		cmd->eid, cmd->cmd_str, no);
}

void decrease_blocking_processes()
{
	assert(blocking_processes > 0ul);
	blocking_processes--;
	TRACE("blocking processes-- %lu\n", blocking_processes);
}

void handle_blocking(st_command *cmd)
{
	if (!(cmd->flags & CMD_BLOCKING))
		return;
	decrease_blocking_processes();
	TRACE("pid %d cmd '%s' Wait bprcs (%lu) cont mprcs (%d) zguard (%d)\n",
		cmd->pid, cmd->cmd_str, blocking_processes,
		continue_main_process, zombie_guard);
	WAIT_FOR((!continue_main_process && blocking_processes) || zombie_guard);
	continue_main_process = 1;
}

void handle_exec_res(st_command *cmd, int status)
{
	int code;
	TRACEE("pid %d cmd '%s'\n", cmd->pid, cmd->cmd_str);
	code = output_exec_result(status);
	print_term_msg_sig(cmd, code);
	handle_blocking(cmd);
	RAISE_ZGUARD();
	TRACE("Child process clean up\n");
	clean_zombie_data(cmd);
	DROP_ZGUARD();
	TRACEL("\n");
}

void check_zombie()
{
	int status;
	pid_t pid = wait4(-1, &status, WNOHANG, NULL);
	st_command *cmd = (st_command *) st_command_traverse(cmds_head,
		compare_pid, (void *) &pid);
	assert(pid != -1 && cmd);
	TRACEE("pid %d cmd '%s'\n", pid, cmd->cmd_str);

	if (zombie_guard)
		TRACE("pid %d waiting for guard %d\n", pid, zombie_guard);
	WAIT_FOR(zombie_guard);
	handle_exec_res(cmd, status);

	TRACEL("\n");
}

void form_post_execution_msg(const st_command *cmd)
{
	char *target = post_execution_msg;
	size_t maxlen = sizeof(post_execution_msg);
	size_t len = strlen(target);
	TRACEE("\n");

	if (len > 0ul)
		len += snprintf(target + len, maxlen, "\n");

	/* TODO add signal code before this msg if there was one */
	/* like 'msh: Job N, 'sleep 15 &' terminated by signal SIGABRT (Abort)' */
	snprintf(target + len, maxlen, "msh: Job %d, '%s' has ended",
		cmd->eid, cmd->cmd_str);
	TRACEL("\n");
}

void clear_post_execution_msg()
{
	post_execution_msg[0] = '\0';
}

void st_commands_free()
{
	st_commands_clear(cmds_head);
}

void print_cmds_head()
{
	st_command *it = cmds_head;
	TRACEE("\n");
	for (; it; it = it->next)
	{
		st_command_print(it, "\t");
		putchar(10);
	}
	TRACEL("\n");
}

void st_command_pass_ownership(st_command *cmds)
{
	TRACEE("cmds addr %p\n", (void *) cmds);
	st_command_push_back(&cmds_head, cmds);
	recent_cmds = cmds;
/* #ifdef _TRACE */
/* 	print_cmds_head(); */
/* #endif */
	TRACEL("\n");
}
