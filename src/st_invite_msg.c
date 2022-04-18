#include "st_invite_msg.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char *copy_envvar(const char *name)
{
	char *copy;
	char *env = getenv(name);
	if (!env)
	{
		fprintf(stderr, "Failed to get environment variable [%s]\n", name);
		return NULL;
	}
	copy = (char *) malloc(strlen(env) + 1);
	strcpy(copy, env);
	return copy;
}

void replace_home_with_tile(char *path)
{
	static char *home = NULL;
	static size_t home_len = (size_t) -1;
	if (!home)
	{
		home = copy_envvar("HOME");
		if (home)
			home_len = strlen(home);
		else
		{
			fputs("I don't know where your home is :(", stderr);
			return; /* HOME may be abscent */
		}
	}

	if (strncmp(path, home, home_len) == 0)
	{
		path[0] = '~';
		strcpy(path + 1, path + home_len);
	}
}

void append_user_text(st_invite_msg *msg)
{
	char *str = copy_envvar("USER");
	if (!str)
	{
		str = (char *) malloc(1);
		str[0] = '\0';
	}
	msg->user = str;
}

void append_post_user_text(st_invite_msg *msg)
{
	const char defstr[] = ":";
	char *str = (char *) malloc(sizeof(defstr));
	strcpy(str, defstr);
	msg->post_user_text = str;
}

void append_post_path_text(st_invite_msg *msg)
{
	const char defstr[] = "$ ";
	char *str = (char *) malloc(sizeof(defstr));
	strcpy(str, defstr);
	msg->post_path_text = str;
}

void append_abs_path_text(st_invite_msg *msg)
{
	char *cwd = getcwd(NULL, 0);
	if (!cwd)
	{
		perror("Failed to read working directory absolute path");
		exit(1);
	}
	replace_home_with_tile(cwd);
	if (msg->abs_path)
		free(msg->abs_path);
	msg->abs_path = cwd;
}

st_invite_msg *st_invite_msg_form(st_invite_msg **msg)
{
	if (!(*msg))
		*msg = (st_invite_msg *) malloc(sizeof(st_invite_msg));
	if (!(*msg)->user)
		append_user_text(*msg);
	if (!(*msg)->post_user_text)
		append_post_user_text(*msg);
	if (!(*msg)->post_path_text)
		append_post_path_text(*msg);
	append_abs_path_text(*msg);
	return *msg;
}

void st_invite_msg_print(const st_invite_msg *msg)
{
	if (!msg)
	{
		printf("> ");
		return;
	}
	printf("%s%s%s%s", msg->user, msg->post_user_text, msg->abs_path,
		msg->post_path_text);
}
