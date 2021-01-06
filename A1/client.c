#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#define path1 "./client.c" 
#define path2 "./d_server.c"
#define path3 "./m_server.c"

#define MAX_CHUNKSIZE 2000

struct chunkInfo{
	int CID;
	int d1,d2,d3;
};

struct ctom{
	long mtype;
	int pid;
	char forc[100]; //file path or command
};

struct mtoc{
	long mtype;
	int statFor; //secondary status
	int d1,d2,d3;  
	int CID;
	int status;
	struct chunkInfo CIDs[100];
	int size;
};

struct ctod{
	long mtype;
	int CID;
	int pid;
	char cmd[100];
	char chunk[MAX_CHUNKSIZE];
	int size;
};

struct dtoc{
	long mtype;
	int status;
	int CID;
	char result[2*MAX_CHUNKSIZE];
	int resSize;
};

void printMenu()
{
	printf("Enter operation you wish to perform : \n");
	printf(" 1. Add file\n");
	printf(" 2. Copy file (CP)\n");
	printf(" 3. Move file (MV)\n");
	printf(" 4. Remove file (RM)\n");
	printf(" 5. Send command to D server\n");
	printf(" 6. Exit\n");
}

void addFile(char file[],char filepath[],int chunkSize,int qid1,int qid2,int qid4,int qid5)
{
	char buff[chunkSize];
	int fd1 = open(file,O_RDONLY);
	if(fd1==-1)
	{
		perror("open");
		return;
	}
	int numRead;
	//request to add file at path filepath
	struct ctom addReq;
	addReq.mtype=1;
	addReq.pid=getpid();
	strcpy(addReq.forc,filepath);
	if(msgsnd(qid1,&addReq,sizeof(addReq)-sizeof(long),0)==-1)
	{
		perror("msgsnd");
		exit(1);
	}
	//receiving status back from M
	struct mtoc recvStat;
	if(msgrcv(qid2,&recvStat,sizeof(recvStat)-sizeof(long),getpid(),0)==-1)
	{
		perror("msgrcv");
		exit(1);
	}
	else if(recvStat.statFor==1)
	{
		if(recvStat.status==0)
			printf("File hierarchy modified!\n");
		else if(recvStat.status==1)
		{
			printf("Given file path not valid!\n");
			return;
		}
		else if(recvStat.status==2)
		{
			printf("File with given path already exists!\n");
			return;
		}
	}

	//send addchunk req
	struct ctom addChunk;
	addChunk.mtype=2;
	addChunk.pid=getpid();
	strcpy(addChunk.forc,filepath);
	struct mtoc recvAddr;
	while((numRead=read(fd1,buff,chunkSize-1)))
	{
		if(numRead==-1)
		{
			perror("read");
			return;
		}
		buff[numRead]='\0';

		if(msgsnd(qid1,&addChunk,sizeof(addChunk)-sizeof(long),0)==-1)
		{
			perror("msgsnd1");
			exit(1);
		}
		if(msgrcv(qid2,&recvAddr,sizeof(recvAddr)-sizeof(long),getpid(),0)==-1)
		{
			perror("msgrcv");
			exit(1);
		}

		if(recvAddr.statFor!=2)
		{
			printf("Error!\n");
			exit(1);
		}

		//send chunk and its ID to D servers
		struct ctod sendChunk;
		sendChunk.CID=recvAddr.CID;
		sendChunk.pid=getpid();
		strcpy(sendChunk.chunk,buff);
		sendChunk.size=numRead+1;
		struct dtoc recvStat;

		//D1
		sendChunk.mtype=recvAddr.d1;
		if(msgsnd(qid4,&sendChunk,sizeof(sendChunk)-sizeof(long),0)==-1)
		{
			perror("msgsnd2");
			exit(1);
		}
		if(msgrcv(qid5,&recvStat,sizeof(recvStat)-sizeof(long),getpid(),0)==-1)
		{
			perror("msgrcv");
			exit(1);
		}
		if(recvStat.status)
		{
			printf("Error in storing chunk (ID : %d) at server D1!\n",recvStat.CID);
		}
		else
		{
			printf("Chunk (ID : %d) stored at D1!\n",recvStat.CID);
		}

		//D2
		sendChunk.mtype=recvAddr.d2;
		if(msgsnd(qid4,&sendChunk,sizeof(sendChunk)-sizeof(long),0)==-1)
		{
			perror("msgsnd");
			exit(1);
		}
		if(msgrcv(qid5,&recvStat,sizeof(recvStat)-sizeof(long),getpid(),0)==-1)
		{
			perror("msgrcv");
			exit(1);
		}
		if(recvStat.status)
		{
			printf("Error in storing chunk (ID : %d) at server D2!\n",recvStat.CID);
		}
		else
		{
			printf("Chunk (ID : %d) stored at D2!\n",recvStat.CID);
		}
		
		//D3		
		sendChunk.mtype=recvAddr.d3;
		if(msgsnd(qid4,&sendChunk,sizeof(sendChunk)-sizeof(long),0)==-1)
		{
			perror("msgsnd");
			exit(1);
		}
		if(msgrcv(qid5,&recvStat,sizeof(recvStat)-sizeof(long),getpid(),0)==-1)
		{
			perror("msgrcv");
			exit(1);
		}
		if(recvStat.status)
		{
			printf("Error in storing chunk (ID : %d) at server D3!\n",recvStat.CID);
		}
		else
		{
			printf("Chunk (ID : %d) stored at D3!\n",recvStat.CID);
		}

	}
	close(fd1);
}

