#include "header.h"
#include	"unprtt.h"
#include	<setjmp.h>

typedef struct {
  char server_ip[100];
  unsigned int portnumber;
  char filename[100000];
  unsigned int recvslidewindowsize; // In # of datagrams
  unsigned int seedvalue;
  float p; // probability
  unsigned int mean; // In milliseconds
} input_t;

typedef struct {
  input_t input;
  int sockfd;
} args_t;

static struct rtt_info   rttinfo;
static int	rttinit = 0;
static sigjmp_buf jmpbuf, jmpbuf2;
static void sig_alrm(int signo);
int awaiting_file_name_ack = 0;
static cli_in_buff_t **global_buffer;
fair_lock_t lock = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0};
static int remaining_size = -1;
int dup_ack_window_update_seq;

int
get_remaining_buffer_size(int buffer_size)
{
  if (remaining_size == -1) {
    printf ("Remaining size not initialized yet.\n");
    return 0;
  }
  if (remaining_size > buffer_size) {
    fprintf (stderr, "ERR: Remaining size(%d) > buffer_size(%d)\n",
              remaining_size, buffer_size);
    return INT_MAX;
  }

  return remaining_size;
}

/* return 0 - drop datagram */
/* return 1 - do not drop */
int simulate_transmission_loss(float p) {
	float num = ((float) rand() / (float)(RAND_MAX)) * 1.0;
	//printf("Random number:%f\n", num);
	if (num <=p) {
		//printf("drop packet.");
		return 0;
	} else {
		//printf("Do not drop this packet");
		return 1;
	}
}

int
append_to_buffer(cli_in_buff_t *recvbuf, int seq, int buffer_size)
{
  cli_in_buff_t *temp;
  int done = 0;

  if (seq < 0) {
    fprintf (stderr, "ERROR: Seq number(%d) < 0.\n", seq);
    exit(0);
  }

  if (!seq) {
    global_buffer = (cli_in_buff_t **) calloc (buffer_size, sizeof(cli_in_buff_t *));
    remaining_size = buffer_size;
  }

  if (!(temp = malloc(sizeof(cli_in_buff_t)))) {
    err_sys("Failed getting new buffer for cli window.\n");
    exit(0);
  }
  memcpy(temp, recvbuf, sizeof(cli_in_buff_t));

  fair_lock(&lock);
  {
    if (global_buffer[seq % buffer_size])
      goto unlock;

    global_buffer[seq % buffer_size] = temp;
    --remaining_size;
    done = 1;
  }
unlock:
  fair_unlock(&lock);

  if (!done)
    free(temp);

  return done;
}

int
receive_file(int sockfd, float p /* prob */, int buffer_size)
{
  int n, fin = 0, time_wait_state = 0;
  send_buffer_t recvbuf, sendbuf;
  cli_in_buff_t in_buff[buffer_size];
  int i = -1;
  int seq = 0;

  Signal(SIGALRM, sig_alrm);
  /* Remove and add proper receiver window logic here */
  if (rttinit == 0) {
    rtt_init(&rttinfo);		/* first time we're called */
    rttinit = 1;
    rtt_d_flag = 1;
  }

  printf ("Waiting for server to send the file.\n");
  while (fin != 1 || time_wait_state == 1) {

    rtt_newpack(&rttinfo);		/* initialize for this packet */

    memset(&recvbuf, 0, sizeof(recvbuf));
    memset(&sendbuf, 0, sizeof(sendbuf));

	printf("memset\n");

	if (sigsetjmp(jmpbuf, 1) != 0) {
		printf("Timer expired and server did not contact me.\n Success.\n");
		time_wait_state = 0;
		return 0;
	}
	printf("waiting for read()\n");
	// These two parts should be together.
    {
      while ((n = Read(sockfd, &recvbuf, sizeof(recvbuf))) < sizeof(seq_header_t))
          if (RTT_DEBUG) fprintf (stderr, "Received data is smaller than header\n");
      // Simulate incoming packet loss.
      if (simulate_transmission_loss(p) == 0) {
        printf ("Dropping packet #%d\n", recvbuf.hdr.seq);
        continue;
      }
	  if (time_wait_state == 1){
		  printf("Server has lost my ACK of FIN, send again.\n");
		  sendbuf.hdr.ack = 1;
		  sendbuf.hdr.seq = seq;
		  sendbuf.hdr.ts = rtt_ts(&rttinfo);
		  sendbuf.hdr.fin = 1;
		  alarm(10);
		  if (simulate_transmission_loss(p) == 0){
			printf("Dropping sending of ACK of FIN %d\n", sendbuf.hdr.seq);
		  } else {
			Write(sockfd, &sendbuf, sizeof(seq_header_t));
		  }
		  continue;
	  }

    }

    printf ("Received packet: #%d\n", recvbuf.hdr.seq);

    if (recvbuf.hdr.seq == seq) {
      if (append_to_buffer(&recvbuf, seq, buffer_size) <= 0) {
        printf ("Client buffer is full. Dropping the packet.\n");
        continue;
      }

      ++seq;

      if (recvbuf.hdr.fin == 1) {
        if (RTT_DEBUG) printf ("Fin received\n");
        sendbuf.hdr.fin = 1;
		printf("Setting fin to 1\n");
		fin = 1;
		time_wait_state = 1;
		alarm(10);
      }
    }

    sendbuf.hdr.ack = 1;
    sendbuf.hdr.seq = seq;
    sendbuf.hdr.ts = rtt_ts(&rttinfo);
	sendbuf.hdr.rwnd = remaining_size;
	if (remaining_size == 0) dup_ack_window_update_seq = seq;

	if(sendbuf.hdr.fin == 1) printf("Sending fin's ACK\n");
    // Simulate ack packet loss.
    if (simulate_transmission_loss(p) == 0){
		printf("Dropping sending of ACK %d\n", sendbuf.hdr.seq);
	} else {
		printf("Sending ACK with SEQ:%d\n", sendbuf.hdr.seq);
		Write(sockfd, &sendbuf, sizeof(seq_header_t));
	}

  }
  return 0;
}

