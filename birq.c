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

#include "log.h"

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
int daemonize(int nochdir, int noclose);
struct options *opts_init(void);
void opts_free(struct options *opts);
static int opts_parse(int argc, char *argv[], struct options *opts);

/* Command line options */
struct options {
	char *pidfile;
	char *chroot;
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
	struct sigaction sig_act, sigpipe_act;
	sigset_t sig_set, sigpipe_set;

	/* Parse command line options */
	opts = opts_init();
	if (opts_parse(argc, argv, opts))
		goto err;

	/* Initialize syslog */
	openlog(argv[0], LOG_CONS, opts->log_facility);
	syslog(LOG_ERR, "Start daemon.\n");

	/* Fork the daemon */
	if (!opts->debug) {
		/* Daemonize */
		if (daemonize(0, 0) < 0) {
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

#ifdef HAVE_CHROOT
	/* Chroot */
	if (opts->chroot) {
		if (chroot(opts->chroot) < 0) {
			syslog(LOG_ERR, "Can't chroot to %s: %s",
				opts->chroot, strerror(errno));
			goto err;
		}
	}
#endif

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

	/* Ignore SIGPIPE */
	sigemptyset(&sigpipe_set);
	sigaddset(&sigpipe_set, SIGPIPE);
	sigpipe_act.sa_flags = 0;
	sigpipe_act.sa_mask = sigpipe_set;
	sigpipe_act.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sigpipe_act, NULL);




	retval = 0;
err:
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
/* Implement own simple daemon() to don't use Non-POSIX */
int daemonize(int nochdir, int noclose)
{
	int fd;
	int pid;

	pid = fork();
	if (-1 == pid)
		return -1;
	if (pid > 0)
		_exit(0); /* Exit parent */
	if (setsid() == -1)
		return -1;
	if (!nochdir) {
		if (chdir("/"))
			return -1;
	}
	if (!noclose) {
		fd = open("/dev/null", O_RDWR, 0);
		if (fd < 0)
			return -1;
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		if (fd > 2)
			close(fd);
	}

	return 0;
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
	opts->chroot = NULL;
	opts->log_facility = LOG_DAEMON;

	return opts;
}

/*--------------------------------------------------------- */
/* Free option structure */
void opts_free(struct options *opts)
{
	if (opts->pidfile)
		free(opts->pidfile);
	if (opts->chroot)
		free(opts->chroot);
	free(opts);
}

/*--------------------------------------------------------- */
/* Parse command line options */
static int opts_parse(int argc, char *argv[], struct options *opts)
{
	static const char *shortopts = "hvp:dr:O:";
#ifdef HAVE_GETOPT_H
	static const struct option longopts[] = {
		{"help",	0, NULL, 'h'},
		{"version",	0, NULL, 'v'},
		{"pid",		1, NULL, 'p'},
		{"debug",	0, NULL, 'd'},
		{"chroot",	1, NULL, 'r'},
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
		case 'r':
#ifdef HAVE_CHROOT
			if (opts->chroot)
				free(opts->chroot);
			opts->chroot = strdup(optarg);
#else
			fprintf(stderr, "Error: The --chroot option is not supported.\n");
			return -1;
#endif
			break;
		case 'd':
			opts->debug = 1;
			break;
		case 'O':
			if (parse_log_facility(optarg, &(opts->log_facility))) {
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
		printf("\t-r <path>, --chroot=<path>\tDirectory to chroot.\n");
		printf("\t-O, --facility\tSyslog facility. Default is DAEMON.\n");
	}
}