void cpFile(char cmd[],int qid1,int qid2,int qid4,int qid5)
{
	struct ctom copy;
	copy.mtype=3;
	copy.pid=getpid();
	strcpy(copy.forc,cmd);
	//send copy request to M
	if(msgsnd(qid1,&copy,sizeof(copy)-sizeof(long),0)==-1)
	{
		perror("msgsnd");
		exit(1);
	}

	//receive status 
	struct mtoc copyStat;
	if(msgrcv(qid2,&copyStat,sizeof(copyStat)-sizeof(long),getpid(),0)==-1)
	{
		perror("msgrcv");
		exit(1);
	}
	if(copyStat.statFor!=3)
	{
		printf("Error!\n");
	}
	if(copyStat.status==0)
	{
		printf("File hierarchy modified!\n");
		printf("Waiting for D servers to return status\n");
		struct dtoc cpd;
		for(int i=0;i<copyStat.size;i++) //C should recieve status of copy from copyStat.size number of D servers
		{
			if(msgrcv(qid5,&cpd,sizeof(cpd)-sizeof(long),getpid(),0)==-1)
			{
				perror("msgrcv");
				exit(1);
			}
			if(cpd.status==0)
				printf("Success! (server %d)\n",i+1);
			else
				printf("Failure! (server %d)\n",i+1);

		}

	}
	else if(copyStat.status==1)
	{
		printf("Source file with given path does not exist!\n");
	}
	else if(copyStat.status==2)
	{
		printf("Source or destination file path is not valid!\n");
	}
	else if(copyStat.status==3)
	{
		printf("Check command again!\n");
	}

}

void mvFile(char cmd[],int qid1,int qid2)
{
	struct ctom move;
	move.mtype=4;
	move.pid=getpid();
	strcpy(move.forc,cmd);
	//send move request to M
	if(msgsnd(qid1,&move,sizeof(move)-sizeof(long),0)==-1)
	{
		perror("msgsnd");
		exit(1);
	}

	//receive status
	struct mtoc moveStat;
	if(msgrcv(qid2,&moveStat,sizeof(moveStat)-sizeof(long),getpid(),0)==-1)
	{
		perror("msgrcv");
		exit(1);
	}
	if(moveStat.statFor!=4)
	{
		printf("Error!\n");
		exit(1);
	}

	if(moveStat.status==0)
	{
		printf("File hierarchy modified!\n");
	}
	else if(moveStat.status==1)
	{
		printf("Source file with given path does not exist!\n");
	}
	else if(moveStat.status==2)
	{
		printf("Source or destination file path is not valid!\n");
	}
	else if(moveStat.status==3)
	{
		printf("Check command again!\n");
	}
}

void rmFile(char rmcmd[],int qid1,int qid2)
{
	struct ctom rm;
	rm.mtype=5;
	rm.pid=getpid();
	strcpy(rm.forc,rmcmd);
	//send remove request to M
	if(msgsnd(qid1,&rm,sizeof(rm)-sizeof(long),0)==-1)
	{
		perror("msgsnd");
		exit(1);
	}

	//receive status
	struct mtoc rmStat;
	if(msgrcv(qid2,&rmStat,sizeof(rmStat)-sizeof(long),getpid(),0)==-1)
	{
		perror("msgrcv1");
		exit(1);
	} 
	if(rmStat.statFor!=5)
	{
		printf("Error!\n");
		exit(1);
	}

	if(rmStat.status==0)
	{
		printf("Succesfully deleted!\n");
	}
	else if(rmStat.status==1)
	{
		printf("Source file with given path does not exist!\n");
	}
	else if(rmStat.status==2)
	{
		printf("Source or destination file path is not valid!\n");
	}
	else if(rmStat.status==3)
	{
		printf("Check command again!\n");
	}
}

