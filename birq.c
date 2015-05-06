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
#include <time.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include "birq.h"
#include "lub/log.h"
#include "lub/list.h"
#include "irq.h"
#include "numa.h"
#include "cpu.h"
#include "statistics.h"
#include "balance.h"
#include "pxm.h"

#ifndef VERSION
#define VERSION "1.0.0"
#endif

/* Signal handlers */
static volatile int sigterm = 0; /* Exit if 1 */
static void sighandler(int signo);

static void help(int status, const char *argv0);
static struct options *opts_init(void);
static void opts_free(struct options *opts);
static int opts_parse(int argc, char *argv[], struct options *opts);

/* Command line options */
struct options {
	char *pidfile;
	char *pxm; /* Proximity config file */
	int debug; /* Don't daemonize in debug mode */
	int log_facility;
	float threshold;
	int verbose;
	int ht;
	unsigned int long_interval;
	unsigned int short_interval;
	birq_choose_strategy_e strategy;
};

/*--------------------------------------------------------- */
int main(int argc, char **argv)
{
	int retval = -1;
	struct options *opts = NULL;
	int pidfd = -1;
	unsigned int interval;

	/* Signal vars */
	struct sigaction sig_act;
	sigset_t sig_set;

	/* IRQ list. It contain all found IRQs. */
	lub_list_t *irqs;
	/* IRQs need to be balanced */
	lub_list_t *balance_irqs;
	/* CPU list. It contain all found CPUs. */
	lub_list_t *cpus;
	/* NUMA list. It contain all found NUMA nodes. */
	lub_list_t *numas;
	/* Proximity list. */
	lub_list_t *pxms;

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
			str[sizeof(str) - 1] = '\0';
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

	/* Randomize */
	srand(time(NULL));

	/* Scan NUMA nodes */
	numas = lub_list_new(numa_list_compare);
	scan_numas(numas);
	if (opts->verbose)
		show_numas(numas);

	/* Scan CPUs */
	cpus = lub_list_new(cpu_list_compare);
	scan_cpus(cpus, opts->ht);
	if (opts->verbose)
		show_cpus(cpus);

	/* Prepare data structures */
	irqs = lub_list_new(irq_list_compare);
	balance_irqs = lub_list_new(irq_list_compare);

	/* Parse proximity file */
	pxms = lub_list_new(NULL);
	if (opts->pxm)
		parse_pxm_config(opts->pxm, pxms, numas);
	if (opts->verbose)
		show_pxms(pxms);

	/* Main loop */
	while (!sigterm) {
		lub_list_node_t *node;
		char outstr[10];
		time_t t;
		struct tm *tmp;

		t = time(NULL);
		tmp = localtime(&t);
		if (tmp) {
			strftime(outstr, sizeof(outstr), "%H:%M:%S", tmp);
			printf("----[ %s ]----------------------------------------------------------------\n", outstr);
		}

		/* Rescan PCI devices for new IRQs. */
		scan_irqs(irqs, balance_irqs, pxms);
		if (opts->verbose)
			irq_list_show(irqs);
		/* Link IRQs to CPUs due to real current smp affinity. */
		link_irqs_to_cpus(cpus, irqs);

		/* Gather statistics on CPU load and number of interrupts. */
		gather_statistics(cpus, irqs);
		show_statistics(cpus, opts->verbose);
		/* Choose IRQ to move to another CPU. */
		choose_irqs_to_move(cpus, balance_irqs,
			opts->threshold, opts->strategy);

		/* If nothing to balance */
		if (lub_list_len(balance_irqs) != 0) {
			/* Set short interval to make balancing faster. */
			interval = opts->short_interval;
			/* Choose new CPU for IRQs need to be balanced. */
			balance(cpus, balance_irqs, opts->threshold);
			/* Write new values to /proc/irq/<IRQ>/smp_affinity */
			apply_affinity(balance_irqs);
			/* Free list of balanced IRQs */
			while ((node = lub_list__get_tail(balance_irqs))) {
				lub_list_del(balance_irqs, node);
				lub_list_node_free(node);
			}
		} else {
			/* If nothing to balance */
			interval = opts->long_interval;
		}
		
		/* Wait before next iteration */
		sleep(interval);
	}

	/* Free data structures */
	irq_list_free(irqs);
	lub_list_free(balance_irqs);
	cpu_list_free(cpus);
	numa_list_free(numas);
	pxm_list_free(pxms);

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
	signo = signo; /* Happy compiler */
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
	opts->pxm = NULL;
	opts->log_facility = LOG_DAEMON;
	opts->threshold = BIRQ_DEFAULT_THRESHOLD;
	opts->verbose = 0;
	opts->ht = 0;
	opts->long_interval = BIRQ_LONG_INTERVAL;
	opts->short_interval = BIRQ_SHORT_INTERVAL;
	opts->strategy = BIRQ_CHOOSE_RND;

	return opts;
}

