#ifndef ISDN_SAPI
#define ISDN_SAPI

/*
 * Service Access Point Identifiers. (Oder sowas in der Art...)
 */

#define SAPI_PHONE 0
#define SAPI_PHONE_DSS1 8
#define SAPI_PHONE_1TR6_0 64 /* Unused (so far) */
#define SAPI_PHONE_1TR6_1 65

#define SAPI_TEI 63

/* pseudo-SAPIs */
#define SAPI_FIXED 64

#define SAPI_CAPI 65
#define SAPI_CAPI_BINTEC 0

#define SAPI_INVALID 127

#endif
