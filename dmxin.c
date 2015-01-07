#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <signal.h>
#include <clib.h>

#define SA      struct sockaddr
#define MAXLINE 1024

#define DMX_CHANNELS 512

typedef unsigned char dmx_t;
dmx_t                 dmx[DMX_CHANNELS + 1];

#define DMX_MAB 160    // Mark After Break 8 uS or more
#define DMX_BREAK 110  // Break 88 uS or more

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

void print_dmx(int size)
{
	int i;

	for (i = 0; i <= size; i++)
	{
		printf("%4d", dmx[i]);
	}
	printf("\n");
}

unsigned char artnet[600] = "Art-Net\x00\x00\x50\x00\x0e\x00\x00\x00\x00\x00";
#define DATA_OFFSET 18

unsigned char *dmxdata = artnet + DATA_OFFSET;
// skip byte containing 0x00 with frame error en DMX type byte 0x00
unsigned char *dmxdata2 = artnet + DATA_OFFSET - 2;


int loop1(int argc, char *argv[])
{
	int                 ret;
	struct sockaddr_in	servaddr;
	int					sockfd;
	char                buffer[512];
	int                 error;
	
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(atoi(argv[2]));
	inet_addr(argv[1], (unsigned long far *) &servaddr.sin_addr);

	sockfd = opensocket(SOCK_DGRAM, &error);

	if ((ret = dmx_init(port, 250000L, FOSSIL_PARITY_NO, 8, 2)) == EXIT_SUCCESS)
	{
#ifdef DIAG
		int count;
		while (1)
		{
			artnet[0] = fossil_status_request(port);
			count = fossil_readblock(port, artnet + 2, 512);
			artnet[1] = fossil_status_request(port);
			sendto(sockfd, artnet, count + 2, 0, (const struct sockaddr *) &servaddr, &error);
		}
#else
		int status1;
		int status2;
		int status4;
		int count;
		while (1)
		{
			status1 = fossil_status_request(port);
			count = fossil_readblock(port, dmxdata2, 512);
			status2 = fossil_status_request(port);
			if ((status1 & FOSSIL_LINE_BREAK) && !(status2 & (FOSSIL_LINE_BREAK | FOSSIL_FRAMING_ERROR)))
			{
				if (status2 & FOSSIL_DATA_AVAILABLE)
				{
					do
					{
						count += fossil_readblock(port, dmxdata2 + count, 512 - count);
						status4 = fossil_status_request(port);
					} while (status4 & FOSSIL_DATA_AVAILABLE && !(status4 & (FOSSIL_LINE_BREAK | FOSSIL_FRAMING_ERROR)));
					
					if (!(status4 & (FOSSIL_LINE_BREAK | FOSSIL_FRAMING_ERROR)))
					{
						dmxdata2[0] = (count - 2) >> 8;
						dmxdata2[1] = (count - 2);
						sendto(sockfd, artnet, DATA_OFFSET + count - 2, 0, (const struct sockaddr *) &servaddr, &error);
					}
				}
				else
				{
					dmxdata2[0] = (count - 2) >> 8;
					dmxdata2[1] = (count - 2);
					sendto(sockfd, artnet, DATA_OFFSET + count - 2, 0, (const struct sockaddr *) &servaddr, &error);
				}
			}
		}
#endif
	}
	else
	{
		printf("loop1: dmx_init failed\n");
	}
	return 0;
}

int main(int argc, char **argv)
{
	return loop1(argc, argv);
}

