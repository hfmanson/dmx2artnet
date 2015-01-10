#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#ifdef SC12
#include <clib.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>  /* sockaddr_in{} and other Internet defns */
#include <arpa/inet.h>   /* inet(3) functions */
#endif

#ifndef EMULATE
#ifdef FTDI
#include <ftdi.h>
#endif // FTDI
#endif // EMULATE

#define SA      struct sockaddr
#define MAXLINE 1024

#define DMX_CHANNELS 512

typedef unsigned char dmx_t;
dmx_t                 dmx[DMX_CHANNELS + 1];

#ifndef EMULATE

#ifdef __linux
static void daemonize(void)
{
    pid_t pid, sid;

    /* already a daemon */
    if ( getppid() == 1 ) return;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then we can exit the parent process. */
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    /* At this point we are executing as the child process */

    /* Change the file mode mask */
    umask(0);

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    /* Change the current working directory.  This prevents the current
       directory from being locked; hence not being able to remove it. */
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }

    /* Redirect standard files to /dev/null */
    freopen( "/dev/null", "r", stdin);
    freopen( "/dev/null", "w", stdout);
    freopen( "/dev/null", "w", stderr);
}
#endif // __linux

#define DMX_MAB 160    // Mark After Break 8 uS or more
#define DMX_BREAK 110  // Break 88 uS or more

#ifdef FTDI

struct ftdi_context ftdic;

static int do_dmx_break(struct ftdi_context* ftdic)
{
    int ret;
    
	if ((ret = ftdi_set_line_property2(ftdic, BITS_8, STOP_BIT_2, NONE, BREAK_ON)) < 0)
	{
        fprintf(stderr, "unable to set BREAK ON: %d (%s)\n", ret, ftdi_get_error_string(ftdic));
        return EXIT_FAILURE;
	}
    usleep(DMX_BREAK);
	if ((ret = ftdi_set_line_property2(ftdic, BITS_8, STOP_BIT_2, NONE, BREAK_OFF)) < 0)
	{
        fprintf(stderr, "unable to set BREAK OFF: %d (%s)\n", ret, ftdi_get_error_string(ftdic));
        return EXIT_FAILURE;
	}
    usleep(DMX_MAB);
	return EXIT_SUCCESS;
}

