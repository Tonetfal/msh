#ifndef ST_INVITE_MSG_H
#define ST_INVITE_MSG_H

#include "def.h"

struct st_invite_msg
{
	char *user;
	char *post_user_text;
	char *abs_path;
	char *post_path_text;
};

st_invite_msg *st_invite_msg_create_empty();
st_invite_msg *st_invite_msg_form(st_invite_msg **msg);
void st_invite_msg_print(const st_invite_msg *msg);
void st_invite_msg_delete(st_invite_msg *item);

#endif /* !ST_INVITE_MSG_H */
