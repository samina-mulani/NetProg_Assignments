#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/types.h>
#include <string.h>
#include <arpa/inet.h>

//OPCODES
#define RRQ 1
#define DATA 3
#define ACK 4
#define ERROR 5

#define MAXCLIENTS 1000
#define LEN 516

#define S_PORT 49152

/*** Data structures required ***/

typedef struct rrq{
	char fnameMode[514];
}rrq;

typedef struct data{
	uint16_t blockNum;
	char fdata[512];
}data;

typedef struct ack{
	uint16_t blockNum;
}ack;

typedef struct error{
	uint16_t errCode;
	char errMsg[512];
}error;

typedef union msg{
	rrq r;
	data d;
	ack a;
	error e;
}msg;

typedef struct packet{
	uint16_t op;
	msg details;
}packet;

typedef struct clientInfo{
	int valid;
	uint16_t blockNum;
	FILE * fp;
	int sfd;
	struct sockaddr_in caddr;
	int port;
	int lastACK;
	int bytes; //if lastACK == 1, this is set to the number of bytes in data packet to be sent minus the header info
	int retransmit; //number of times a data packet has been retransmitted
}clientInfo;

clientInfo cInfo[MAXCLIENTS]; //to store client info - global variable

/*** End of Data structures ***/

packet makeErrP(int errCode,char * emsg)
{
	packet p;
	p.op = htons(ERROR);
	p.details.e.errCode = htons(errCode);
	strcpy(p.details.e.errMsg,emsg);
	return p;
}

packet makeDataP(int blockNum,FILE* fp,int *lastACK,int *bRead)
{
	packet p;
	p.op = htons(DATA);
	p.details.d.blockNum = htons(blockNum);
	int bytesr;
	if((bytesr=fread(p.details.d.fdata,1,512,fp))==-1)
	{
		printf("Error with fread\n");
		exit(1);
	}
	if(bytesr<512)
		*lastACK = 1;
	*bRead=bytesr;
	return p;
}

int parseFNM(char fnameMode[],char fname[],char mode[])
{
	int i;
	for(i=0;i<514;i++)
	{
		if(fnameMode[i]=='\0') break;
		fname[i]=fnameMode[i];
	}
	if(i>=514) return 1;
	fname[i]='\0';
	int j=0;
	i++;
	for(;i<514;i++)
	{
		if(fnameMode[i]=='\0') break;
		mode[j]=fnameMode[i];
		j++;
	}
	if(i>=514) return 1;
	mode[j]='\0';
	return 0;
}

void mprint(packet p,struct sockaddr_in client,int port,int dir)
{
	int op = ntohs(p.op);
	if(op==RRQ)
	{
		char fname[514],mode[514];
		printf("\nRRQ received from CLIENT (Client Port - %d ; Server Port - %d) : %s\n",client.sin_port,port,inet_ntoa(client.sin_addr));
		int ret = parseFNM(p.details.r.fnameMode,fname,mode);
		if(!ret)printf("File Name : %s \tMode : %s\n",fname,mode);
		else printf("Malformed packet!!\n");
	}
	else if(op==ACK)
	{
		printf("\nACK received from CLIENT (Client Port - %d ; Server Port - %d) : %s\n",client.sin_port,port,inet_ntoa(client.sin_addr));
		printf("Block Number : %d\n",ntohs(p.details.a.blockNum));
	}
	else if(op==DATA)
	{
		printf("\nDATA sent to CLIENT (Client Port - %d ; Server Port - %d) : %s\n",client.sin_port,port,inet_ntoa(client.sin_addr));
		printf("Block Number : %d\n",ntohs(p.details.d.blockNum));
	}
	else if(op==ERROR)
	{
		if(dir==0)
		{
			printf("\nERROR received from CLIENT (Client Port - %d ; Server Port - %d) : %s\n",client.sin_port,port,inet_ntoa(client.sin_addr));
			printf("Error Code : %d\tError Message : %s\n",ntohs(p.details.e.errCode),p.details.e.errMsg);
		}
		else if(dir==1)
		{
			printf("\nERROR sent to CLIENT (Client Port - %d ; Server Port - %d) : %s\n",client.sin_port,port,inet_ntoa(client.sin_addr));
			printf("Error Code : %d\tError Message : %s\n",ntohs(p.details.e.errCode),p.details.e.errMsg);
		}
	}
}

