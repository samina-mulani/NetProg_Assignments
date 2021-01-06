#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

#define MYMAXTTL 30
#define BUFSIZE 1500
#define DPORT_START 35000
#define SPORT 8089

pthread_mutex_t m_msg[MYMAXTTL];
pthread_mutex_t m_caddr = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m_print = PTHREAD_MUTEX_INITIALIZER;

int sfd=-1,sport,dport[MYMAXTTL];
pthread_cond_t cond[MYMAXTTL];

struct sockaddr_in caddr; //for host information

struct icmp_reply{
	int ttl_index; //validity indicator
	char rcvbuf[BUFSIZE];
	int numRead;
}msg[MYMAXTTL];

void * icmp_recv()
{
	pthread_detach(pthread_self());

	//raw socket creation
	int rfd=-1;
	if((rfd = socket(AF_INET,SOCK_RAW,IPPROTO_ICMP))==-1)
	{
		perror("rfd");
		exit(1);
	}
 
	struct sockaddr_in raddr;
	int numbytes;
	char rcvbuf[BUFSIZE];
	socklen_t len;
	struct ip* ip1,*ip2;
	struct udphdr* udp1;
	int hlen1,hlen2;
	int ttl_index;

	while(1)
	{
		
		bzero(rcvbuf,sizeof(rcvbuf));
		hlen1=hlen2=0;
		ttl_index=-1;

		numbytes = recvfrom(rfd,rcvbuf,sizeof(rcvbuf),0,(struct sockaddr*)&raddr,&len);

		if(numbytes<=0) continue;

		if(numbytes==-1)
		{
			perror("recvfrom rfd");
			continue;
		}
		
		ip1 = (struct ip*)rcvbuf; //first IP header
		hlen1 = ip1->ip_hl << 2;
		
		if(numbytes<hlen1+8) continue; //msg not long enough
		ip2 = (struct ip*)(rcvbuf+hlen1+8); //second IP header
		hlen2 = ip2->ip_hl << 2;
		
		if(numbytes<hlen1+8+hlen2+4) continue; //msg not long enough
		udp1 = (struct udphdr*)(rcvbuf+hlen1+8+hlen2); //UDP header - first 8 bytes

		for(int i=0;i<MYMAXTTL;i++)
		{
			if(htons(dport[i])==udp1->dest) //matched with a TTL thread
			{
				ttl_index=i;
				break;
			}
		}

		if(ttl_index==-1) continue; //no match was found

		//copy received buffer into global shared variable - one element of the array
		pthread_mutex_lock(&m_msg[ttl_index]);
		msg[ttl_index].ttl_index = ttl_index;
		msg[ttl_index].numRead = numbytes;
		for(int i=0;i<numbytes;i++)
			msg[ttl_index].rcvbuf[i]=rcvbuf[i];
		pthread_mutex_unlock(&m_msg[ttl_index]);

		pthread_cond_signal(&cond[ttl_index]); //signal the right TTL thread
	}

}

void * ttl_send(void * arg)
{
	int * ttl_ptr = (int*) arg;
	int ttl = *ttl_ptr;

	pthread_mutex_lock(&m_caddr);
	dport[ttl-1] = DPORT_START+ttl-1; //set destination port
	caddr.sin_port = htons(dport[ttl-1]);
	setsockopt(sfd,IPPROTO_IP,IP_TTL,&ttl,sizeof(ttl)); //set TTL
	if(sendto(sfd,NULL,0,0,(struct sockaddr*)&caddr,sizeof(caddr))==-1)
	{
		perror("sendto");
		exit(1);
	}
	pthread_mutex_unlock(&m_caddr);

	struct timespec ts;
	int err,flag_timeout;
	int numRead;
	char rcvbuf[BUFSIZE];
	struct ip *ip1, *ip2;
	struct icmp *icmp1;
	struct udphdr *udp1; 
	int hlen1,hlen2,icmplen;

	while(1)
	{
		flag_timeout=err=0;
		numRead=-1;
		bzero(rcvbuf,sizeof(rcvbuf));
		hlen1=hlen2=icmplen=0;

		pthread_mutex_lock(&m_msg[ttl-1]);
		clock_gettime(CLOCK_REALTIME,&ts); //get current time
		ts.tv_sec+=5 ;
		while(msg[ttl-1].ttl_index!=ttl-1)
		{
			err = pthread_cond_timedwait(&cond[ttl-1],&m_msg[ttl-1],&ts); //waits till either signal received or time out
			if(err==ETIMEDOUT)
			{
				printf("TTL=%3d \t**Timeout**\n",ttl);
				flag_timeout=1;
				break;
			}
		}

		if(!err&&!flag_timeout) //no error occurred, so copy needed data to local variable
		{
			numRead=msg[ttl-1].numRead;
			for(int k=0;k<msg[ttl-1].numRead;k++)
				rcvbuf[k]=msg[ttl-1].rcvbuf[k];
		}

		msg[ttl-1].ttl_index=-1;
		msg[ttl-1].numRead=-1;
		bzero(msg[ttl-1].rcvbuf,sizeof(msg[ttl-1].rcvbuf));
		pthread_mutex_unlock(&m_msg[ttl-1]);

		if(flag_timeout) break;
		if(err) continue;

		//process reply
		ip1 = (struct ip*) rcvbuf; //first IP header
		hlen1 = ip1->ip_hl << 2;

		icmp1 = (struct icmp*) (rcvbuf+hlen1); //ICMP header
		if((icmplen=numRead-hlen1)<8)
			continue;

		if(icmp1->icmp_type==ICMP_TIMXCEED&&icmp1->icmp_code==ICMP_TIMXCEED_INTRANS) //intermediate router
		{
			if(icmplen<8+sizeof(struct ip))
				continue;

			ip2 = (struct ip*)(rcvbuf+hlen1+8); //second IP header
			hlen2 = ip2->ip_hl << 2;
			if(icmplen<8+hlen2+4)
				continue;

			if(strcmp(inet_ntoa(ip2->ip_dst),inet_ntoa(caddr.sin_addr))!=0) //Check if IP destination matches host's IP
				continue;

			udp1 = (struct udphdr*)(rcvbuf+hlen1+8+hlen2); //UDP header - first 8 bytes
			if(ip2->ip_p==IPPROTO_UDP&&udp1->source==htons(sport)&&udp1->dest==htons(dport[ttl-1]))
			{
				pthread_mutex_lock(&m_print);
				printf("TTL=%3d \tIntermediate IP: %s\n",ttl,inet_ntoa(ip1->ip_src));
				pthread_mutex_unlock(&m_print);
				break;
			}
		}
		else if(icmp1->icmp_type==ICMP_UNREACH) //destination reached
		{
			if(icmplen<8+sizeof(struct ip))
				continue;

			ip2 = (struct ip*)(rcvbuf+hlen1+8); //second IP header
			hlen2 = ip2->ip_hl << 2;
			if(icmplen<8+hlen2+4)
				continue;

			if(strcmp(inet_ntoa(ip2->ip_dst),inet_ntoa(caddr.sin_addr))!=0) //Check if IP destination matches host's IP
				continue;

			udp1 = (struct udphdr*)(rcvbuf+hlen1+8+hlen2); //UDP header - first 8 bytes
			if(ip2->ip_p==IPPROTO_UDP&&udp1->source==htons(sport)&&udp1->dest==htons(dport[ttl-1]))
			{
				if(icmp1->icmp_code==ICMP_UNREACH_PORT)
				{
					pthread_mutex_lock(&m_print);
					printf("TTL=%3d \tDestination  IP: %s\n",ttl,inet_ntoa(ip1->ip_src));
					pthread_mutex_unlock(&m_print);	
					break;
				}
				
			}	
		}

	}

}

