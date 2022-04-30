#ifndef DEF_H
#define DEF_H

#include <sys/types.h>

#define SHELL "msh"
#define UNUSED(x) (void) (x) /* ignore warning */
#define FWD(STRUCT) \
	struct STRUCT; \
	typedef struct STRUCT STRUCT;

FWD(st_command)
FWD(st_dll_string)
FWD(st_invite_msg)
FWD(st_redirector)
FWD(st_token)

#undef FWD

#endif /* !DEF_H */
