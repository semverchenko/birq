#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <asm/types.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <poll.h>

#include "nl.h"

nl_fds_t * nl_init(void)
{
	struct sockaddr_nl nl_addr;
	int nl;

	memset(&nl_addr, 0, sizeof(nl_addr));
	nl_addr.nl_family = AF_NETLINK;
	nl_addr.nl_pad = 0;
	nl_addr.nl_pid = 0; /* Let kernel to assign id */
	nl_addr.nl_groups = -1; /* Listen all multicast */

	if ((nl = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT)) < 0) {
		fprintf(stderr, "Error: Can't create socket\n");
		return -1;
	}
	if (bind(nl, (void *)&nl_addr, sizeof(nl_addr))) {
		fprintf(stderr, "Error: Can't bind NetLink\n");
		return -1;
	}

	return nl;
}

void nl_close(nl_fds_t *nl_fds)
{
	int fd;
	int i;

	if (!nl_fds)
		return;

	for (i = 0; i < NL_FDS_LEN; i++) {
		if (nl_fds[i] >= 0)
			close(nl_fds[i]);
	}
}

int nl_poll(nl_fds_t *nl_fds, int timeout)
{
	struct pollfd pfd;
	char buf[10];
	int n;

	pfd.events = POLLIN;
	pfd.fd = nl;

	n = poll(&pfd, 1, (timeout * 1000));
	if (n < 0) {
		if (EINTR == errno)
			return -2;
		return -1;
	}
	/* Some device-related event */
	/* Read all messages. We don't need a message content. */
	if (n > 0)
		while (recv(nl, buf, sizeof(buf), MSG_DONTWAIT) > 0);

	return n;
}
