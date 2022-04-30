#ifndef ST_DLL_STRING
#define ST_DLL_STRING

#include "def.h"

/* Double linked list string */
struct st_dll_string
{
	char *str;
	struct st_dll_string *prev, *next;
};

st_dll_string *st_dll_string_create_empty();
st_dll_string *st_dll_string_create(const char *str);
void st_dll_string_push_back(st_dll_string **node, st_dll_string *new_item);
void st_dll_string_push_front(st_dll_string **node, st_dll_string *new_item);
void st_dll_insert_on_callback(st_dll_string **node, st_dll_string *new_item,
	st_dll_string *(*callback)(st_dll_string*, st_dll_string*));
void st_dll_string_print(const st_dll_string *head);
void st_dll_string_clear(st_dll_string *head);
void st_dll_string_sort(st_dll_string **head);
size_t st_dll_string_size(const st_dll_string *head);

#endif /* !ST_DLL_STRING */
