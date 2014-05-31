#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>

//#define SRV_IP "192.168.129.255"
#define SRV_IP "255.255.255.255"
#define PORT 9930

#define LEN	0x5a0

unsigned char buf[LEN];
int location=0;
void print_until(unsigned char until);
int make_data();
void diep(char *s);

void print_until(unsigned char until)
{
	int i;
	for ( i=0;i<=until;i++){
		//printf("%c",i);
		buf[location++]=i;
	}
}

int make_data()
{
	int i;

	for (i=0;i<5;i++)
		print_until(0xff);

	print_until(0x9f);


	for (i=0;i<LEN;i++)
		printf("%c",buf[i]);

	return 0;
}

/* diep(), #includes and #defines like in the server */
void diep(char *s)
{
	perror(s);
	exit(1);
}

int main(void)
{
	struct sockaddr_in si_other;
	struct ifreq interface;
	int s, i, slen=sizeof(si_other);
	int val=1,size=sizeof(val);
	int retn;

	printf("hello: Socket\n");
	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
		diep("socket");

	printf("hello: sockopt\n");
	memset(&interface, 0, sizeof(interface));
	strncpy(interface.ifr_ifrn.ifrn_name, "bnep0",
			sizeof(interface.ifr_ifrn.ifrn_name));

	if (setsockopt(s, SOL_SOCKET, SO_BINDTODEVICE, &interface,
						sizeof(interface)) < 0){
		diep("socket");
		close(s);
	}
	retn = setsockopt(s, SOL_SOCKET, SO_BROADCAST, &val, size);
	if (retn < 0) {
		perror("SO_BROADCAST");
		close(s);
		exit(-1);
	}

	make_data();

	memset((char *) &si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(PORT);

	if (inet_aton(SRV_IP, &si_other.sin_addr)==0) {
		fprintf(stderr, "inet_aton() failed\n");
		exit(1);
	}

	printf("hello: Send packet\n");
	printf("Sending packet %d\n", i);
	if (sendto(s, buf, LEN, 0, (const struct sockaddr *)&si_other, slen)==-1)
		diep("sendto()");

	close(s);
	return 0;
}