/*--------------------------------------------------------- */
/* Free option structure */
static void opts_free(struct options *opts)
{
	if (opts->pidfile)
		free(opts->pidfile);
	if (opts->pxm)
		free(opts->pxm);
	free(opts);
}

/*--------------------------------------------------------- */
/* Parse command line options */
static int opts_parse(int argc, char *argv[], struct options *opts)
{
	static const char *shortopts = "hp:dO:t:vri:I:s:x:";
#ifdef HAVE_GETOPT_H
	static const struct option longopts[] = {
		{"help",		0, NULL, 'h'},
		{"pid",			1, NULL, 'p'},
		{"debug",		0, NULL, 'd'},
		{"facility",		1, NULL, 'O'},
		{"threshold",		1, NULL, 't'},
		{"verbose",		0, NULL, 'v'},
		{"ht",			0, NULL, 'r'},
		{"short-interval",	1, NULL, 'i'},
		{"long-interval",	1, NULL, 'i'},
		{"strategy",		1, NULL, 's'},
		{"pxm",			1, NULL, 'x'},
		{NULL,			0, NULL, 0}
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
		case 'x':
			if (opts->pxm)
				free(opts->pxm);
			opts->pxm = strdup(optarg);
			break;
		case 'd':
			opts->debug = 1;
			break;
		case 'v':
			opts->verbose = 1;
			break;
		case 'r':
			opts->ht = 1;
			break;
		case 'O':
			if (lub_log_facility(optarg, &(opts->log_facility))) {
				fprintf(stderr, "Error: Illegal syslog facility %s.\n", optarg);
				help(-1, argv[0]);
				exit(-1);
			}
			break;
		case 't':
			{
			char *endptr;
			float thresh;
			thresh = strtof(optarg, &endptr);
			if (endptr == optarg)
				thresh = opts->threshold;
			opts->threshold = thresh;
			if (thresh > 100.00) {
				fprintf(stderr, "Error: Illegal threshold value %s.\n", optarg);
				help(-1, argv[0]);
				exit(-1);
			}
			}
			break;
		case 'i':
			{
			char *endptr;
			unsigned long int val;
			val = strtoul(optarg, &endptr, 10);
			if (endptr != optarg)
				opts->short_interval = val;
			}
			break;
		case 'I':
			{
			char *endptr;
			unsigned long int val;
			val = strtoul(optarg, &endptr, 10);
			if (endptr != optarg)
				opts->long_interval = val;
			}
			break;
		case 's':
			if (!strcmp(optarg, "max"))
				opts->strategy = BIRQ_CHOOSE_MAX;
			else if (!strcmp(optarg, "min"))
				opts->strategy = BIRQ_CHOOSE_MIN;
			else if (!strcmp(optarg, "rnd"))
				opts->strategy = BIRQ_CHOOSE_RND;
			else {
				fprintf(stderr, "Error: Illegal strategy value %s.\n", optarg);
				help(-1, argv[0]);
				exit(-1);
			}
			break;
		case 'h':
			help(0, argv[0]);
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
		printf("Version : %s\n", VERSION);
		printf("Usage   : %s [options]\n", name);
		printf("Daemon to balance IRQs.\n");
		printf("Options :\n");
		printf("\t-h, --help\tPrint this help.\n");
		printf("\t-d, --debug\tDebug mode. Don't daemonize.\n");
		printf("\t-v, --verbose\tBe verbose.\n");
		printf("\t-r, --ht\tEnable hyper-threading. Not recommended.\n");
		printf("\t-p <path>, --pid=<path>\tFile to save daemon's PID to.\n");
		printf("\t-x <path>, --pxm=<path>\tProximity config file.\n");
		printf("\t-O, --facility\tSyslog facility. Default is DAEMON.\n");
		printf("\t-t <float>, --threshold=<float>\tThreshold to consider CPU is overloaded, in percents.\n");
		printf("\t-i <sec>, --short-interval=<sec>\tShort iteration interval.\n");
		printf("\t-I <sec>, --long-interval=<sec>\tLong iteration interval.\n");
		printf("\t-s <strategy>, --strategy=<strategy>\tStrategy to choose IRQ to move (min/max/rnd).\n");
	}
}