unsigned int
get_time_from_mu(unsigned int mu)
{
  double rnum = ((double) rand() / (double)(RAND_MAX)) * 1.0;
  double log_rnum = log(rnum);
  double floatsleeptime = (-1 * (double) mu * log(rnum));
  unsigned int sleeptime = (unsigned int)floatsleeptime;

  //if (RTT_DEBUG) printf ("Sleeping for: %lf milliseconds\n", floatsleeptime);

  return (unsigned int)floatsleeptime;
}

int
print_from_buf(int buffer_size, unsigned int  mu, int sockfd, float pr)
{
  int seq = 0;
  unsigned int sleep_for;
  cli_in_buff_t *print_buf;
  int is_buf_full = 0;
  send_buffer_t recvbuf;
  printf("print_buf_going into while\n");
  while (42) {
    sleep_for = get_time_from_mu(mu);
    //printf("\nRecv buffer to stdout thread: sleeping for:%u ms or %f seconds\n",
            //sleep_for, (float)sleep_for/1000);
    usleep(sleep_for*1000);// usleep takes microseconds, so multiple by 1000.

    while (42) {

      print_buf = NULL;
      fair_lock(&lock);
      {
        // Handle the case where print buffer can reach consuming before the
        // buffer is actualy filled by the producer. Bcz, producer initializes
        // the buffer at first production.
        if (!global_buffer)
          goto unlockme;

		//printf("global buffer %x, seq:%d, rem size:%d\n", global_buffer[seq % buffer_size], seq, remaining_size);
        if (global_buffer[seq % buffer_size]) {
		  if (remaining_size == 0)
			  is_buf_full = 1;
          print_buf = global_buffer[seq % buffer_size];
          global_buffer[seq % buffer_size] = NULL;
          ++remaining_size;
        }
      }
unlockme:
      fair_unlock(&lock);

      if (!print_buf)
        break;


      print_buf->payload[print_buf->length] = '\0';
      printf ("%s\n", print_buf->payload);

      if (print_buf->hdr.fin == 1) {
        free(print_buf);
        return 0;
      }

      free(print_buf);
      seq = (++seq) % buffer_size;
    }
	if (is_buf_full == 1) {
		is_buf_full = 0;
		recvbuf.hdr.ack = 2;
		recvbuf.hdr.seq = dup_ack_window_update_seq;
		recvbuf.hdr.rwnd = remaining_size;
		if (simulate_transmission_loss(pr) == 0) {
			printf("Dropping sending of buffer_now_available update\n");
		} else {
			Write(sockfd, &recvbuf, sizeof(recvbuf));
		}
	}
  }
}
void *
print_from_buf_fn(void *args)
{
  args_t *largs = args;

  if (print_from_buf(largs->input.recvslidewindowsize, largs->input.mean, largs->sockfd, largs->input.p))
    fprintf (stderr, "Printing from the buffer failed.\n");

  printf ("\nFile printed completely. Exiting the print buffer thread.\n");
  return NULL;
}

