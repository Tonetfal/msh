#define _XOPEN_SOURCE 500 /* snprintf, usleep */
#define _DEFAULT_SOURCE /* wait4 */

#include "def.h"
#include "io_utility.h"
#include "log.h"
#include "st_command.h"
#include "st_dll_string.h"
#include "st_redirector.h"
#include "st_token.h"
#include "tokens_utility.h"
#include "utility.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define SKIP_TIME_SLICE() usleep(1000)
#define WAIT_FOR(flag) \
    do { \
        if (flag) \
            PTRACE("Wait for '" #flag "'\n"); \
        while (flag) \
            SKIP_TIME_SLICE(); \
    } while(0)
#define RAISE_ZGUARD() \
    do { PTRACE("raise zguard\n"); zombie_guard = 1; } while(0)
#define DROP_ZGUARD() \
    do { PTRACE("drop zguard\n"); zombie_guard = 0; } while(0)
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
#define IS_MAIN_BLOCKING_PROGRAM(cmd) \
    ((cmd->flags & CMD_BLOCKING) && !(cmd->flags & (CMD_BG | CMD_PIPED)))

volatile sig_atomic_t continue_main_process = 1;
volatile sig_atomic_t zombie_guard = 0;
volatile sig_atomic_t child_start = 0;
volatile sig_atomic_t ready_children = 0;
volatile sig_atomic_t blocking_processes = 0;
int spawned_children = 0;
char post_execution_msg[16368] = {0};
pid_t last_main_process = -2;
st_command *cmds_head = NULL, *recent_cmds = NULL;

void parent_usr1hdl(int signo)
{
    signal(SIGUSR1, &parent_usr1hdl);
    assert(signo == SIGUSR1);
    ready_children++;
    TRACE("Got child's signal. Ready children '%d'\n", ready_children);
    UNUSED(signo);
}

void st_command_init()
{
    signal(SIGUSR1, &parent_usr1hdl);
}

size_t count_command_tokens(const st_token *head)
{
    size_t i = 0ul;
    for (; head; head = head->next, i++)
    {
        if (is_cmd_delim(head))
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

size_t count_cmd_redirectors(const st_command *cmd)
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
        *tokens_len -= 1ul;
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
    if (!is_cmd_delim(item) || item->type == bg_process)
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
    str = (char *) calloc(totlen + 1ul, sizeof(char));
    TRACE("Allocated %zu bytes for ptr '%p'. tokens_len %zu token_c %zu\n",
        totlen + 1ul, (void *) str, tokens_len, token_c);
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
    TRACEE("Head addr '%p' Tail addr '%p'\n", (void *) head, (void *) tail);
    assert(head && tail && argv);
    *argc = 0;
    st_token_traverse_range((st_token *) head, (st_token *) tail,
        &callback_is_arg, (void *) argc);
    *argv = (char **) calloc(*argc + 1, sizeof(char *));
    TRACEL("Allocated bytes %zu for ptr '%p'. argc %d\n",
        (*argc + 1) * sizeof(char *), (void *) *argv, *argc);
    assert(*argv);
}

void allocate_redirectors_memory(const st_token *head,
    const st_token *tail, st_file_redirector ***redir)
{
    size_t redir_c;
    TRACEE("Head addr '%p' Tail addr '%p' Redir '%p' *Redir '%p'\n",
        (void *) head, (void *) tail, (void *) redir, (void *) *redir);
    assert(head && tail && redir);
    redir_c = st_token_count_range(head, tail, &st_token_compare,
        (void *) file_redirector);
    *redir = (st_file_redirector **) calloc(redir_c + 1ul,
        sizeof(st_file_redirector *));
    TRACEL("Allocated %zu bytes for ptr '%p'\n", (redir_c + 1ul) *
        sizeof(st_file_redirector *), (void *) *redir);
    assert(*redir);
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

void handle_file_redirector(const st_token *token, st_command *cmd)
{
    size_t redir_c = count_cmd_redirectors(cmd);
    st_file_redirector *src = token->redir;
    TRACEE("Cmd addr '%p' Source addr '%p'\n", (void *) cmd, (void *) src);
    assert(src && token->type == file_redirector && token->next);
    TRACE("A new file redirector is about to be added to a command:\n");
    TRACER("path [%s] fd %d app %d dir %d\n",
        src->path, src->fd, src->app, src->dir);

    cmd->file_redirectors[redir_c] = st_redirector_dup(src);
    TRACEL("\n");
}

void handle_arg(const st_token *token, int *argc, char **argv)
{
    TRACEE("\n");
    argv[*argc] = strdup(token->str);
    (*argc)++;
    TRACEL("\n");
}

void handle_bg_process(st_command *cmd)
{
    TRACEE("\n");
    cmd->flags |= CMD_BG;
    cmd->flags &= ~CMD_BLOCKING;
    TRACEL("\n");
}

void handle_pipe(st_command *cmd)
{
    TRACEE("\n");
    cmd->flags |= CMD_PIPED;
    TRACEL("\n");
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
            handle_file_redirector(token, cmd);
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
    st_command *item = (st_command *) malloc(sizeof(st_command));
    ATRACEE(item);
    item->cmd_str = NULL;
    item->argc = 0;
    item->argv = NULL;
    item->flags = CMD_BLOCKING;
    item->file_redirectors = NULL;
    item->pipefd[0] = -1;
    item->pipefd[1] = -1;
    item->pid = 0;
    item->eid = 0;
    item->next = NULL;
    item->prev = NULL;
    TRACEL("\n");
    return item;
}

st_command *st_command_create(const st_token *head)
{
    int argc;
    char **argv;
    const st_token *tail = find_command_tail(head);
    st_command *item = st_command_create_empty();
    TRACEE("\n");
    if (!head)
    {
        TRACEL("An empty command has been created due null head\n");
        return item;
    }

    allocate_arguments_memory(head, tail, &argc, &argv);
    allocate_redirectors_memory(head, tail, &item->file_redirectors);
    assert(item->file_redirectors);
    handle_command_tokens(head, tail, item, argv);
    assert(!argv[argc]);

    item->argc = argc;
    item->argv = argv;
    item->cmd_str = form_command_string(head, tail);

#ifdef _TRACE
    TRACE("New command has been created\n");
    LOGPRINT("\n");
    st_command_print(item, TAB);
    LOGPRINT("\n");
#endif
    TRACEL("\n");
    return item;
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

void st_command_traverse(st_command *cmd, cmd_vcallback_t callback, void *userdata)
{
    if (!cmd)
        return;
    callback(cmd, userdata);
    st_command_traverse(cmd->next, callback, userdata);
}

#define PREF (prefix ? prefix : "")
#define PRINT(...) printf(__VA_ARGS__)
#define PRINT_PREF() PRINT("%s", PREF)
#define PRINT_DPREF() PRINT("%s%s", PREF, PREF)

/* Single prefix print */
#define P_PRINT(...) \
    do { \
        PRINT_PREF(); \
        PRINT(__VA_ARGS__); \
    } while (0)

/* Double prefix print */
#define PP_PRINT(...) \
    do { \
        PRINT_DPREF(); \
        PRINT(__VA_ARGS__); \
    } while (0);

void print_literal_flags(const st_command *cmd)
{
#define CONTAINS(var) if (cmd->flags & (var)) PRINT("%s ", #var)

    CONTAINS(CMD_BG);
    CONTAINS(CMD_PIPED);
    CONTAINS(CMD_BLOCKING);
    CONTAINS(CMD_FORKED);
    CONTAINS(CMD_STARTED);

#undef CONTAINS
}

/* Left/Right enclosure */
#define LENC "'"
#define RENC "'"
#define STR  LENC "%s" RENC
#define DEC  LENC "%d" RENC
#define ADDR LENC "%p" RENC
void st_command_print(const st_command *cmd, const char *prefix)
{
    size_t i;
    st_file_redirector **redir = cmd->file_redirectors;
    P_PRINT("cmd addr - "ADDR"\n", (void *) cmd);
    P_PRINT("cmd_str: "STR" - "ADDR"\n", cmd->cmd_str,
        (void *) cmd->cmd_str);
    P_PRINT("argc: %d\n", cmd->argc);

    P_PRINT("argv: [ \n");
    for (i = 0ul; cmd->argv[i]; i++)
        PP_PRINT(STR" - "ADDR",\n", cmd->argv[i], (void *) cmd->argv[i]);
    PP_PRINT(STR"\n", cmd->argv[i]);
    P_PRINT("]\n");

    P_PRINT("flags: ");
    print_flags((void *) &cmd->flags, sizeof(cmd->flags));
    PRINT("\n");
    PRINT_DPREF();
    print_literal_flags(cmd);
    PRINT("\n");

    P_PRINT("pid: "DEC"\n", cmd->pid);
    P_PRINT("eid: "DEC"\n", cmd->eid);

    P_PRINT("file_redirectors - "ADDR": [ \n", (void *) cmd->file_redirectors);
    for (i = 0ul; redir[i]; i++)
    {
        PP_PRINT("[");
        st_redirector_print(redir[i]);
        PRINT("],\n");
    }
    PP_PRINT("%p\n", (void *) redir[i]);
    P_PRINT("]\n");
    P_PRINT("pipefd: IN "DEC" | OUT "DEC"\n", cmd->pipefd[0], cmd->pipefd[1]);
    P_PRINT("Next: "ADDR" Prev: "ADDR"\n", (void *) cmd->next,
        (void *) cmd->prev);
}
#undef ADDR
#undef DEC
#undef STR
#undef RENC
#undef LENC
#undef PP_PRINT
#undef P_PRINT
#undef PRINT
#undef PREF

void argv_delete(char ***argv)
{
    int i = 0;
    LOGFNPP(argv);
    for (; (*argv)[i]; i++)
        FREE((*argv)[i]);
    FREE(*argv);
    *argv = NULL;
    TRACEL("\n");
}

void st_redirectors_delete(st_redirector ***redirectors)
{
    size_t i = 0ul;
    LOGFNPP(redirectors);
    for (; (*redirectors)[i]; i++)
        st_redirector_delete(&(*redirectors)[i]);
    TRACE("Exit for\n");
    FREE(*redirectors);
    TRACEL("\n");
}

void st_command_delete(st_command **cmd)
{
    LOGFNPP(cmd);
    FREE_IFEX((*cmd)->cmd_str);
    argv_delete(&(*cmd)->argv);
    st_redirectors_delete(&(*cmd)->file_redirectors);
    /* fds are closed in execute_commands */
    release_eid((*cmd)->eid);
    FREE(*cmd);
    TRACEL("\n");
}

void st_commands_clear(st_command **head)
{
    LOGCFNPP(head);
    if (*head)
    {
        st_commands_clear(&(*head)->next);
        st_command_delete(head);
    }
    TRACELC("\n");
}

void st_command_push_back(st_command **head, st_command *new_item)
{
    st_command *it = *head;
    TRACEE("New item %p\n", (void *) new_item);
    if (!(*head))
        *head = new_item;
    else
    {
        while (it->next)
            it = it->next;
        it->next = new_item;
        new_item->prev = it;
        TRACE("it->next = %p new_item->prev = %p\n", (void *) new_item,
            (void *) it);
    }
    TRACEL("\n");
}

st_command *st_command_find(st_command *head, cmd_scallback_t callback,
    void *userdata)
{
    st_command *res;
    if (!head)
        return NULL;
    res = callback(head, userdata);
    if (res)
        return res;
    return st_command_find(head->next, callback, userdata);
}

int st_command_erase_item(st_command **head, st_command *item)
{
    LOGFNE(head, item);
    if (*head == item)
    {
        *head = (*head)->next;
        if (*head)
            (*head)->prev = item->prev;
    }
    else
    {
        if (item->prev)
            item->prev->next = item->next;
        if (item->next)
            item->next->prev = item->prev;
    }
    st_command_delete(&item);
    TRACEL("\n");
    return 1;
}

int output_exec_result(int status)
{
    if (status != 0)
    {
        /* if (WIFEXITED(status)) */
        /*  printf("Terminated with %d", WEXITSTATUS(status)); */
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
    size_t i = 0ul;
    for (; __environ[i]; i++)
    {
        node = st_dll_string_create(__environ[i]);
        st_dll_insert_on_callback(&head, node, &find_higher);
    }
    st_dll_string_print(head);
    st_dll_string_clear(&head);
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
        PTRACE("dup2(%d, %d): ", from, to); \
        perror(""); \
        return 1; \
    }

int redirect_file_stream(st_command *cmd)
{
    int fd;
    size_t i;
    st_redirector **redic = cmd->file_redirectors;
    PTRACEE("\n");
    for (i = 0ul; redic[i]; i++)
    {
#ifdef _TRACE
        PTRACE("New iteration. redirector: ");
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
            PTRACE("Failed to open '%s': ", redic[i]->path);
            perror("");
            return 1;
        }
        /* redirect a stream */
        REDIRECT(fd, redic[i]->fd);
        PTRACE("Redirected a file stream (%d) to %d\n", fd, redic[i]->fd);
    }
    PTRACEL("\n");
    return 0;
}

int redirect_process_stream(st_command *cmd)
{
    int i, fd;
    PTRACEE("\n");
    for (i = 0; i < 2; i++)
    {
        fd = cmd->pipefd[i];
        if (fd != -1)
        {
            REDIRECT(fd, i);
            assert(!i ? cmd->prev : cmd->next);
            PTRACE("Substitute %s's fd with %d, %s of '%s'\n",
                !i ? "stdin" : "stdout",
                cmd->pipefd[i],
                !i ? "stdout" : "stdin",
                !i ? cmd->prev->cmd_str : cmd->next->cmd_str);
        }
    }
    PTRACEL("\n");
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
    if (is_output_piped(cmd))
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
    PTRACEE("cmd - '%s' pipefds '%d' | '%d'\n", cmd->cmd_str, fds[0], fds[1]);
    if (fds[0] != -1)
        CLOSE_FD(fds[0] + 1);
    if (fds[1] != -1)
        CLOSE_FD(fds[1] - 1);
    TRACEL("\n");
}

void child_usr1hdl(int signo)
{
    child_start = 1;
    UNUSED(signo);
}

void wait_parent()
{
    sigset_t mask_usr1, mask_empty;
    PTRACEE("\n");
    sigemptyset(&mask_usr1);
    sigemptyset(&mask_empty);
    sigaddset(&mask_usr1, SIGUSR1);
    sigprocmask(SIG_SETMASK, &mask_usr1, NULL);
    signal(SIGUSR1, &child_usr1hdl);
    kill(getppid(), SIGUSR1);
    PTRACE("Signaled parent about readiness\n");
    while (!child_start)
        sigsuspend(&mask_empty);
    PTRACE("Got parent's signal\n");
    PTRACEL("\n");
}

void wait_children_readiness()
{
    sigset_t mask_usr1, mask_empty;
    TRACEE("\n");
    sigemptyset(&mask_usr1);
    sigemptyset(&mask_empty);
    sigaddset(&mask_usr1, SIGUSR1);
    sigprocmask(SIG_SETMASK, &mask_usr1, NULL);
    assert(spawned_children >= ready_children);
    while (spawned_children > ready_children)
        sigsuspend(&mask_empty);
    TRACEL("Children (%d) are ready\n", spawned_children);
    spawned_children = ready_children = 0;
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
        wait_parent();
        if (errno == SIGINT)
            exit(1);
        close_opp_pipe_side(cmd);
        if (redirect_streams(cmd) == -1)
        {
            PTRACE("'%s' exit\n", cmd->cmd_str);
            exit(1);
        }
        PTRACE("Executing '%s'...\n", cmd->cmd_str);
        execvp(cmd->argv[0], cmd->argv);
        perror(cmd->cmd_str);
        exit(1);
    }

    assert(!(cmd->flags & CMD_FORKED));
    cmd->flags |= CMD_FORKED;
    TRACE("Launched command\n");
#ifdef _TRACE
    st_command_print(cmd, TAB);
#endif
    spawned_children++;

    TRACEL("\n");
    return pid;
}

pid_t call_function(st_command *cmd)
{
    TRACEE("\n");

    open_pipe(cmd);
    cmd->pid = cmd_fork(cmd);

    PTRACEL("Fork PID: %d\n", cmd->pid);
    return cmd->pid;
}

void unpause_child(st_command *cmd, void *data)
{
    UNUSED(data);
    if (!(cmd->flags & CMD_STARTED) && cmd->flags & CMD_FORKED)
    {
        assert(cmd->pid > 0);
        PTRACE("PID %d woke up\n", cmd->pid);
        kill(cmd->pid, SIGUSR1);
        cmd->flags |= CMD_STARTED;
    }
}

void unpause_children()
{
    TRACEE("Wait children\n");
    wait_children_readiness();
    TRACE("Unpause children\n");
    st_command_traverse(cmds_head, &unpause_child, NULL);
    TRACEL("\n");
}

pid_t handle_process(st_command *cmd)
{
    pid_t pid;
    TRACEE("cmd [%s] addr %p\n", cmd->cmd_str, (void *) cmd);

    if (cmd->flags & CMD_BG)
        cmd->eid = acquire_eid();
    pid = call_function(cmd);
    cmd->pid = pid;
    if (cmd->pid == -1)
    {
        TRACEL("if pid == -1\n");
        assert(!"FIX: Clean must be done after all commands have been " \
            "launched");
        /* st_command_delete(&cmd); */
        return 0;
    }
    if (cmd->flags & CMD_BLOCKING)
    {
        blocking_processes++;
        TRACE("blocking_processes++ %d\n", blocking_processes);
    }
    if (IS_MAIN_BLOCKING_PROGRAM(cmd))
    {
        TRACE("Command '%s' continue_main_process = 0\n", cmd->cmd_str);
        last_main_process = pid;
        continue_main_process = 0;

        if (cmd->pipefd[0] != -1)
            close_pipe_cmd(cmd);
        unpause_children();
        WAIT_FOR(!continue_main_process);
    }
    TRACEL("\n");
    return pid;
}

int execute_commands()
{
    int res, main_blocking_launched = 0;
    st_command *cmd, *next;
    TRACEE("\n");
    for (; recent_cmds; recent_cmds = next)
    {
        res = 0;
        cmd = recent_cmds;
        next = cmd->next; /* take it before cmd gets deleted */
        if (!strcmp(cmd->argv[0], "cd"))
            handle_cd(cmd);
        else if (!strcmp(cmd->argv[0], "export"))
            handle_export(cmd);
        else if (!strcmp(cmd->argv[0], "exit"))
            res = -1;
        else
            res = handle_process(cmd);

        if (last_main_process == res)
            main_blocking_launched = 1;
        /* close a pipe as soon as its second process starts */
        if (last_main_process != res && cmd->pipefd[0] != -1)
            close_pipe_cmd(cmd);

        if (res == -1)
        {
            TRACEL("res -1\n");
            return -1;
        }
    }
    if (!main_blocking_launched)
    {
        unpause_children();
    }
    TRACEL("\n");
    return 0;
}

st_command *compare_pid(st_command *cmd, void *pid)
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

void handle_res_output(st_command *cmd, int status)
{
    /* TODO use some struct to keep info about res codes such as if a process
        was signaled or exited, the actual exit code str etc */
    int code = output_exec_result(status);
    if (cmd->flags & CMD_BG)
    {
        print_term_msg_sig(cmd, code);
        form_post_execution_msg(cmd);
    }
}

void decrease_blocking_processes()
{
    assert(blocking_processes > 0);
    blocking_processes--;
    PTRACE("blocking processes-- %d\n", blocking_processes);
}

void handle_blocking(st_command *cmd)
{
    if (!(cmd->flags & CMD_BLOCKING))
        return;
    decrease_blocking_processes();
    PTRACE("cmd '%s' Wait cont mprcs (%d) bprcs (%d) zguard (%d)\n",
        cmd->cmd_str, continue_main_process, blocking_processes, zombie_guard);
    if (!IS_MAIN_BLOCKING_PROGRAM(cmd))
        return;
    WAIT_FOR((!continue_main_process && blocking_processes) || zombie_guard);
    continue_main_process = 1;
}

void handle_exec_res(st_command *cmd, int status)
{
    PTRACEE("cmd '%s'\n", cmd->cmd_str);
    handle_res_output(cmd, status);
    handle_blocking(cmd);
    WAIT_FOR(zombie_guard);
    RAISE_ZGUARD();
    st_command_erase_item(&cmds_head, cmd);
    DROP_ZGUARD();
    PTRACEL("\n");
}

void check_zombie()
{
    int status;
    pid_t pid = wait4(-1, &status, WNOHANG, NULL);
    st_command *cmd = (st_command *) st_command_find(cmds_head, compare_pid,
        (void *) &pid);
    assert(pid != -1);
    assert(cmd);
    PTRACEE("cmd '%s'\n", cmd->cmd_str);
    handle_exec_res(cmd, status);
    PTRACEL("\n");
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
    st_commands_clear(&cmds_head);
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
/*  print_cmds_head(); */
/* #endif */
    TRACEL("\n");
}
