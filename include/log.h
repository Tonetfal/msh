#ifndef LOG_H
#define LOG_H

#include <assert.h>

#ifdef LOGLVL
	#if LOGLVL <= 1
		#define _TRACE
	#endif
#endif

/*
 * Some logging is enabled only with determined LOGLVL
 */

/* Log a memory free call and set the ptr to NULL */
#define FREE(ptr) do { LOGFREE(ptr); (ptr) = NULL; } while (0)
#define FREE_IFEX(ptr) free_if_exists((void *) ptr)

#define TAB "    "
#ifdef _TRACE
	extern int tabs, __n;

	#define IN_SIGN "-->"
	#define OUT_SIGN "<--"
	#define STAY_SIGN "|"

	#define EXEC_INFO __FILE__, __LINE__, __FUNCTION__
	#define EXEC_PINFO EXEC_INFO, getpid()

	#define LOGPRINT(...) fprintf(stderr, __VA_ARGS__)
	#define PRINT_TABS() for (__n = 0; __n < tabs; __n++) LOGPRINT(TAB)
	#define PRINT_INFO(sign) \
		LOGPRINT("%s: %d "sign" %s() ", EXEC_INFO)
	#define PRINT_PINFO(sign) \
		LOGPRINT("%s: %d "sign" %s() [PID: %d] ", EXEC_PINFO)

	/* Raw trace logging macro, no file information is provided */
	#define TRACER(...) do { PRINT_TABS() LOGPRINT(__VA_ARGS__) } while (0)

	/* Regular trace logging macro */
	#define TRACE(...) \
		do { \
			PRINT_TABS(); \
			PRINT_INFO(STAY_SIGN); \
			LOGPRINT(__VA_ARGS__); \
		} while (0)

	/* Trace logging macro with PID */
	#define PTRACE(...) \
		do { \
			PRINT_TABS(); \
			PRINT_PINFO(STAY_SIGN); \
			LOGPRINT(__VA_ARGS__); \
		} while (0)

	/* Log a function entrance event */
	#define TRACEE(...) \
		do { \
			PRINT_TABS(); \
			PRINT_INFO(IN_SIGN); \
			LOGPRINT(__VA_ARGS__); \
			tabs++; \
		} while (0)

	/* Log a function exit event */
	#define TRACEL(...) \
		do { \
			assert(tabs > 0); \
			tabs--; \
			PRINT_TABS(); \
			PRINT_INFO(OUT_SIGN); \
			LOGPRINT(__VA_ARGS__); \
		} while (0)

	/* Log a function entrance event with pid */
	#define PTRACEE(...) \
		do { \
			PRINT_TABS(); \
			PRINT_PINFO(IN_SIGN); \
			LOGPRINT(__VA_ARGS__); \
			tabs++; \
		} while (0)

	/* Log a function exit event with pid */
	#define PTRACEL(...) \
		do { \
			assert(tabs > 0); \
			tabs--; \
			PRINT_TABS(); \
			PRINT_PINFO(OUT_SIGN); \
			LOGPRINT(__VA_ARGS__); \
		} while (0)

	/* Log a clear recursive function enterance event */
	#define TRACEEC(...) \
		do { \
			PRINT_INFO(IN_SIGN); \
			LOGPRINT(__VA_ARGS__); \
		} while (0)

	/* Log a clear recursvie function exit event */
	#define TRACELC(...) \
		do { \
			PRINT_TABS(); \
			PRINT_INFO(OUT_SIGN); \
			LOGPRINT(__VA_ARGS__); \
		} while (0)

	/* Log a memory free call */
	#define LOGFREE(ptr) \
		do { \
			TRACE("free '" #ptr "' %p\n", (void *) (ptr)); \
			free(ptr); \
		} while (0)

	/* Control and log ptr passed to a clear recursive function */
	#define LOGCFNPP(ptr) \
		do { \
			assert(ptr); \
			TRACEEC(#ptr " addr %p *" #ptr " addr %p\n",  \
				(void *) (ptr), (void *) *(ptr)); \
		} while (0)

	/* Control and log ptr to ptr passed to a delete function */
	#define LOGFNPP(ptr) \
		do { \
			assert(ptr); \
			assert(*ptr); \
			TRACEE(#ptr " addr %p *" #ptr " addr %p\n",  \
				(void *) (ptr), (void *) *(ptr)); \
		} while (0)

	/* Control and log ptr to ptr and item passed to an erase function */
	#define LOGFNE(ptr, item) \
		do { \
			assert(ptr); \
			assert(*ptr); \
			assert(item); \
			TRACEE(#ptr " addr %p *" #ptr " addr %p " #item " addr %p\n", \
				(void *) (ptr), (void *) *(ptr), (void *) (item)); \
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

	#define TRACE(...)                  ALL_UNUSED(__VA_ARGS__)
	#define PTRACE(...)                 TRACE(__VA_ARGS__)
	#define PTRACEE(...)                TRACE(__VA_ARGS__)
	#define PTRACEL(...)                TRACE(__VA_ARGS__)
	#define TRACEE(...)                 TRACE(__VA_ARGS__)
	#define TRACEL(...)                 TRACE(__VA_ARGS__)
	#define TRACEEC(...)                TRACE(__VA_ARGS__)
	#define TRACELC(...)                TRACE(__VA_ARGS__)

	#define LOGFREE(ptr)      UNUSED1(ptr)
	#define LOGCFNPP(ptr)     do { assert(ptr); } while (0)
	#define LOGFNPP(ptr)      do { assert(ptr); assert(*ptr); } while (0)
	#define LOGFNE(ptr, item) do { LOGFNPP(ptr); assert(item); } while(0)
#endif

#endif /* !LOG_H */