int
readargsfromfile(input_t *input)
{
  FILE *fd;
  char line[512], ip[512];
  int i;
  if (!(fd = fopen("client.in", "r"))) {
    err_sys("Opening file failed"); // err_sys prints errno's explaination as well.
    return -1;
  }
  fgets(line, 512, fd);
  sscanf(line, "%s", input->server_ip);
  printf("%s\n", input->server_ip);
  fgets(line, 512, fd);
  sscanf(line, "%u", &input->portnumber);
  printf("%u\n", input->portnumber);
  fgets(line, 512, fd);
  sscanf(line, "%s", input->filename);
  printf("%s\n", input->filename);
  fgets(line, 512, fd);
  sscanf(line, "%u", &input->recvslidewindowsize);
  printf("%u\n", input->recvslidewindowsize);
  fgets(line, 512, fd);
  sscanf(line, "%u", &input->seedvalue);
  printf("%u\n", input->seedvalue);
  fgets(line, 512, fd);
  sscanf(line, "%f", &input->p);
  if (input->p > 1.0 || input->p < 0.0) {
	  err_quit("Probability has to be between 0.0 and 1.0, fix client.in and try again.");
  }
  printf("%f\n", input->p);
  fgets(line, 512, fd);
  sscanf(line, "%lu", &input->mean);
  printf("%lu\n", input->mean);
  return 0;
}

void set_ip_server_and_ip_client(struct sockaddr_in *servaddr, struct sockaddr_in *clientaddr, interface_info_t *ii, size_t interface_info_len, int *is_local) {

	int i;
	struct in_addr client_ip;
	//struct sockaddr_in netmask;
	struct in_addr temp, netmask, longest_prefix;
	char str[INET_ADDRSTRLEN];

	for (i = 0; i < interface_info_len; i++) {
		client_ip = ii[i].ip->sin_addr;
		if (compare_ips(client_ip, servaddr->sin_addr) == 1) {
			printf("Server and Client are on the same host\n");
			inet_pton(AF_INET, "127.0.0.1", &temp);
			servaddr->sin_addr = temp;
			clientaddr->sin_addr = temp;
			return;
		}
	}
	// Not on same host, check if they are on same subnet now.

	inet_pton(AF_INET, "0.0.0.0", &longest_prefix);
	*is_local = 0;
	for (i = 0; i < interface_info_len; i++) {
		client_ip = ii[i].ip->sin_addr;
		netmask = ii[i].netmask->sin_addr;
		if ((servaddr->sin_addr.s_addr & netmask.s_addr) == (client_ip.s_addr & netmask.s_addr)) {
				*is_local = 1;
				if (netmask.s_addr > longest_prefix.s_addr) {
					longest_prefix.s_addr = netmask.s_addr;
					clientaddr->sin_addr = client_ip;
				}
		}
	}
	if (*is_local == 1) {
		printf("Client and Server are on same subnet\n");
	}
	else {
		printf("Client and Server are not local\n");
		clientaddr->sin_addr = ii[i-1].ip->sin_addr; // is this correct. What about loopback, is that valid for clientIP? TODO(@aashray)
	}
}


