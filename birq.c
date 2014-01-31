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
#include "lub/list.h"
#include "irq.h"
#include "cpu.h"
#include "nl.h"
#include "statistics.h"

#define BIRQ_PIDFILE "/var/run/birq.pid"
#define BIRQ_INTERVAL 3 /* in seconds */

#ifndef VERSION
#define VERSION 1.0.0
#endif
#define QUOTE(t) #t
#define version(v) printf("%s\n", v)

static volatile int sigterm = 0; /* Exit if 1 */
static volatile int rescan = 1; /* IRQ rescan is needed */

/* Signal handlers */
static void sighandler(int signo);
static void rescan_sighandler(int signo);

static void help(int status, const char *argv0);
static struct options *opts_init(void);
static void opts_free(struct options *opts);
static int opts_parse(int argc, char *argv[], struct options *opts);

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

	/* Signal vars */
	struct sigaction sig_act;
	sigset_t sig_set;

	/* NetLink vars */
	nl_fds_t *nl_fds = NULL; /* NetLink socket */

	/* IRQ list. It contain all found irqs. */
	lub_list_t *irqs;
	/* CPU list. It contain all found CPUs. */
	lub_list_t *cpus;

	/* Parse command line options */
	opts = opts_init();
	if (opts_parse(argc, argv, opts))
		goto err;

	/* Initialize syslog */
	openlog(argv[0], LOG_CONS, opts->log_facility);
	syslog(LOG_ERR, "Start daemon.\n");

	/* Init NetLink socket */
	if (!(nl_fds = nl_init()))
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

	/* Set rescan signal handler */
	sigemptyset(&sig_set);
	sigaddset(&sig_set, SIGUSR1);

	sig_act.sa_flags = 0;
	sig_act.sa_mask = sig_set;
	sig_act.sa_handler = &rescan_sighandler;
	sigaction(SIGUSR1, &sig_act, NULL);

	/* Scan CPUs */
	cpus = lub_list_new(cpu_list_compare);
	if (opts->debug)
		fprintf(stdout, "Scanning CPUs...\n");
	scan_cpus(cpus);
	if (opts->debug)
		show_cpus(cpus);

	/* Prepare data structures */
	irqs = lub_list_new(irq_list_compare);

	/* Main loop */
	while (!sigterm) {
		int n;

		if (rescan) {
			if (opts->debug)
				fprintf(stdout, "Scanning hardware...\n");
			rescan = 0;
			irq_list_populate(irqs);
			if (opts->debug)
				irq_list_show(irqs);
		}

		/* Timeout and poll for new devices */
		while ((n = nl_poll(nl_fds, BIRQ_INTERVAL)) != 0) {
			if (-1 == n) {
				fprintf(stderr,
					"Error: Broken NetLink socket.\n");
				goto end;
			}
			if (-2 == n) /* EINTR */
				break;
			if (n > 0) {
				rescan = 1;
				continue;
			}
		}

		if (opts->debug)
			printf("Some balancing...\n");
		parse_proc_stat(cpus);
	}

end:
	/* Free data structures */
	irq_list_free(irqs);
	cpu_list_free(cpus);

	retval = 0;
err:
	/* Close NetLink socket */
	nl_close(nl_fds);

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

/*--------------------------------------------------------- */
/*
 * Signal handler for temination signals (like SIGTERM, SIGINT, ...)
 */
static void sighandler(int signo)
{
	sigterm = 1;
}

/*--------------------------------------------------------- */
/*
 * Force IRQ rescan (SIGUSR1)
 */
static void rescan_sighandler(int signo)
{
	rescan = 1;
}

/*--------------------------------------------------------- */
/* Initialize option structure by defaults */
static struct options *opts_init(void)
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
static void opts_free(struct options *opts)
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
