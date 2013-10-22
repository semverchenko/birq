#ifndef _nl_h
#define _nl_h

int nl_init(void);
void nl_close(int nl);
int nl_poll(int nl, int timeout);

#endif
