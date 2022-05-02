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
#define _FREE(ptr) \
	do { free(ptr); (ptr) = NULL; _FREED(ptr); } while (0)
#define _FREED(ptr) \
	do { \
		TRACE(#ptr " has been freed and set to NULL\n"); assert(!(ptr)); \
	} while (0)
#define FREE(ptr) \
	do { LOGFREE(ptr);               _FREE(ptr); } while (0)
#define FREE_IFEX(ptr) \
	do { LOGFREE_IFEX(ptr); if (ptr) _FREE(ptr); } while (0)

#define TAB "    "
#ifdef _TRACE
	extern int tabs, __n;

	#define IN_SIGN "-->"
	#define OUT_SIGN "<--"
	#define STAY_SIGN "|"

	#define EXEC_INFO __FILE__, __LINE__, __FUNCTION__
	#define EXEC_PINFO EXEC_INFO, getpid()

	#define INC_TABS() do {                     tabs++; } while(0)
	#define DEC_TABS() do { assert(tabs > 0); tabs--; } while(0)

	#define LOGPRINT(...) fprintf(stderr, __VA_ARGS__)
	#define PRINT_TABS() for (__n = 0; __n < tabs; __n++) LOGPRINT(TAB)
	#define PRINT_INFO(sign) \
		LOGPRINT("%s: %d "sign" %s() ", EXEC_INFO)
	#define PRINT_PINFO(sign) \
		LOGPRINT("%s: %d "sign" %s() [PID: %d] ", EXEC_PINFO)

	/* Raw trace logging macro, no file information is provided */
	#define TRACER(...) do { PRINT_TABS(); LOGPRINT(__VA_ARGS__); } while (0)

	/* Regular trace logging macro */
	#define _TRACE_IMPL(SIGN, ...) \
		do { \
			PRINT_TABS(); \
			PRINT_INFO(SIGN); \
			LOGPRINT(__VA_ARGS__); \
		} while (0)

	#define TRACE(...) _TRACE_IMPL(STAY_SIGN, __VA_ARGS__)
	#define TRACEE(...) \
		do { \
			_TRACE_IMPL(IN_SIGN, __VA_ARGS__); \
			INC_TABS(); \
		} while (0)
	#define TRACEL(...) \
		do { \
			DEC_TABS(); \
			_TRACE_IMPL(OUT_SIGN, __VA_ARGS__); \
		} while (0)


	/* Trace logging macro with PID */
	#define _PTRACE_IMPL(sign, ...) \
		do { \
			PRINT_TABS(); \
			PRINT_PINFO(sign); \
			LOGPRINT(__VA_ARGS__); \
		} while (0)
	#define PTRACE(...) _PTRACE_IMPL(STAY_SIGN, __VA_ARGS__)
	#define PTRACEE(...) \
		do { \
			_PTRACE_IMPL(IN_SIGN, __VA_ARGS__); \
			INC_TABS(); \
		} while (0)
	#define PTRACEL(...) \
		do { \
			DEC_TABS(); \
			_PTRACE_IMPL(OUT_SIGN, __VA_ARGS__); \
		} while (0)


	/* Trace memory allocation logging macro */
	#define _ATRACE_IMPL(sign, ptr) \
		do { \
			PRINT_TABS(); \
			PRINT_INFO(sign); \
			LOGPRINT("Allocated %zu for ptr '%p'\n", sizeof(*ptr), \
				(void *) (ptr)); \
			assert(ptr); \
		} while (0)
	#define ATRACE(ptr) _ATRACE_IMPL(STAY_SIGN, ptr)
	#define ATRACEE(ptr) \
		do { \
			_ATRACE_IMPL(IN_SIGN, ptr); \
			INC_TABS(); \
		} while (0)
	#define ATRACEL(ptr) \
		do { \
			DEC_TABS(); \
			_ATRACE_IMPL(OUT_SIGN, ptr); \
		} while (0)


	/* Log a clear recursive function enterance event */
	#define TRACEEC(...) \
		do { \
			PRINT_INFO(IN_SIGN); \
			LOGPRINT(__VA_ARGS__); \
		} while (0)
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
		} while (0)
	#define LOGFREE_IFEX(ptr) \
		do { \
			TRACE("free if exists '" #ptr "' %p\n", (void *) (ptr)); \
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

	#define TRACER(...)                 ALL_UNUSED(__VA_ARGS__)
	#define TRACE(...)                  TRACER(__VA_ARGS__)
	#define PTRACE(...)                 TRACER(__VA_ARGS__)
	#define PTRACEE(...)                TRACER(__VA_ARGS__)
	#define PTRACEL(...)                TRACER(__VA_ARGS__)
	#define ATRACE(...)                TRACER(__VA_ARGS__)
	#define ATRACEE(...)                TRACER(__VA_ARGS__)
	#define ATRACEL(...)                TRACER(__VA_ARGS__)
	#define TRACEE(...)                 TRACER(__VA_ARGS__)
	#define TRACEL(...)                 TRACER(__VA_ARGS__)
	#define TRACEEC(...)                TRACER(__VA_ARGS__)
	#define TRACELC(...)                TRACER(__VA_ARGS__)

	#define LOGFREE(ptr)      UNUSED1(ptr)
	#define LOGFREE_IFEX(ptr) UNUSED1(ptr)
	#define LOGCFNPP(ptr)     do { assert(ptr); } while (0)
	#define LOGFNPP(ptr)      do { assert(ptr); assert(*ptr); } while (0)
	#define LOGFNE(ptr, item) do { LOGFNPP(ptr); assert(item); } while(0)
#endif

#endif /* !LOG_H */
