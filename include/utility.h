#ifndef UTILITY_H
#define UTILITY_H

#include <sys/types.h>

int is_space(char ch);
size_t skip_spaces(const char *str);
int stricmp(const char *lhs, const char *rhs);
char *strdup(const char *str);
size_t argvlen(int argc, char **argv);

#endif /* !UTILITY_H */
