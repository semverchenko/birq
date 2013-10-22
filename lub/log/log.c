#include "lub/log.h"
#include <syslog.h>
#include <strings.h>

int lub_log_facility(const char *str, int *facility)
{
	if (!strcasecmp(str, "local0"))
		*facility = LOG_LOCAL0;
	else if (!strcasecmp(str, "local1"))
		*facility = LOG_LOCAL1;
	else if (!strcasecmp(str, "local2"))
		*facility = LOG_LOCAL2;
	else if (!strcasecmp(str, "local3"))
		*facility = LOG_LOCAL3;
	else if (!strcasecmp(str, "local4"))
		*facility = LOG_LOCAL4;
	else if (!strcasecmp(str, "local5"))
		*facility = LOG_LOCAL5;
	else if (!strcasecmp(str, "local6"))
		*facility = LOG_LOCAL6;
	else if (!strcasecmp(str, "local7"))
		*facility = LOG_LOCAL7;
	else if (!strcasecmp(str, "auth"))
		*facility = LOG_AUTH;
	else if (!strcasecmp(str, "authpriv"))
		*facility = LOG_AUTHPRIV;
	else if (!strcasecmp(str, "cron"))
		*facility = LOG_CRON;
	else if (!strcasecmp(str, "daemon"))
		*facility = LOG_DAEMON;
	else if (!strcasecmp(str, "ftp"))
		*facility = LOG_FTP;
	else if (!strcasecmp(str, "kern"))
		*facility = LOG_KERN;
	else if (!strcasecmp(str, "lpr"))
		*facility = LOG_LPR;
	else if (!strcasecmp(str, "mail"))
		*facility = LOG_MAIL;
	else if (!strcasecmp(str, "news"))
		*facility = LOG_NEWS;
	else if (!strcasecmp(str, "syslog"))
		*facility = LOG_SYSLOG;
	else if (!strcasecmp(str, "user"))
		*facility = LOG_USER;
	else if (!strcasecmp(str, "uucp"))
		*facility = LOG_UUCP;
	else
		return -1;

	return 0;
}
