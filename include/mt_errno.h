#ifndef MT_ERRNO_H
#define MT_ERRNO_H

#define MT_OK   0
#define MT_ERR -1

/* Syntax Check <code> */
#define SC_MSQUO 1 /* mismatching quotes */
#define SC_MSRTK 2 /* mismatching reserved tokens */

extern int mt_errno;
extern char mt_errstr[];

#endif /* !MT_ERRNO_H */
