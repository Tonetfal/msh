#include "eid.h"

#include <stdio.h>

/* Supports only 1000 simultaneous background processes */
#define MAX_EID 1000
static int available_eids[MAX_EID] = { 0 };

/*
 * EID module acts as if it managed own IDs from index 1, however internally
 * it manages them from 0. The acquired and released EIDs from functions are
 * incremented and decremented by one respectively.
*/
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
	return i + 1;
}

void release_eid(eid_t eid)
{
	eid--;
	if (eid < 0 || eid > MAX_EID)
		return;
	available_eids[eid] = 0;
}
