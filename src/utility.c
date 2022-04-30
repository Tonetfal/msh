#include "utility.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int is_space(char ch)
{
	return ch == ' ' || ch == '\t';
}

size_t skip_spaces(const char *str)
{
	size_t i = 0ul;
	while (is_space(str[i]) && str[i] != '\0')
		i++;
	return i;
}

int stricmp(const char *lhs, const char *rhs)
{
	char clhs, crhs;
	while (lhs && rhs)
	{
		clhs = tolower(*lhs);
		crhs = tolower(*rhs);

		if (clhs != crhs)
			return clhs - crhs;

		lhs++;
		rhs++;
	}
	return 0;
}

char *strdup(const char *str)
{
	char *cpy = NULL;
	if (str)
	{
		cpy = (char *) malloc(strlen(str) + 1);
		if (cpy)
			strcpy(cpy, str);
	}
	return cpy;
}

size_t argvlen(int argc, char **argv)
{
	if (argc == 0)
		return 0;
	return argvlen(argc - 1, argv + 1) + strlen(*argv);
}

void free_if_exists(void *data)
{
	if (data)
		free(data);
}
