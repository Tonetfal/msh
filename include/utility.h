#ifndef UTILITY_H
#define UTILITY_H

#include <stddef.h>

int is_space(char ch);
size_t skip_spaces(const char *str);
int stricmp(const char *lhs, const char *rhs);
size_t get_argv_total_length(int argc, char **argv);

#endif /* !UTILITY_H */
