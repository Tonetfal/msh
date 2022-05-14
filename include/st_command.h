#ifndef ST_COMMAND_H
#define ST_COMMAND_H

#include "eid.h"
#include "def.h"

#include <signal.h>

/*
 * Command represents a program call with some behavior modifiers such as
 * arguments, i/o stream redirectors in/from files or programs.
 * Main command linked list is stored and handled only by st_command module,
 * however initially main has the ownership (pointer to the allocated memory
 * by the module).
 * A command is formed up by the tokens list. Once a command has been created
 * it'll allocate memory and copy all the tokens that have formed the command
 * (the strings).
 * Once an asynchronous command has been executed in execute_command it'll be
 * stored into the main list that's completely managed by st_command module.
 * List elements are erased only when corresponding PID is returned by wait4
 * in check_zombies function.
 */

/*
 * NOTES
 * File redirectors are designed to support multiple I/O, i.e. it'll be
 * possible to change destination not only for stdin, stdout and stderr, but
 * even for non-standard descriptors.

 * To redirect a standard I/O stream do:
 *  'prg < src > dest'
 * This will change both stdin and stdout respectively. In case of
 * redirecting stdout the previous syntax will truncate the file, but if
 * appending is required '>>' can be used instead.

 * To redirect some other stream following syntax will be possible:
 *  'prg 1> stdout_dest 2> stderr_dest 3> dest 4< src'
 * This will change output file descriptors 1, 2, and 3 to files
 * 'stdout_dest', 'stderr_dest' and 'dest' respectively, while input file
 * descriptor 4 will be created with file 'src'.
 */

typedef st_redirector st_file_redirector;
typedef void (*cmd_vcallback_t)(st_command *, void *);
typedef st_command *(*cmd_scallback_t)(st_command *, void *);

enum
{
    CMD_BG          = 1 << 0,       /* Background process */
    CMD_PIPED       = 1 << 1,       /* Redirected I/O with some process */
    CMD_BLOCKING    = 1 << 2,       /* No prompt while it's running */
    CMD_FORKED      = 1 << 3,       /* Cmd was used to fork */
    CMD_STARTED     = 1 << 4,       /* Process has started */
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

void st_command_init();
st_command *st_command_create_empty();
st_command *st_command_create(const st_token *head);
st_command *st_commands_create(const st_token *head);

void st_command_traverse(st_command *cmd, cmd_vcallback_t callback, void *userdata);
st_command *st_command_find(st_command *head, cmd_scallback_t callback,
    void *userdata);

void st_command_print(const st_command *cmd, const char *prefix);
void st_command_delete(st_command **cmd);
void st_commands_clear(st_command **head);
void st_command_push_back(st_command **head, st_command *new_item);
int st_command_erase_item(st_command **head, st_command *item);
int execute_commands();
void check_zombie();
void form_post_execution_msg(const st_command *cmd);
void clear_post_execution_msg();
void st_commands_free();
void st_command_pass_ownership(st_command *cmds);

#endif /* !ST_COMMAND_H */
