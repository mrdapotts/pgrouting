#include <stdio.h>
#include <stdlib.h>
#include <postgres.h>  /* postgres */

#include <search.h>    /* hash search */
#include <string.h>    /* memcpy */
#include <math.h>      /* exp    */

#include "tsp.h"
#define DBG_PRINT_LEN	300

/**
 * Wrapper functions for memory allocation.  It seems its impossible to access
 * palloc/pfree without running in to problems with the name managler 
 */
void * getMem(size_t request ){
	return palloc(request);
}
void releaseMem(void *mem ){
	pfree (mem);
}
/**
 * Postgres does not appear direct access to elog from c++
 * hence this hack.
 */
void dbgprintf(const char *str,...){
	va_list ap;
	char buff[DBG_PRINT_LEN];

#ifdef DEBUG
	va_start(ap,str);
	vsnprintf(buff,DBG_PRINT_LEN,str,ap);
	va_end(ap);

        elog(NOTICE, "%s",buff );
#endif 
}