void retransmitPacket(clientInfo cInfo)
{
	if(fseek(cInfo.fp,-cInfo.bytes,SEEK_CUR)!=0)
	{
		perror("fseek");
		exit(1);
	}
	packet sendC = makeDataP(cInfo.blockNum,cInfo.fp,&cInfo.lastACK,&cInfo.bytes);
	char suffix[3];
	switch(cInfo.retransmit)
	{
		case 1: strcpy(suffix,"st");
		break;
		case 2: strcpy(suffix,"nd");
		break;
		case 3: strcpy(suffix,"rd");
		break;
		default: printf("Unexpected value of retransmit value\n");
		exit(1);
	}
	printf("\n**RETRANSMITTING %d%s time**",cInfo.retransmit,suffix);
	mprint(sendC,cInfo.caddr,cInfo.port,1);
	if(cInfo.lastACK==1) //manually form packet and send if last packet
	{
		char buff[cInfo.bytes];
		int j=0;
		while(j!=cInfo.bytes){buff[j]=sendC.details.d.fdata[j];j++;}
		sendto(cInfo.sfd,&sendC.op,sizeof(sendC.op),MSG_MORE,(struct sockaddr*)&cInfo.caddr,sizeof(cInfo.caddr));
		sendto(cInfo.sfd,&sendC.details.d.blockNum,sizeof(sendC.details.d.blockNum),MSG_MORE,(struct sockaddr*)&cInfo.caddr,sizeof(cInfo.caddr));
		if(sendto(cInfo.sfd,&buff,sizeof(buff),0,(struct sockaddr*)&cInfo.caddr,sizeof(cInfo.caddr))==-1)
		{
			perror("sendto");
			exit(1);
		}
	}
	else if(sendto(cInfo.sfd,&sendC,sizeof(sendC),0,(struct sockaddr*)&cInfo.caddr,sizeof(cInfo.caddr))==-1)
	{
		perror("sendto");
		exit(1);
	}
}