static int dmx_init(struct ftdi_context* ftdic)
{
    int ret;

    if (ftdi_init(ftdic) < 0)
    {
        fprintf(stderr, "ftdi_init failed\n");
        return EXIT_FAILURE;
    }

    if ((ret = ftdi_usb_open(ftdic, 0x0403, 0x6001)) < 0)
    {
        fprintf(stderr, "unable to open ftdi device: %d (%s)\n", ret, ftdi_get_error_string(ftdic));
        return EXIT_FAILURE;
    }

    if ((ret = ftdi_set_baudrate(ftdic, 250000)) < 0)
	{
        fprintf(stderr, "unable to set baudrate: %d (%s)\n", ret, ftdi_get_error_string(ftdic));
        return EXIT_FAILURE;
	}

	if ((ret = ftdi_set_line_property(ftdic, BITS_8, STOP_BIT_2, NONE)) < 0)
	{
        fprintf(stderr, "unable to set line property: %d (%s)\n", ret, ftdi_get_error_string(ftdic));
        return EXIT_FAILURE;
	}

    if ((ret = ftdi_setflowctrl(ftdic, SIO_DISABLE_FLOW_CTRL)) < 0)
	{
        fprintf(stderr, "unable to set flow control: %d (%s)\n", ret, ftdi_get_error_string(ftdic));
        return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

static int dmx_write(struct ftdi_context* ftdic, dmx_t* dmx, size_t size)
{
    int ret;

	if ((ret = do_dmx_break(ftdic)) == EXIT_SUCCESS)
	{
    	if ((ret = ftdi_write_data(ftdic, dmx, size)) < 0)
    	{
        	fprintf(stderr, "unable to write data: %d (%s)\n", ret, ftdi_get_error_string(ftdic));
        	ret = EXIT_FAILURE;
    	}
	}
	return ret;
}

void alarm_wakeup(int i)
{
	dmx_write(&ftdic, dmx, sizeof(dmx));
}

void set_timer(void)
{
	struct itimerval tout_val;

	signal(SIGALRM, alarm_wakeup);
	tout_val.it_interval.tv_sec = 0;
	tout_val.it_interval.tv_usec = 100000;
	tout_val.it_value.tv_sec = 0;
	tout_val.it_value.tv_usec = 100000;
	setitimer(ITIMER_REAL, &tout_val, 0);
}
#endif // FTDI

#ifdef SC12
//int port = FOSSIL_EXT;
int port = FOSSIL_COM;

static int do_dmx_break(int port)
{
    int ret;
    
	fossil_send_break(port, FOS_EXTRALONG_BREAK);
	return EXIT_SUCCESS;
}

static int dmx_init(int port, long baudrate, int parity, int wordlen, int stopbits)
{
    if (fossil_init(port) != 0x1954)
    {
        fprintf(stderr, "fossil_init failed\n");
        return EXIT_FAILURE;
    }

	if (fossil_setbaud(port, baudrate, parity, wordlen, stopbits) == -1)
	{
        fprintf(stderr, "unable to set baudrate\n");
        return EXIT_FAILURE;
	}

    fossil_set_flowcontrol(port, FOSSIL_FLOWCTRL_OFF);
	printf("dmx_init(%d, %ld, %d, %d, %d) succeeded\n", port, baudrate, parity, wordlen, stopbits);
	return EXIT_SUCCESS;
}

static int dmx_write(int port, dmx_t *dmx, size_t size)
{
    int ret;

	if ((ret = do_dmx_break(port)) == EXIT_SUCCESS)
	{
		int j;

//		BIOS_putch('*');
		RTX_Sleep_Time(1);
    	for( j = 0; j < size; )
    	{
    		j += fossil_writeblock(port, ((unsigned char far *)dmx) + j, size - j);
//			printf("j = %d\n", j);
    	}
	}
	return EXIT_SUCCESS;
}
#endif // SC12

#endif // EMULATE

void print_dmx(int size)
{
	int i;

	for (i = 0; i <= size; i++)
	{
		printf("%4d", dmx[i]);
	}
	printf("\n");
}

int dg_echo(int sockfd, SA *pcliaddr, socklen_t clilen)
{
    int ret;

#ifndef EMULATE
#ifdef FTDI
	if ((ret = dmx_init(&ftdic)) == EXIT_SUCCESS)
#endif // FTDI
#ifdef SC12
	if ((ret = dmx_init(port, 250000L, FOSSIL_PARITY_NO, 8, 2)) == EXIT_SUCCESS)
//	if ((ret = dmx_init(port, 19200L, FOSSIL_PARITY_NO, 8, 1)) == EXIT_SUCCESS)
#endif // SC12
#endif // EMULATE
	{
		int                     n, m;
		int             channel;
		int             nchars;
		socklen_t       len;
		unsigned char   mesg[MAXLINE];
		int             i;
#ifdef SC12
		int error;
#endif

		bzero(dmx, sizeof(dmx));
#ifndef EMULATE
#ifndef SC12
		set_timer();
#endif
#endif
		for ( ; ; ) {
#ifdef SC12
			n = recvfrom(sockfd, mesg, MAXLINE, MSG_BLOCKING, 0L, pcliaddr, &error);
			if (n >= 20) // minimum required ArtDMX size
			{
#else
			len = clilen;
			n = recvfrom(sockfd, mesg, MAXLINE, 0, pcliaddr, &len);
#endif
			if (strcmp(mesg, "Art-Net") == 0)
			{
				int opcode = mesg[8] | mesg[9] << 8; 
				if (opcode == 0x5000) // OpDmx
				{
					int length = mesg[16] << 8 | (mesg[17] & 0xFE); // force even
					if (length <= 512)
					{
						bcopy(mesg + 18, dmx + 1, length);
						dmx_write(port, dmx, length + 1);
						fossil_flush_output(port);
					}
				}
			}
#ifdef SC12
			}
#endif
		}
	}
	return ret;
}

int loop()
{
	int					sockfd;
	struct sockaddr_in	servaddr, cliaddr;
#ifdef SC12
	int error;
#endif

#ifdef SC12
	sockfd = opensocket(SOCK_DGRAM, &error);
#else
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
#endif
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(6454);

#ifdef SC12
	bind(sockfd, (const struct sockaddr far *) &servaddr, &error);
#else
	bind(sockfd, (SA *) &servaddr, sizeof(servaddr));
#endif

	return dg_echo(sockfd, (SA *) &cliaddr, sizeof(cliaddr));
}

int main(int argc, char **argv)
{
#ifndef EMULATE
#ifdef __linux
	daemonize();
#endif // __linux
#endif // EMULATE
	return loop();

}

