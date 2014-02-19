#ifndef _pxm_h
#define _pxm_h

#include "cpumask.h"

struct pxm_s {
	char *addr;
	cpumask_t cpumask;
};
typedef struct pxm_s pxm_t;

int pxm_list_free(lub_list_t *pxms);
int show_pxms(lub_list_t *pxms);
int pxm_search(lub_list_t *pxms, const char *addr, cpumask_t *cpumask);
int parse_pxm_config(const char *fname, lub_list_t *pxms, lub_list_t *numas);

#endif