int
main(int argc, char *argv[]) {
	input_t input = {0,};
	struct sockaddr_in servaddr, clientaddr, client_bind, serv_connect;
	interface_info_t ii[MAX_INTERFACE_INFO] = {0,};
	size_t interface_info_len;
	char str[INET_ADDRSTRLEN + 1], recvline[MAXLINE + 1];
	int n, is_local, sockfd, serv_sock_fd, temp_port;
	const int do_not_route = 1, on = 1;

	socklen_t len = sizeof(clientaddr);
	readargsfromfile(&input);
	// srand here. Once per program run.
	srand(input.seedvalue);

	bzero(&servaddr, sizeof(servaddr));
	bzero(&clientaddr, sizeof(clientaddr));
	build_inferface_info(ii, &interface_info_len, 0, -1);
	print_interface_info(ii, interface_info_len);

	if(inet_pton(AF_INET, input.server_ip, &servaddr.sin_addr) != 1){
		err_quit("client.in does not cotain a valid server IP. Please correct and try again.");
	}

	set_ip_server_and_ip_client(&servaddr, &clientaddr, ii, interface_info_len, &is_local);

	printf("IPclient:%s\n", Inet_ntop(AF_INET, &clientaddr.sin_addr, str, INET_ADDRSTRLEN));
	printf("IPserver:%s\n", Inet_ntop(AF_INET, &servaddr.sin_addr, str, INET_ADDRSTRLEN));

	sockfd = Socket(AF_INET, SOCK_DGRAM, 0);
	if (is_local == 1)
		Setsockopt(sockfd, SOL_SOCKET, SO_DONTROUTE, &do_not_route, sizeof(do_not_route));
	Setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	clientaddr.sin_family = AF_INET;
	clientaddr.sin_port = htons(0);

	Bind(sockfd, (SA *) &clientaddr, sizeof(clientaddr));
    len = sizeof(clientaddr);
	Getsockname(sockfd, (SA*) &clientaddr, &len);
	printf("\nIPclient bound, protocol Address:%s\n", Sock_ntop((SA*) &clientaddr, len));

	// Connect to IPserver.
	//serv_sock_fd = Socket(AF_INET, SOCK_DGRAM, 0);
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(input.portnumber);
	Connect(sockfd, (SA*) &servaddr, sizeof(servaddr));

	// Getpeername
	len = sizeof(serv_connect);
	Getpeername(sockfd, (SA*) &serv_connect, &len);
	printf("\nIPserver connected, protocol Address:%s\n", Sock_ntop((SA*) &serv_connect, len));
	len = sizeof(clientaddr);
	printf("\nIPclient connected, protocol Address:%s\n", Sock_ntop((SA*) &clientaddr, len));
	// Send the filename
	Signal(SIGALRM, sig_alrm);

	if (simulate_transmission_loss(input.p) == 0) {
		printf("Dropping datagram sending filename to server\n");
	} else {
		printf("Filename sent\n");
		Write(sockfd, input.filename, strlen(input.filename));
	}

	awaiting_file_name_ack = 1;
	alarm(5);

	if (sigsetjmp(jmpbuf2, 1) != 0) {
		printf("Server may have not recieved the file-name. Retransmit.\n");
		if (simulate_transmission_loss(input.p) != 0)
			Write(sockfd, input.filename, strlen(input.filename));
		alarm(5);
	}
        send_buffer_t buf;
	n = Read(sockfd, &buf, sizeof(buf));
	alarm(0); // Got response from server, switch off timer.
    awaiting_file_name_ack = 0;
	Fputs(buf.payload, stdout);
	//bzero(&servaddr, sizeof(serv_connect));
	//Sock_pton(recvline, &servaddr);
	sscanf(buf.payload, "%d", &temp_port);
	servaddr.sin_port = htons(temp_port);
	Connect(sockfd, (SA*) &servaddr, sizeof(servaddr));
	bzero(&serv_connect, sizeof(serv_connect));
	len = sizeof(serv_connect);
	Getpeername(sockfd, (SA*) &serv_connect, &len);
	printf("\nIPserver connected, protocol Address:%s\n", Sock_ntop((SA*) &serv_connect, len));

	// Send ACK to server on new port to continue file transfer.
        if (simulate_transmission_loss(input.p))
          Write(sockfd, "ACK!!\0", strlen("ACK!!\0"));
        else
          printf ("Dropped ack for port number.\n");

        do {
          memset(&buf, 0, sizeof(buf));
          n = Read(sockfd, &buf, sizeof(buf));
          if (simulate_transmission_loss(input.p))
            Write(sockfd, "ACK!!\0", strlen("ACK!!\0"));
          else
            printf ("Dropped ack for port number.\n");
        } while (buf.hdr.sft == 0);
        printf ("All these control transfer is done. Moving to file transfer.\n");

        // pthread code starts here.
        pthread_t thread_print_from_buf;
        args_t *args = calloc(1, sizeof(args_t));

        memcpy (&args->input, &input, sizeof(args_t));
		args->sockfd = sockfd;

        if (pthread_create(&thread_print_from_buf, NULL, &print_from_buf_fn, args)) {
          fprintf (stderr, "Couldn't create thread for print from buffer.\n");
          return -1;
        }

        if (receive_file(sockfd, input.p, input.recvslidewindowsize))
          fprintf (stderr, "Receive file exited with error.\n");

        if (pthread_join(thread_print_from_buf, NULL))
          fprintf (stderr, "pthread join failed\n");
        printf ("Finished with all the jobs. Exiting the client.\n");

        free(args);

	return 0;
}

static void
sig_alrm(int signo)
{
		if (awaiting_file_name_ack == 1) siglongjmp(jmpbuf2, 1);
		else siglongjmp(jmpbuf, 1);
}
