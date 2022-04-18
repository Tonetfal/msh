#include "eid.h"

#include <stdio.h>

/* Supports only 1000 simulteneous background processes */
#define MAX_EID 1000
static int available_eids[MAX_EID] = { 0 };

eid_t acquire_eid()
{
	int i = 0;
	while (available_eids[i] == 1)
		i++;
	if (i > MAX_EID)
	{
		fputs("A process couldn't receive an own EID.\n", stderr);
		return -1;
	}
	available_eids[i] = 1;
	return available_eids[i];
}

void release_eid(eid_t eid)
{
	if (eid < 0 || eid > MAX_EID)
		return;
	available_eids[eid] = 0;
}
