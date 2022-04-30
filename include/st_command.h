#ifndef ST_COMMAND_H
#define ST_COMMAND_H

#include "eid.h"
#include "def.h"

#include <signal.h>

/*
	Command represents a program call with some behavior modifiers such as
	arguments, i/o stream redirectors in/from files or programs.
	Main command linked list is stored and handled only by st_command module,
	however initially main has the ownership (pointer to the allocated memory
	by the module).
	A command is formed up by the tokens list. Once a command has been created
	it'll allocate memory and copy all the tokens that have formed the command
	(the strings).
	Once an asynchronous command has been executed in execute_command it'll be
	stored into the main list that's completely managed by st_command module.
	List elements are erased only when corresponding PID is returned by wait4
	in check_zombies function.
*/

typedef st_redirector st_file_redirector;
typedef void *(*callback_t)(st_command *, void *);

enum
{
	CMD_BG          = 1 << 0,
	CMD_PIPED       = 1 << 1,
	CMD_BLOCKING    = 1 << 2,
	CMD_NOT_FG      = CMD_PIPED | CMD_BG,
};

struct st_command
{
	char *cmd_str;
	int argc;
	char **argv;
	u_int16_t flags;
	st_file_redirector **file_redirectors;
	int pipefd[2];

	pid_t pid;
	eid_t eid;
	st_command *next, *prev;
};

extern char post_execution_msg[16368];

st_command *st_command_create_empty();
st_command *st_command_create(const st_token *head);
st_command *st_commands_create(const st_token *head);
void *st_command_traverse(st_command *cmd, callback_t callback,
	void *userdata);
void st_command_print(const st_command *cmd, const char *prefix);
void st_command_delete(st_command *cmd);
void st_commands_clear(st_command *head);
void st_command_push_back(st_command **head, st_command *new_item);
int st_command_erase_item(st_command **head, st_command *item);
int execute_commands();
void check_zombie();
void form_post_execution_msg(const st_command *cmd);
void clear_post_execution_msg();
void st_commands_free();
void st_command_pass_ownership(st_command *cmds);

#endif /* !ST_COMMAND_H */
