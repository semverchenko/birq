#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <asm/types.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#include "nl.h"

nl_fds_t * nl_init(void)
{
	struct sockaddr_nl nl_addr;
	nl_fds_t *nl_fds;

	nl_fds = malloc(sizeof(*nl_fds) * NL_FDS_LEN);
	if (!nl_fds)
		return NULL;

	memset(&nl_addr, 0, sizeof(nl_addr));
	nl_addr.nl_family = AF_NETLINK;
	nl_addr.nl_pad = 0;
	nl_addr.nl_pid = 0; /* Let kernel to assign id */
	nl_addr.nl_groups = -1; /* Listen all multicast */

	/* NETLINK_KOBJECT_UEVENT for PCI events */
	if ((nl_fds[0] = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT)) < 0) {
		fprintf(stderr, "Error: Can't create socket for NETLINK_KOBJECT_UEVENT.\n");
		return NULL;
	}
	if (bind(nl_fds[0], (void *)&nl_addr, sizeof(nl_addr))) {
		fprintf(stderr, "Error: Can't bind NETLINK_KOBJECT_UEVENT.\n");
		return NULL;
	}
printf("KOBJECT=%d\n", nl_fds[0]);

	/* NETLINK_ROUTER for network events like interface up/down */
	if ((nl_fds[1] = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE)) < 0) {
		fprintf(stderr, "Error: Can't create socket for NETLINK_ROUTER\n");
		return NULL;
	}
	if (bind(nl_fds[1], (void *)&nl_addr, sizeof(nl_addr))) {
		fprintf(stderr, "Error: Can't bind NETLINK_ROUTER\n");
		return NULL;
	}
printf("ROUTE=%d\n", nl_fds[1]);

	return nl_fds;
}

void nl_close(nl_fds_t *nl_fds)
{
	int i;

	if (!nl_fds)
		return;

	for (i = 0; i < NL_FDS_LEN; i++) {
		if (nl_fds[i] >= 0)
			close(nl_fds[i]);
	}

	free(nl_fds);
}

int nl_poll(nl_fds_t *nl_fds, int timeout)
{
	fd_set fd_set;
	char buf[10];
	int n;
	int i;
	int nfds = 0;
	struct timeval tv;

	if (!nl_fds)
		return -1;

	/* Initialize the set of active sockets. */
	FD_ZERO(&fd_set);
	for (i = 0; i < NL_FDS_LEN; i++) {
		FD_SET(nl_fds[i], &fd_set);
		if (nl_fds[i] > nfds)
			nfds = nl_fds[i];
	}
	nfds++;

	/* Wait up to five seconds. */
	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	n = select(nfds, &fd_set, NULL, NULL, &tv);
printf("NETLINK: n=%d\n", n);
	if (n < 0) {
		if (EINTR == errno)
			return -2;
		return -1;
	}
	if (0 == n)
		return n;

	/* Service all the sockets with input pending. */
	for (i = 0; i < NL_FDS_LEN; i++) {
		if (!FD_ISSET(nl_fds[i], &fd_set))
				continue;
printf("RECV %d %d\n", i, nl_fds[i]);
		/* Read all messages. We don't need a message content. */
		while (recv(nl_fds[i], buf, sizeof(buf), MSG_DONTWAIT) > 0);
	}

	return n;
}
