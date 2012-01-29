#include <stdlib.h>
#include <xetypes.h>

#include <lwip/sockets.h>

#include <network/network.h>
#include "config.h"

extern void launch_elf(void * addr, unsigned len);

#define TFTP_PORT	69
/* opcode */
#define TFTP_RRQ			1 	/* read request */
#define TFTP_WRQ			2	/* write request */
#define TFTP_DATA			3	/* data */
#define TFTP_ACK			4	/* ACK */
#define TFTP_ERROR			5	/* error */

char tftp_buffer[512 + 4];
int do_tftp(void * buffer, int maxlen, const char* host, const char* filename)
{
	int loc = 0;
	int sock_fd, sock_opt;
	struct sockaddr_in tftp_addr, from_addr;
	unsigned int length;
	socklen_t fromlen;

	/* connect to tftp server */
	inet_aton(host, (struct in_addr*)&(tftp_addr.sin_addr));
	tftp_addr.sin_family = AF_INET;
	tftp_addr.sin_port = TFTP_PORT;
    
	sock_fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	if (sock_fd < 0)
	{
	    printf("can't create a socket\n");
	    return -1;
	}
	
	/* set socket option */
	sock_opt = 5000; /* 5 seconds */
	lwip_setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &sock_opt, sizeof(sock_opt));

	/* make tftp request */
	tftp_buffer[0] = 0;			/* opcode */
	tftp_buffer[1] = TFTP_RRQ; 	/* RRQ */
	length = sprintf((char *)&tftp_buffer[2], "%s", filename) + 2;
	tftp_buffer[length] = 0; length ++;
	length += sprintf((char*)&tftp_buffer[length], "%s", "octet");
	tftp_buffer[length] = 0; length ++;

	fromlen = sizeof(struct sockaddr_in);
	
	/* send request */	
	lwip_sendto(sock_fd, tftp_buffer, length, 0, 
		(struct sockaddr *)&tftp_addr, fromlen);
	
	do
	{
		length = lwip_recvfrom(sock_fd, tftp_buffer, sizeof(tftp_buffer), 0, 
			(struct sockaddr *)&from_addr, &fromlen);
		
		if (length > 0)
		{
			if(loc + length - 4 > maxlen)
			{
				if(loc > 0)
					printf("\n");
				printf("Exceded max length\n");
				return -1;
			}
			memcpy(buffer + loc, &tftp_buffer[4], length - 4);
			loc += length - 4;
			//write(fd, (char*)&tftp_buffer[4], length - 4);
			printf("#");

			/* make ACK */			
			tftp_buffer[0] = 0; tftp_buffer[1] = TFTP_ACK; /* opcode */
			/* send ACK */
			lwip_sendto(sock_fd, tftp_buffer, 4, 0, 
				(struct sockaddr *)&from_addr, fromlen);
		}
	} while (length == 516);

	if (length == 0){
		if(loc > 0)
			printf("\n");
		printf("timeout\n");
		return -1;
	}
	else
	{
		if(loc > 0)
			printf("\n");
		printf("Transfter of %i bytes compelted\n", loc);
	}

	lwip_close(sock_fd);

	return loc;
}

int boot_tftp(const char *server_addr, const char *tftp_bootfile)
{
	char *args = strchr(tftp_bootfile, ' ');
	if (args)
		*args++ = 0;

	//const char *msg = " was specified, neither manually nor via dhcp. aborting tftp.\n";
	
	
	if (!(tftp_bootfile && *tftp_bootfile))
	{
		printf("no tftp bootfile name");
		//printf(msg);
		return -1;
	}
	//printf(" * loading tftp bootfile '%s:%s'\n", server_addr, tftp_bootfile);
	
	void * elf_raw=malloc(ELF_MAXSIZE);
	
	int res = do_tftp(elf_raw, ELF_MAXSIZE, server_addr, tftp_bootfile);
	if (res < 0){
		free(elf_raw);
		return res;
	}
	
	launch_elf(elf_raw,res);
	
	free(elf_raw);
	return 0;
}

extern int boot_tftp_url(const char *url)
{
	const char *bootfile = url;
	
	char server_addr[20];
	
	if (!bootfile)
		bootfile = "";
	
	// ip:/path
	// /path
	// ip
	
	const char *r = strchr(bootfile, ':');
	
	if (r)
	{
		int l = r - bootfile;
		if (l > 19)
			l = 19;
		memcpy(server_addr, bootfile, l);
		server_addr[l] = 0;
		bootfile = r + 1;
	} else
	{
		*server_addr = 0;
		bootfile = url;
	}
	
	return boot_tftp(server_addr, bootfile);
}
