#ifndef UTILITY_H
#define UTILITY_H

#include <stddef.h>

int is_space(char ch);
size_t skip_spaces(const char *str);
int stricmp(const char *lhs, const char *rhs);
size_t get_argv_total_length(int argc, char **argv);
void free_if_exists(void *data);

#endif /* !UTILITY_H */
