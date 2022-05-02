#include "st_redirector.h"
#include "utility.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

st_redirector *st_redirector_create_empty()
{
	st_redirector *item = (st_redirector *) malloc(sizeof(st_redirector));
	ATRACEE(item);
	item->fd = -1;
	item->app = 0;
	item->path = NULL;
	item->dir = rd_ukw;
	TRACEL("\n");
	return item;
}

void st_redirector_delete(st_redirector **item)
{
	LOGFNPP(item);
	FREE_IFEX((*item)->path);
	FREE(*item);
	TRACEL("\n");
}

void st_redirector_print(st_redirector *item)
{
	printf("path '%s' - '%p', fd '%d', app '%d', dir '%s'", item->path,
		(void *) item, item->fd, item->app, item->dir == rd_in ? "in" :
		item->dir == rd_out ? "out" : "unknown");
}

st_redirector *st_redirector_dup(const st_redirector *src)
{
	TRACEE("Source addr '%p'\n", (void *) src);
	assert(src);
	st_redirector *dest = (st_redirector *) malloc (sizeof(st_redirector));
	ATRACE(dest);
	memcpy(dest, src, sizeof(st_redirector));
	if (src->path)
		dest->path = strdup(src->path);
	TRACEL("\n");
	return dest;
}
