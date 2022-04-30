#include "io_utility.h"
#include "mt_errno.h"
#include "log.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

char *read_string(char *buf, const size_t size)
{
	char *res = NULL;
	do
	{
		errno = 0;
		res = fgets(buf, size - 1, stdin);
		if (errno == EINTR)
			continue;
		if (!res)
		{
			if (errno == 0)
				continue;
			perror("Failed to read a string");
			exit(1);
		}
	} while(!res);
	res[strlen(res) - 1] = '\0';
	TRACE("read_string() - str [%s]\n", res);
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
	else if (mt_errno == SC_MSRDT)
		fprintf(stderr, "Mismatching redirector tokens. %s\n", mt_errstr);
	else if (mt_errno == SC_MSREP)
		fprintf(stderr, "Missing redirector path. %s\n", mt_errstr);
	else if (mt_errno == SC_USPFD)
		fprintf(stderr, "Unsupported file descriptor. %s\n", mt_errstr);
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

void print_byte(int byte)
{
	int i;
	for (i = 0; i < 8; i++)
		putchar(!!(byte & (1 << i)) + '0');
}

void print_flags(void *flags, int bytes)
{
	int i;
	char *arr = (char *) malloc(bytes);
	memcpy(arr, flags, bytes);
	for (i = 0; i < bytes; i++)
		print_byte(arr[i]);
	free(arr);
}
