#include "io_utility.h"
#include "mt_errno.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *read_string(char *buf, const size_t size)
{
	char *res;
	errno = 0;
	res= fgets(buf, size - 1, stdin);
	if (!res)
	{
		if (errno == 0)
			return NULL;
		perror("Failed to read a string");
		exit(1);
	}
	res[strlen(res) - 1] = '\0';
	return res;
}

void mt_perror(const char *str)
{
	if (str)
		fprintf(stderr, "%s: ", str);

	if (mt_errno == MT_OK)
		fputs("Success.\n", stderr);
	else if (mt_errno == SC_MSQUO)
		fputs("Mismathicng quotes.\n", stderr);
	else if (mt_errno == SC_MSRTK)
		fprintf(stderr, "Mismatching reserved tokens. %s\n", mt_errstr);
	else
		fprintf(stderr, "Unknown error code - %d.\n", mt_errno);
}

void print_argv(char **argv)
{
	size_t i;
	for (i = 0ul; argv[i]; i++)
	{
		printf("%s", argv[i]);
		if (i > 0)
			putchar(' ');
	}
}
