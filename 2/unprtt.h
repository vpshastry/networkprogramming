#ifndef	__unp_rtt_h
#define	__unp_rtt_h

#include	"unp.h"

struct rtt_info {
  int rtt_rtt;	/* most recent measured RTT, in seconds */
  int rtt_srtt;	/* smoothed RTT estimator, in seconds */
  int rtt_rttvar;	/* smoothed mean deviation, in seconds */
  int rtt_rto;	/* current RTO to use, in seconds */
  int		rtt_nrexmt;	/* # times retransmitted: 0, 1, 2, ... */
  uint32_t	rtt_base;	/* # sec since 1/1/1970 at start */
};

#define	RTT_RXTMIN      1000000	/* 100 ms min retransmit timeout value, in seconds */
#define	RTT_RXTMAX     3000000	/* 300 ms max retransmit timeout value, in seconds */
#define	RTT_MAXNREXMT 	12 /* max # times to retransmit */

				/* function prototypes */
void	 rtt_debug(struct rtt_info *);
void	 rtt_init(struct rtt_info *);
void	 rtt_newpack(struct rtt_info *);
int		 rtt_start(struct rtt_info *);
void	 rtt_stop(struct rtt_info *, uint32_t);
int		 rtt_timeout(struct rtt_info *);
uint32_t rtt_ts(struct rtt_info *);

extern int	rtt_d_flag;	/* can be set to nonzero for addl info */

#endif	/* __unp_rtt_h */
