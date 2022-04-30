#ifndef LOG_H
#define LOG_H

#include <assert.h>

#if LOGLVL <= 1
	#define _TRACE
#endif
#if LOGLVL <= 2
	#define _DEBUG
#endif

#define TAB "    "
#ifdef _TRACE
	extern int tabs, __n;

	#define PRINT_TABS \
		for (__n = 0; __n < tabs; __n++) \
			fprintf(stderr, TAB);

	#define PRINT(...) \
		fprintf(stderr, __VA_ARGS__);

	#define TRACER(...) \
		do \
		{ \
			PRINT_TABS \
			PRINT(__VA_ARGS__) \
		} while (0)

	#define EXEC_INFO \
		__FILE__, __LINE__, __FUNCTION__

	#define TRACE(...) \
		do \
		{ \
			PRINT_TABS \
			PRINT("%s: %d | %s() ", EXEC_INFO) \
			PRINT(__VA_ARGS__) \
		} while (0)

	#define NWTRACE(...) \
		do \
		{ \
			PRINT_TABS \
			PRINT(__VA_ARGS__) \
		} while (0)

	/* enter in a function */
	#define TRACEE(...) \
		do \
		{ \
			PRINT_TABS \
			PRINT("%s: %d --> %s() ", EXEC_INFO) \
			PRINT(__VA_ARGS__) \
			tabs++; \
		} while (0)

	/* leave from a function */
	#define TRACEL(...) \
		do \
		{ \
			assert(tabs > 0); \
			tabs--; \
			PRINT_TABS \
			PRINT("%s: %d <-- %s() ", EXEC_INFO) \
			PRINT(__VA_ARGS__) \
		} while (0)
#else
	/* atm I have no idea how UNUSED macros enterily work */
	#define UNUSED0()
	#define UNUSED1(a)                  (void) (a)
	#define UNUSED2(a,b)                (void) (a), UNUSED1(b)
	#define UNUSED3(a,b,c)              (void) (a), UNUSED2(b,c)
	#define UNUSED4(a,b,c,d)            (void) (a), UNUSED3(b,c,d)
	#define UNUSED5(a,b,c,d,e)          (void) (a), UNUSED4(b,c,d,e)
	#define UNUSED6(a,b,c,d,e,f)        (void) (a), UNUSED5(b,c,d,e,f)

	#define VA_NUM_ARGS_IMPL(_0,_1,_2,_3,_4,_5,_6, N,...) N
	#define VA_NUM_ARGS(...) \
		VA_NUM_ARGS_IMPL(100, ## __VA_ARGS__, 6, 5, 4, 3, 2, 1, 0 )

	#define ALL_UNUSED_IMPL_(nargs) UNUSED ## nargs
	#define ALL_UNUSED_IMPL(nargs) ALL_UNUSED_IMPL_(nargs)
	#define ALL_UNUSED(...) \
		ALL_UNUSED_IMPL(VA_NUM_ARGS(__VA_ARGS__)) (__VA_ARGS__)

	#define TRACE(...) ALL_UNUSED(__VA_ARGS__)
	#define TRACEE(...) TRACE(__VA_ARGS__)
	#define TRACEL(...) TRACE(__VA_ARGS__)
	#define NWTRACE(...) TRACE(__VA_ARGS__)
#endif

#endif /* !LOG_H */
