#ifndef _MSGTYPE
#define _MSGTYPE

/*
 * Streams message types for expedited delivery, i.e. enqueued before normal
 * data messages.
 */

/* =()<#define MSG_PROTO @<MSG_PROTO>@>()= */
#define MSG_PROTO M_EXPROTO

/* =()<#define MSG_DATA @<MSG_DATA>@>()= */
#define MSG_DATA M_DATA

/* =()<#define MSG_EXDATA @<MSG_EXDATA>@>()= */
#define MSG_EXDATA M_EXDATA


#if defined(M_EXDATA) && M_EXDATA != MSG_EXDATA && M_EXDATA != MSG_DATA && M_EXDATA != M_DATA
#if MSG_DATA != MSG_EXDATA

#if M_DATA != MSG_DATA
#define CASE_DATA case M_DATA: case MSG_DATA: case MSG_EXDATA: case M_EXDATA:
#else
#define CASE_DATA case MSG_DATA: case MSG_EXDATA: case M_EXDATA:
#endif

#else

#if M_DATA != MSG_DATA
#define CASE_DATA case M_DATA: case MSG_DATA: case M_EXDATA:
#else
#define CASE_DATA case M_DATA: case M_EXDATA:
#endif

#endif
#else
#if MSG_DATA != MSG_EXDATA

#if M_DATA != MSG_DATA
#define CASE_DATA case M_DATA: case MSG_DATA: case MSG_EXDATA:
#else
#define CASE_DATA case MSG_DATA: case MSG_EXDATA:
#endif

#else

#if M_DATA != MSG_DATA
#define CASE_DATA case M_DATA: case MSG_DATA:
#else
#define CASE_DATA case M_DATA:
#endif

#endif
#endif

#endif							/* _MSGTYPE */
