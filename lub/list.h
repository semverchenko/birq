#ifndef _lub_list_h
#define _lub_list_h

#include <stddef.h>
#include "lub/c_decl.h"

typedef struct lub_list_node_s lub_list_node_t;
typedef int lub_list_compare_fn(const void *first, const void *second);
typedef struct lub_list_s lub_list_t;
typedef struct lub_list_node_s lub_list_iterator_t;

_BEGIN_C_DECL

lub_list_t *lub_list_new(lub_list_compare_fn compareFn);
lub_list_node_t *lub_list_node_new(void *data);
void lub_list_free(lub_list_t *list);
void lub_list_node_free(lub_list_node_t *node);
lub_list_node_t *lub_list__get_head(lub_list_t *list);
lub_list_node_t *lub_list__get_tail(lub_list_t *list);
lub_list_node_t *lub_list_node__get_prev(lub_list_node_t *node);
lub_list_node_t *lub_list_node__get_next(lub_list_node_t *node);
void *lub_list_node__get_data(lub_list_node_t *node);
lub_list_node_t *lub_list_iterator_init(lub_list_t *list);
lub_list_node_t *lub_list_iterator_next(lub_list_node_t *node);
lub_list_node_t *lub_list_iterator_prev(lub_list_node_t *node);
lub_list_node_t *lub_list_add(lub_list_t *list, void *data);
void lub_list_del(lub_list_t *list, lub_list_node_t *node);
void lub_list_node_copy(lub_list_node_t *dst, lub_list_node_t *src);

_END_C_DECL
#endif				/* _lub_list_h */

