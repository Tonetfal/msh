#include "st_redirector.h"

#include <stdio.h>
#include <stdlib.h>

st_redirector *st_redirector_create_empty()
{
	st_redirector *item = (st_redirector *) malloc(sizeof(st_redirector));
	item->fd = -1;
	item->app = 0;
	item->path = NULL;
	item->dir = rd_ukw;
	return item;
}

void st_redirector_delete(st_redirector *item)
{
	if (!item)
		return;
	if (item->path)
		free(item->path);
	free(item);
}

void st_redirector_print(st_redirector *item)
{
	printf("path [%s] fd [%d] app [%d] dir [%s]", item->path, item->fd,
		item->app, item->dir == rd_in ? "in" : item->dir == rd_out ? "out" :
		"unknown");
}
