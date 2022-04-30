#ifndef ST_REDIRECTOR_H
#define ST_REDIRECTOR_H

#include "def.h"

struct st_redirector
{
	int fd, app;
	char *path;
	enum { rd_in, rd_out, rd_ukw } dir;
};

st_redirector *st_redirector_create_empty();
void st_redirector_delete(st_redirector *item);
void st_redirector_print(st_redirector *item);

#endif /* ST_REDIRECTOR_H */