int main(int argc,char* argv[])
{
	if(argc!=2)
	{
		printf("Correct usage : <executable> <hostname>\n");
		exit(1);
	}

	int errcode;
	struct addrinfo hints,*res;
	bzero(&hints,sizeof(hints));
	hints.ai_family=AF_INET;
	if((errcode=getaddrinfo(argv[1],NULL,&hints,&res))!=0)
	{
		printf("getaddrinfo: %s\n",gai_strerror(errcode));
		exit(1);
	}

	for(int i=0;i<MYMAXTTL;i++) //initialise global shared variables of ICMP replies 
	{
		msg[i].ttl_index=-1;
		msg[i].numRead=-1;
		bzero(msg[i].rcvbuf,sizeof(msg[i].rcvbuf));	
	}
	

	//set host address in global variable
	bzero(&caddr,sizeof(caddr));
	caddr.sin_family = ((struct sockaddr_in*)(res->ai_addr))->sin_family;
	caddr.sin_addr = ((struct sockaddr_in*)(res->ai_addr))->sin_addr;
	strcpy(caddr.sin_zero,((struct sockaddr_in*)(res->ai_addr))->sin_zero);

	sport=SPORT;
	struct sockaddr_in myaddr;
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = ntohs(sport);
	myaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if((sfd=socket(AF_INET,SOCK_DGRAM,0))==-1) //to send UDP datagrams
	{
		perror("sfd");
		exit(1);
	}

	if(bind(sfd,(struct sockaddr*)&myaddr,sizeof(myaddr))==-1)
	{
		perror("bind");
		exit(1);
	}

	for(int i=0;i<MYMAXTTL;i++) //initialise global array of destination ports
	{
		dport[i]=-1;
	}

	for(int i=0;i<MYMAXTTL;i++)
	{
		if(pthread_cond_init(&cond[i],NULL)) //initialise condition variables
		{
			printf("Error in initialising condition variables\n");
			exit(1);
		}
		if(pthread_mutex_init(&m_msg[i],NULL)) //initiliase mutexes
		{
			printf("Error in initialising mutex\n");
			exit(1);
		}
	}


	pthread_t icmp;
	pthread_t tids[MYMAXTTL];

	if(pthread_create(&icmp,NULL,icmp_recv,NULL)!=0) //ICMP thread created
	{
		printf("Error creating ICMP thread\n");
		exit(1);
	}

	int arg[MYMAXTTL];
	for(int i=0;i<MYMAXTTL;i++)
	{
		arg[i] = i+1;
		if(pthread_create(&tids[i],NULL,ttl_send,(void*)&arg[i])!=0) //TTL threads created
		{
			printf("Error creating TTL=%3d thread\n",arg[i]);
			exit(1);
		}
	}

	for(int i=0;i<MYMAXTTL;i++)
	{
		if(pthread_join(tids[i],NULL)!=0) //wait for TTL threads to finish execution
		{
			printf("Error in join\n");
			exit(1);
		}
	}

	return 0;
}