void sendCmd(char filep[],char cmdD[],int qid1,int qid2,int qid4,int qid5)
{
	struct ctom reqCIDs;
	reqCIDs.mtype=6;
	reqCIDs.pid=getpid();
	strcpy(reqCIDs.forc,filep);

	if(msgsnd(qid1,&reqCIDs,sizeof(reqCIDs)-sizeof(long),0)==-1)
	{
		perror("msgsnd");
		exit(1);
	}

	struct mtoc response;

	while(1)
	{
		if(msgrcv(qid2,&response,sizeof(response)-sizeof(long),getpid(),0)==-1)
		{
			perror("msgrcv");
			exit(1);
		}

		if(response.status==1)
		{
			printf("Given file path not valid!\n");
			return;
		}
		else if(response.status==2)
		{
			printf("File with given path does not exist!\n");
			return;
		}

		else if(response.status==0)
		{
			if(response.size==-1) //M has sent all CIDs
				break;
			else
			{
				struct ctod cmd2d;
				cmd2d.pid=getpid();
				strcpy(cmd2d.cmd,cmdD);
				for(int k=0;k<response.size;k++)
				{
					cmd2d.mtype=response.CIDs[k].d1; //send to 1 server
					cmd2d.CID=response.CIDs[k].CID;
					cmd2d.size=-1; //to indicate cmd is to be carried out and not add chunk
					if(msgsnd(qid4,&cmd2d,sizeof(cmd2d)-sizeof(long),0)==-1)
					{
						perror("msgsnd1");
						exit(1);
					}
					struct dtoc res;
					if(msgrcv(qid5,&res,sizeof(res)-sizeof(long),getpid(),0)==-1)
					{
						perror("msgrcv");
						exit(1);
					}
					if(res.status==0)
					{
						printf("Result from D%d (Chunk:%d) : \n",response.CIDs[k].d1,response.CIDs[k].CID);
						for(int p=0;p<res.resSize;p++)
						{
							printf("%c",res.result[p]);
						}
						printf("\n");
					}
					else
					{
						printf("Error from D%d!\n",response.CIDs[k].d1);
					}

				}

			}
		}

	}

}

int main()
{
	char file[100],filepath[100],cpcmd[100],mvcmd[100],rmcmd[100],cmdD[100],filep[100];
	int chunkSize;
	//Create or open message queues 1 (C to M), 2 (M to C)
	key_t key1,key2,key4,key5;
	int qid1,qid2,qid4,qid5;
	if((key1=ftok(path1,1))==-1)
	{
		perror("ftok");
		exit(1);
	}
	if((key2=ftok(path1,2))==-1)
	{
		perror("ftok");
		exit(1);
	}
	if((key4=ftok(path2,1))==-1)
	{
		perror("ftok");
		exit(1);
	}
	if((key5=ftok(path2,2))==-1)
	{
		perror("ftok");
		exit(1);
	}
	//message Q from C to M
	if((qid1=msgget(key1,0))==-1)
	{
		perror("msgget");
		exit(1);
	}
	//message Q from M to C
	if((qid2=msgget(key2,0))==-1)
	{
		perror("msgget");
		exit(1);
	}
	//message Q from C to D
	if((qid4=msgget(key4,0))==-1)
	{
		perror("msgget");
		exit(1);
	}
	//message Q from D to C
	if((qid5=msgget(key5,0))==-1)
	{
		perror("msgget");
		exit(1);
	}

	while(1)
	{
		int choice;
		printMenu();
		scanf("%d",&choice);
		getchar();
		switch(choice)
		{
			case 1: 
			printf("Enter path (on system) of file to be added (like: /home/samina/f1.txt) : ");
			fgets(file,99,stdin);
			file[strcspn(file,"\n")] = '\0';
			printf("Enter path you want this file to be stored at (like: /home/duser/newname.txt) : ");
			fgets(filepath,99,stdin);
			filepath[strcspn(filepath,"\n")] = '\0';
			do
			{
				printf("Enter chunk size (less than %d and more than 1) : ",MAX_CHUNKSIZE);
				scanf("%d",&chunkSize);
				getchar();
			}while(chunkSize>MAX_CHUNKSIZE&&chunkSize<=1);		
			addFile(file,filepath,chunkSize,qid1,qid2,qid4,qid5);
			break;
			case 2:
			printf("Enter cp cmd with src and destination: ");
			fgets(cpcmd,99,stdin);
			cpcmd[strcspn(cpcmd,"\n")] = '\0';
			cpFile(cpcmd,qid1,qid2,qid4,qid5);
			break;
			case 3:
			printf("Enter mv cmd with src and destination: ");
			fgets(mvcmd,99,stdin);
			mvcmd[strcspn(mvcmd,"\n")] = '\0';
			mvFile(mvcmd,qid1,qid2);
			break;
			case 4:
			printf("Enter rm cmd with full path of file to be removed: ");
			fgets(rmcmd,99,stdin);
			rmcmd[strcspn(rmcmd,"\n")] = '\0';
			rmFile(rmcmd,qid1,qid2);
			break;
			case 5:
			printf("Enter command with file name: ");
			fgets(cmdD,99,stdin);
			cmdD[strcspn(cmdD,"\n")] = '\0';
			int j=0;
			for(int i=strcspn(cmdD,"/");cmdD[i]!='\0';i++,j++) //get filename
			{
				filep[j]=cmdD[i];
			}
			filep[j]='\0';
			sendCmd(filep,cmdD,qid1,qid2,qid4,qid5);
			break;
			case 6:
			exit(0);
			break;
			default: printf("Invalid choice entered!\n");
		}

	}
	
}
