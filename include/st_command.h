#ifndef ST_COMMAND_H
#define ST_COMMAND_H

#include "eid.h"

#include <sys/types.h>

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
	List's elements are erased only when corresponding PID is returned by
	wait4 in check_zombies.
*/

struct st_token_item;

typedef struct
{
	char *dest;
	int fd;
} st_redirection;

/* They'll be treated in similar, though different, ways */
typedef st_redirection st_file_redirection;
typedef st_redirection st_process_redirection;

typedef struct st_command
{
	char *cmd_str;
	int argc;
	char **argv;
	int is_bg_process;
	/* TO IMPLEMENT */
	/* st_file_redirection **file_redirections */
	/* st_process_redirection **process_redirections */

	pid_t pid;
	eid_t eid;
	struct st_command *next;
} st_command;

st_command *st_command_create_empty();
st_command *st_command_create(const struct st_token_item *head);
st_command **st_commands_create(const struct st_token_item *head);
void st_command_print(const st_command *cmd);
void st_command_delete(st_command *cmd);
void st_commands_delete(st_command **cmds);
void st_command_push_back(st_command **head, st_command *new_item);
st_command *st_command_find(st_command *head, pid_t pid);
int st_command_erase_item(st_command **head, st_command *item);
pid_t execute_command(st_command *cmd);
void check_zombies();

#endif /* !ST_COMMAND_H */