int main(int argc,char *argv[])
{

	for(int i=0;i<MAXCLIENTS;i++)
	{
		cInfo[i].valid=-1;
		cInfo[i].blockNum=-1;
		cInfo[i].fp=NULL;
		cInfo[i].sfd=-1;
		cInfo[i].lastACK=-1;
		cInfo[i].bytes=512;
		cInfo[i].retransmit=0;
	}

	int maxfd,maxc=-1,newport;
	fd_set rset,rs,oldset,newset;
	FD_ZERO(&rset);
	FD_ZERO(&oldset);
	FD_ZERO(&newset);

	if(argc!=2)
	{
		printf("Correct usage : <executable> <port_number>\n");
		exit(1);
	}

	int port_number = atoi(argv[1]);
	int lfd = socket(AF_INET,SOCK_DGRAM,0);
	if(lfd==-1)
	{
		perror("socket");
		exit(1);
	}

	struct sockaddr_in servaddr;
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port_number);
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	int err = bind(lfd, (struct sockaddr*) &servaddr,sizeof(servaddr));
	if(err==-1)
	{
		perror("bind");
		exit(1);
	}
	printf("Listening on port : %d\n",ntohs(servaddr.sin_port));

	newport=S_PORT;
	maxfd = lfd;
	FD_SET(lfd,&rset);
	struct timeval timer;
	timer.tv_sec=5;
	timer.tv_usec=0;

	while(1)
	{
		rs=rset;
		int nsel = select(maxfd+1,&rs,NULL,NULL,&timer);

		if(nsel==0) //handling timeout
		{
			timer.tv_sec=5;
			timer.tv_usec=0;
			for(int i=0;i<maxc+1;i++)
			{
				if(cInfo[i].valid==1&&FD_ISSET(cInfo[i].sfd,&oldset)&&!FD_ISSET(cInfo[i].sfd,&newset))
				{
					cInfo[i].retransmit++;
					if(cInfo[i].retransmit<=3)
						retransmitPacket(cInfo[i]);
					else //has retransmitted 3 times already with no response, closes connection
					{
						printf("\nTransmit Limit exceeded. (Client IP: %s, Client Port: %d, Server Port: %d, BlockNum: %d)\n",inet_ntoa(cInfo[i].caddr.sin_addr),cInfo[i].caddr.sin_port,cInfo[i].port,cInfo[i].blockNum);
						cInfo[i].valid=-1;
						cInfo[i].blockNum=-1;
						cInfo[i].lastACK=-1;
						cInfo[i].bytes=512;
						cInfo[i].retransmit=0;
						FD_CLR(cInfo[i].sfd,&rset);
						fclose(cInfo[i].fp);
						close(cInfo[i].sfd);
						cInfo[i].fp=NULL;
						cInfo[i].sfd=-1;
						FD_CLR(cInfo[i].sfd,&oldset);
					}
				}
			}
			for(int i=0;i<maxc+1;i++)
			{
				if(cInfo[i].valid==1&&FD_ISSET(cInfo[i].sfd,&newset))
				{
					FD_SET(cInfo[i].sfd,&oldset);
				}
			}
			FD_ZERO(&newset);
			continue;
		}

		if(FD_ISSET(lfd,&rs)) //new client connection
		{
			struct sockaddr_in client;
			socklen_t len=sizeof(client);
			packet p;
			int ret = recvfrom(lfd,&p,sizeof(p),0,(struct sockaddr*)&client,&len);
			if(ret==-1) { perror("recvfrom");exit(1);}
			mprint(p,client,port_number,0);

			char fname[514],mode[514];
			if(ntohs(p.op)!=RRQ||parseFNM(p.details.r.fnameMode,fname,mode))
			{
				packet sendC = makeErrP(0,"Malformed Packet");
				mprint(sendC,client,port_number,1);
				if(sendto(lfd,&sendC,sizeof(sendC),0,(struct sockaddr*)&client,sizeof(client))==-1)
				{
					perror("sendto");
					exit(1);
				}
				nsel--;
			}
			else
			{
				FILE*fp=NULL;fopen(fname,"rb");
				if(strcmp("netascii",mode)==0)
					fp=fopen(fname,"r");
				else if(strcmp("octet",mode)==0)
					fp=fopen(fname,"rb");
				if(fp==NULL)
				{
					packet sendC = makeErrP(1,"File not found");
					mprint(sendC,client,port_number,1);
					if(sendto(lfd,&sendC,sizeof(sendC),0,(struct sockaddr*)&client,sizeof(client))==-1)
					{
						perror("sendto");
						exit(1);
					}
					nsel--;
				}
				else
				{
					newport++; //using subsequent ports
					if(newport==port_number)
						newport++;
					int sfd = socket(AF_INET,SOCK_DGRAM,0);
					struct sockaddr_in newservaddr;
					bzero(&newservaddr,sizeof(newservaddr));
					newservaddr.sin_family = AF_INET;
					newservaddr.sin_port = htons(newport);
					newservaddr.sin_addr.s_addr = htonl(INADDR_ANY);
					int err = bind(sfd, (struct sockaddr*) &newservaddr,sizeof(newservaddr));
					if(err==-1)
					{
						perror("bind");
						exit(1);
					}

					int k;
					for(k=0;k<MAXCLIENTS;k++)
					{
						if(cInfo[k].valid==-1)
						{
							cInfo[k].valid=1;
							cInfo[k].blockNum=1;
							cInfo[k].fp=fp;
							cInfo[k].sfd=sfd;
							cInfo[k].caddr=client;
							cInfo[k].port=newport;
							break;
						}
					}

					if(k>maxc) maxc=k;
					if(sfd>maxfd) maxfd=sfd;

					packet sendC = makeDataP(cInfo[k].blockNum,cInfo[k].fp,&cInfo[k].lastACK,&cInfo[k].bytes);
					mprint(sendC,cInfo[k].caddr,cInfo[k].port,1);
					if(cInfo[k].lastACK==1) //manually form packet and send if last packet
					{
						char buff[cInfo[k].bytes];
						int j=0;
						while(j!=cInfo[k].bytes){buff[j]=sendC.details.d.fdata[j];j++;}
						sendto(sfd,&sendC.op,sizeof(sendC.op),MSG_MORE,(struct sockaddr*)&client,sizeof(client));
						sendto(sfd,&sendC.details.d.blockNum,sizeof(sendC.details.d.blockNum),MSG_MORE,(struct sockaddr*)&client,sizeof(client));
						if(sendto(sfd,&buff,sizeof(buff),0,(struct sockaddr*)&client,sizeof(client))==-1)
						{
							perror("sendto");
							exit(1);
						}

					}
					else if(sendto(sfd,&sendC,sizeof(sendC),0,(struct sockaddr*)&client,sizeof(client))==-1)
					{
						perror("sendto");
						exit(1);
					}

					FD_SET(sfd,&rset);
					FD_SET(sfd,&newset);
					nsel--;
				}
			}
		}

		else //not a new connection request
		{
			for(int i=0;i<maxc+1;i++)
			{
				if(nsel==0) break;

				if(cInfo[i].valid==1&&FD_ISSET(cInfo[i].sfd,&rs))
				{
					packet recvP;
					socklen_t len;
					struct sockaddr_in client;
					int ret = recvfrom(cInfo[i].sfd,&recvP,sizeof(recvP),0,(struct sockaddr*)&client,&len);
					if(ret==-1) { perror("recvfrom");exit(1);}
					mprint(recvP,client,cInfo[i].port,0);

					if(strcmp(inet_ntoa(cInfo[i].caddr.sin_addr),inet_ntoa(client.sin_addr))!=0||cInfo[i].caddr.sin_port!=client.sin_port) //different client wrongly sent message - ideally, will never occur
					{
						packet sendC = makeErrP(0,"Wrong server!");
						mprint(sendC,client,cInfo[i].port,1);
						if(sendto(cInfo[i].sfd,&sendC,sizeof(sendC),0,(struct sockaddr*)&client,sizeof(client))==-1)
						{
							perror("sendto");
							exit(1);
						}
						nsel--;
					}
					else
					{
						if(ntohs(recvP.op)!=ACK)
						{
							if(ntohs(recvP.op)!=ERROR)
							{
								packet sendC = makeErrP(0,"Malformed packet");
								mprint(sendC,client,cInfo[i].port,1);
								if(sendto(cInfo[i].sfd,&sendC,sizeof(sendC),0,(struct sockaddr*)&client,sizeof(client))==-1)
								{
									perror("sendto");
									exit(1);
								}
							}

							cInfo[i].valid=-1;
							cInfo[i].blockNum=-1;
							cInfo[i].lastACK=-1;
							cInfo[i].bytes=512;
							cInfo[i].retransmit=0;
							FD_CLR(cInfo[i].sfd,&rset);
							FD_CLR(cInfo[i].sfd,&oldset);
							fclose(cInfo[i].fp);
							close(cInfo[i].sfd);
							cInfo[i].fp=NULL;
							cInfo[i].sfd=-1;
							nsel--;
						}
						else
						{
							if(cInfo[i].lastACK==1&&ntohs(recvP.details.a.blockNum)==cInfo[i].blockNum) //no more DATA packets to be sent
							{
								cInfo[i].valid=-1;
								cInfo[i].blockNum=-1;
								cInfo[i].lastACK=-1;
								cInfo[i].bytes=512;
								cInfo[i].retransmit=0;
								FD_CLR(cInfo[i].sfd,&rset);
								FD_CLR(cInfo[i].sfd,&oldset);
								fclose(cInfo[i].fp);
								close(cInfo[i].sfd);
								cInfo[i].fp=NULL;
								cInfo[i].sfd=-1;
								nsel--;
							}
							else
							{
								if(ntohs(recvP.details.a.blockNum)==cInfo[i].blockNum)
								{
									packet sendC = makeDataP(cInfo[i].blockNum+1,cInfo[i].fp,&cInfo[i].lastACK,&cInfo[i].bytes);
									cInfo[i].blockNum++;
									cInfo[i].retransmit=0;
									mprint(sendC,cInfo[i].caddr,cInfo[i].port,1);
									if(cInfo[i].lastACK==1) //manually form packet and send if last packet
									{
										char buff[cInfo[i].bytes];
										int j=0;
										while(j!=cInfo[i].bytes){buff[j]=sendC.details.d.fdata[j];j++;}
										sendto(cInfo[i].sfd,&sendC.op,sizeof(sendC.op),MSG_MORE,(struct sockaddr*)&client,sizeof(client));
										sendto(cInfo[i].sfd,&sendC.details.d.blockNum,sizeof(sendC.details.d.blockNum),MSG_MORE,(struct sockaddr*)&client,sizeof(client));
										if(sendto(cInfo[i].sfd,&buff,sizeof(buff),0,(struct sockaddr*)&client,sizeof(client))==-1)
										{
											perror("sendto");
											exit(1);
										}
									}
									else if(sendto(cInfo[i].sfd,&sendC,sizeof(sendC),0,(struct sockaddr*)&client,sizeof(client))==-1)
									{
										perror("sendto");
										exit(1);
									}
									FD_SET(cInfo[i].sfd,&newset);
								}
								nsel--; //packets with blockNum less than current  - previous ACKS - are ignored
							}
						}
					}
				}
			}//end of for
		} //end of else
	}
}