#include "utility.h"

#include <ctype.h>
#include <stdio.h>
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

size_t get_argv_total_length(int argc, char **argv)
{
	int i = 0;
	size_t len = 0ul;
	for (; i < argc; i++)
	{
		len += strlen(argv[i]);
#ifdef DEBUG
		fprintf(stderr, "get_argv_total_length() - argv[%d] [%s] len %lu\n",
			i, argv[i], len);
#endif
	}
#ifdef DEBUG
	fprintf(stderr, "get_argv_total_length() - len %lu\n", len);
#endif
	return len;
}
