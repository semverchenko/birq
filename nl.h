#ifndef _nl_h
#define _nl_h

#define NL_FDS_LEN 2

typedef int nl_fds_t;

nl_fds_t * nl_init(void);
void nl_close(nl_fds_t *nl_fds);
int nl_poll(nl_fds_t *nl_fds, int timeout);

#endif
