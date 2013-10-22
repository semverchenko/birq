/*
 * birq
 *
 * Balance IRQ
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <sys/poll.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/netlink.h>

#include "lub/log.h"

#ifndef VERSION
#define VERSION 1.0.0
#endif
#define QUOTE(t) #t
#define version(v) printf("%s\n", v)

#define BIRQ_PIDFILE "/var/run/birq.pid"

/* Global signal vars */
static volatile int sigterm = 0;
static void sighandler(int signo);

static void help(int status, const char *argv0);
struct options *opts_init(void);
void opts_free(struct options *opts);
static int opts_parse(int argc, char *argv[], struct options *opts);

static int nl_init(void);
static void nl_close(int nl);
static int nl_poll(int nl, int timeout);

/* Command line options */
struct options {
	char *pidfile;
	int debug; /* Don't daemonize in debug mode */
	int log_facility;
};

/*--------------------------------------------------------- */
int main(int argc, char **argv)
{
	int retval = -1;
	struct options *opts = NULL;
	int pidfd = -1;
	int rescan = 0; /* sysfs rescan needed */

	/* Signal vars */
	struct sigaction sig_act;
	sigset_t sig_set;

	/* NetLink vars */
	int nl = -1; /* NetLink socket */

	/* Parse command line options */
	opts = opts_init();
	if (opts_parse(argc, argv, opts))
		goto err;

	/* Initialize syslog */
	openlog(argv[0], LOG_CONS, opts->log_facility);
	syslog(LOG_ERR, "Start daemon.\n");

	/* Init NetLink socket */
	if ((nl = nl_init()) < 0)
		goto err;

	/* Fork the daemon */
	if (!opts->debug) {
		/* Daemonize */
		if (daemon(0, 0) < 0) {
			syslog(LOG_ERR, "Can't daemonize\n");
			goto err;
		}

		/* Write pidfile */
		if ((pidfd = open(opts->pidfile,
			O_WRONLY | O_CREAT | O_EXCL | O_TRUNC,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
			syslog(LOG_WARNING, "Can't open pidfile %s: %s",
				opts->pidfile, strerror(errno));
		} else {
			char str[20];
			snprintf(str, sizeof(str), "%u\n", getpid());
			if (write(pidfd, str, strlen(str)) < 0)
				syslog(LOG_WARNING, "Can't write to %s: %s",
					opts->pidfile, strerror(errno));
			close(pidfd);
		}
	}

	/* Set signal handler */
	sigemptyset(&sig_set);
	sigaddset(&sig_set, SIGTERM);
	sigaddset(&sig_set, SIGINT);
	sigaddset(&sig_set, SIGQUIT);

	sig_act.sa_flags = 0;
	sig_act.sa_mask = sig_set;
	sig_act.sa_handler = &sighandler;
	sigaction(SIGTERM, &sig_act, NULL);
	sigaction(SIGINT, &sig_act, NULL);
	sigaction(SIGQUIT, &sig_act, NULL);

	/* Main loop */
	while (!sigterm) {
		int n;
		n = nl_poll(nl, 5);
		if (n < 0) {
			if (-2 == n) /* EINTR */
				continue;
			break;
		}
		if (n > 0) {
			rescan = 1;
			continue;
		}

		if (rescan) {
			fprintf(stdout, "Rescanning...\n");
			rescan = 0;
		}
	}

	retval = 0;
err:
	/* Close NetLink socket */
	nl_close(nl);

	/* Remove pidfile */
	if (pidfd >= 0) {
		if (unlink(opts->pidfile) < 0) {
			syslog(LOG_ERR, "Can't remove pid-file %s: %s\n",
			opts->pidfile, strerror(errno));
		}
	}

	/* Free command line options */
	opts_free(opts);

	syslog(LOG_ERR, "Stop daemon.\n");

	return retval;
}

static int nl_init(void)
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

static void nl_close(int nl)
{
	if (nl >= 0)
		close(nl);
}

static int nl_poll(int nl, int timeout)
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

/*--------------------------------------------------------- */
/*
 * Signal handler for temination signals (like SIGTERM, SIGINT, ...)
 */
static void sighandler(int signo)
{
	sigterm = 1;
}

/*--------------------------------------------------------- */
/* Initialize option structure by defaults */
struct options *opts_init(void)
{
	struct options *opts = NULL;

	opts = malloc(sizeof(*opts));
	assert(opts);
	opts->debug = 0; /* daemonize by default */
	opts->pidfile = strdup(BIRQ_PIDFILE);
	opts->log_facility = LOG_DAEMON;

	return opts;
}

/*--------------------------------------------------------- */
/* Free option structure */
void opts_free(struct options *opts)
{
	if (opts->pidfile)
		free(opts->pidfile);
	free(opts);
}

/*--------------------------------------------------------- */
/* Parse command line options */
static int opts_parse(int argc, char *argv[], struct options *opts)
{
	static const char *shortopts = "hvp:dO:";
#ifdef HAVE_GETOPT_H
	static const struct option longopts[] = {
		{"help",	0, NULL, 'h'},
		{"version",	0, NULL, 'v'},
		{"pid",		1, NULL, 'p'},
		{"debug",	0, NULL, 'd'},
		{"facility",	1, NULL, 'O'},
		{NULL,		0, NULL, 0}
	};
#endif
	optind = 1;
	while(1) {
		int opt;
#ifdef HAVE_GETOPT_H
		opt = getopt_long(argc, argv, shortopts, longopts, NULL);
#else
		opt = getopt(argc, argv, shortopts);
#endif
		if (-1 == opt)
			break;
		switch (opt) {
		case 'p':
			if (opts->pidfile)
				free(opts->pidfile);
			opts->pidfile = strdup(optarg);
			break;
		case 'd':
			opts->debug = 1;
			break;
		case 'O':
			if (lub_log_facility(optarg, &(opts->log_facility))) {
				fprintf(stderr, "Error: Illegal syslog facility %s.\n", optarg);
				help(-1, argv[0]);
				exit(-1);
			}
			break;
		case 'h':
			help(0, argv[0]);
			exit(0);
			break;
		case 'v':
			version(VERSION);
			exit(0);
			break;
		default:
			help(-1, argv[0]);
			exit(-1);
			break;
		}
	}

	return 0;
}

/*--------------------------------------------------------- */
/* Print help message */
static void help(int status, const char *argv0)
{
	const char *name = NULL;

	if (!argv0)
		return;

	/* Find the basename */
	name = strrchr(argv0, '/');
	if (name)
		name++;
	else
		name = argv0;

	if (status != 0) {
		fprintf(stderr, "Try `%s -h' for more information.\n",
			name);
	} else {
		printf("Usage: %s [options]\n", name);
		printf("Daemon to store user configuration (i.e. commands). "
			"The part of the klish project.\n");
		printf("Options:\n");
		printf("\t-v, --version\tPrint version.\n");
		printf("\t-h, --help\tPrint this help.\n");
		printf("\t-d, --debug\tDebug mode. Don't daemonize.\n");
		printf("\t-p <path>, --pid=<path>\tFile to save daemon's PID to.\n");
		printf("\t-O, --facility\tSyslog facility. Default is DAEMON.\n");
	}
}
