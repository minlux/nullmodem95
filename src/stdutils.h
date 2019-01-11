//-----------------------------------------------------------------------------
/*!
   \file
   \brief Utility function "normally" provided by standard (C) libraries.

*/
//-----------------------------------------------------------------------------
#ifndef STDUTIL_H_
#define STDUTIL_H_

/* -- Includes ------------------------------------------------------------ */
#include "basedef.h"


#ifdef __cplusplus
extern "C" {
#endif

/* -- Defines ------------------------------------------------------------- */

/* -- Types --------------------------------------------------------------- */

/* -- Global Variables ---------------------------------------------------- */

/* -- Function Prototypes ------------------------------------------------- */
unsigned int stdutils_uitoa(char * destination, unsigned long value, unsigned int num);
unsigned int stdutils_strncpy(char * destination, const char * source, unsigned int num);
int stdutils_strncmp(const char * str1, const char * str2, unsigned int num);
void stdutils_memclr(void * memory, unsigned int num);


/* -- Implementation ------------------------------------------------------ */



#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif






