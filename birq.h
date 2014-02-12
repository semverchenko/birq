#ifndef _birq_h
#define _birq_h

#define BIRQ_PIDFILE "/var/run/birq.pid"

/* Interval beetween balance iterations, in seconds.
   The long interval is used when there are no overloaded CPUs.
   Else the short interval is used. */
#define BIRQ_LONG_INTERVAL 5
#define BIRQ_SHORT_INTERVAL 2

/* Threshold to consider CPU as overloaded.
   In percents, float value. Can't be greater than 100.0 */
#define BIRQ_DEFAULT_THRESHOLD 99.0

#endif
