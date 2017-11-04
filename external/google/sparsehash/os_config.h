#ifndef _MSC_VER
//non-win version

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if the system has the type `uint16_t'. */
#define HAVE_UINT16_T 1

/* Define to 1 if the system has the type `u_int16_t'. */
#define HAVE_U_INT16_T 1

/* Define to 1 if the system has the type `__uint16'. */
/* #undef HAVE___UINT16 */

#else
//win version

/* Define to 1 if you have the <inttypes.h> header file. */
#undef HAVE_INTTYPES_H

/* Define to 1 if you have the <stdint.h> header file. */
#undef HAVE_STDINT_H

/* Define to 1 if the system has the type `uint16_t'. */
#undef HAVE_UINT16_T

/* Define to 1 if the system has the type `u_int16_t'. */
#undef HAVE_U_INT16_T

/* Define to 1 if the system has the type `__uint16'. */
#define HAVE___UINT16  1

#endif
