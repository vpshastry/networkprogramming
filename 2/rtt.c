/* include rtt1 */
#include	"unprtt.h"

int		rtt_d_flag = 1;		/* debug flag; can be set by caller */

/*
 * Calculate the RTO value based on current estimators:
 *		smoothed RTT plus four times the deviation
 */
#define	RTT_RTOCALC(ptr) ((ptr)->rtt_srtt + (4 * (ptr)->rtt_rttvar))

static long
rtt_minmax(long rto)
{
	if (rto < RTT_RXTMIN)
		rto = RTT_RXTMIN;
	else if (rto > RTT_RXTMAX)
		rto = RTT_RXTMAX;
	return(rto);
}

void
rtt_init(struct rtt_info *ptr)
{
	struct timeval	tv;

	Gettimeofday(&tv, NULL);
	ptr->rtt_base = tv.tv_sec;		/* # sec since 1/1/1970 at start */

	ptr->rtt_rtt    = 0;
	ptr->rtt_srtt   = 0;
	ptr->rtt_rttvar = 750 /*0.75 sec = 750 ms*/;
	ptr->rtt_rto = rtt_minmax(RTT_RTOCALC(ptr));
		/* first RTO at (srtt + (4 * rttvar)) = 3 seconds */
}

/*
 * Return the current timestamp.
 * Our timestamps are 32-bit integers that count milliseconds since
 * rtt_init() was called.
 */
uint32_t
rtt_ts(struct rtt_info *ptr)
{
	uint32_t		ts;
	struct timeval	tv;
	printf("rtt_init\n");
	Gettimeofday(&tv, NULL);
	ts = ((tv.tv_sec - ptr->rtt_base) * 1000) + (tv.tv_usec / 1000);
	return(ts);
}

void
rtt_newpack(struct rtt_info *ptr)
{
	ptr->rtt_nrexmt = 0;
	printf("rtt_newpack\n");
}

int
rtt_start(struct rtt_info *ptr)
{
	printf("timer started to value:%u\n", (ptr->rtt_rto + 500) /1000);
	return  ((ptr->rtt_rto + 500/*0.5 sec = 500 ms*/)/1000);		/* round float to int */
		/* 4return value can be used as: alarm(rtt_start(&foo)) */
}

/*
 * A response was received.
 * Stop the timer and update the appropriate values in the structure
 * based on this packet's RTT.  We calculate the RTT, then update the
 * estimators of the RTT and its mean deviation.
 * This function should be called right after turning off the
 * timer with alarm(0), or right after a timeout occurs.
 */
void
rtt_stop(struct rtt_info *ptr, uint32_t ms)
{
	int		delta;

	ptr->rtt_rtt = ms;		/* measured RTT in seconds */

	/*
	 * Update our estimators of RTT and mean deviation of RTT.
	 * See Jacobson's SIGCOMM '88 paper, Appendix A, for the details.
	 * We use floating point here for simplicity.
	 */

	delta = ptr->rtt_rtt - ptr->rtt_srtt;
	ptr->rtt_srtt += delta / 8;		/* g = 1/8 */

	if (delta < 0) delta = -delta;				/* |delta| */

	ptr->rtt_rttvar += (delta - ptr->rtt_rttvar) / 4;	/* h = 1/4 */

	ptr->rtt_rto = rtt_minmax(RTT_RTOCALC(ptr));
}

/*
 * A timeout has occurred.
 * Return -1 if it's time to give up, else return 0.
 */
int
rtt_timeout(struct rtt_info *ptr)
{
	ptr->rtt_rto *= 2;		/* next RTO */
        ptr->rtt_rto = rtt_minmax(ptr->rtt_rto);

	printf("******rrt_nrexmt = %d\n", ptr->rtt_nrexmt);
	if (++ptr->rtt_nrexmt > RTT_MAXNREXMT)
		return -1;			/* time to give up for this packet */

	return 0;
}

/*
 * Print debugging information on stderr, if the "rtt_d_flag" is nonzero.
 */

void
rtt_debug(struct rtt_info *ptr)
{
	if (rtt_d_flag == 0)
		return;

	fprintf(stderr, "rtt = %7lu, srtt = %5lu, rttvar = %5ld, rto = %3lu\n",
			ptr->rtt_rtt, ptr->rtt_srtt, ptr->rtt_rttvar, ptr->rtt_rto);
	fflush(stderr);
}
