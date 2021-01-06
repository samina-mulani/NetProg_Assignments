#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_CHUNKSIZE 2000

#define path1 "./client.c" 
#define path2 "./d_server.c"
#define path3 "./m_server.c"

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

struct mtod
{
	long mtype;
	int op;
	int pid;
	int chunks1[100];
	int size1;
	int chunks2[100];
	int size2;
};

int main(int argc,char *argv[])
{
	if(argc!=2)
	{
		printf("Enter only one argument for the value of N (>=3) which is also divisible by 3\n");
		exit(1); 
	}
	int N=atoi(argv[1]);
	if(N<3)
	{
		printf("Invalid value of N (Enter a value >=3 and divisible by 3)\n");
		exit(1);
	}

	//creating the directories to be used by data servers
	struct stat st = {0};

	for(int i=1;i<=N;i++) //creation of D folders
	{
		char dirname[50];
		char number = i+'0';
		strcpy(dirname,"./d");
		strncat(dirname,&number,1);
		if (stat(dirname, &st) == -1) {
    		mkdir(dirname, 0777);
		}
	}

	key_t key3,key4,key5;
	int qid3,qid4,qid5;

	if((key3=ftok(path3,3))==-1)
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
	//message Q from M to D
	if((qid3=msgget(key3,0))==-1)
	{
		perror("msgget");
		exit(1);
	}
	//message Q from C to D
	if((qid4=msgget(key4,IPC_CREAT|0666))==-1)
	{
		perror("msgget");
		exit(1);
	}
	//message Q from D to C
	if((qid5=msgget(key5,IPC_CREAT|0666))==-1)
	{
		perror("msgget");
		exit(1);
	}
	

	int pid[N]; //stores pids of D servers
	for(int i=0;i<N;i++) pid[i]=-1;
	pid[0]=getpid();
	//need to create N data servers
	for(int i=0;i<N-1;i++)
	{
		if(fork())
			break;
		else
		{
			pid[i+1]=getpid();
		}
	}
	
	while(1)
	{
		for(int i=0;i<N;i++)
		{
			switch(pid[i]==getpid()) //in data server i+1
			{
				case 0: break;
				case 1:
				;
				//message from client
				int flag=0;
				struct ctod clientM;
				if(msgrcv(qid4,&clientM,sizeof(clientM)-sizeof(long),i+1,IPC_NOWAIT)==-1) //doesn't wait
				{
					if(errno==ENOMSG) flag=1;
					else
					{	
						perror("msgrcv");
						exit(1);
					}
				}
				int pidC=clientM.pid;
				if(!flag)
				{
					if(clientM.size!=-1)
					{
						//add chunk
						char buff1[100];
						char number = i+1+'0';
						strcpy(buff1,"./d");
						strncat(buff1,&number,1);
						strcat(buff1,"/");
						int length = snprintf( NULL, 0, "%d", clientM.CID);
						char* string_CID = malloc( length + 1 );
						snprintf(string_CID, length + 1, "%d", clientM.CID);
						strcat(buff1,string_CID); //buff1 has file path where chunk is to be added including chunk ID
						int fd1 = open(buff1,O_CREAT|O_RDWR,0777);
						if(fd1==-1)
						{
							perror("fd");
							exit(1);
						}
						free(string_CID);
						if(write(fd1,clientM.chunk,clientM.size)==-1)
						{
							perror("write");
							struct dtoc sendStat;
							sendStat.mtype=pidC;
							sendStat.status=1; //error
							sendStat.CID=clientM.CID;
							if(msgsnd(qid5,&sendStat,sizeof(sendStat)-sizeof(long),0)==-1)
							{
								perror("msgsnd");
								exit(1);
							}
							
						}
						else
						{
							struct dtoc sendStat;
							sendStat.mtype=pidC;
							sendStat.status=0; //success
							sendStat.CID=clientM.CID;
							if(msgsnd(qid5,&sendStat,sizeof(sendStat)-sizeof(long),0)==-1)
							{
								perror("msgsnd");
								exit(1);
							}	
						}
						
					}
					else
					{
						//cmd on D server
						struct dtoc sendRes;
						sendRes.mtype=pidC;
						sendRes.resSize=0;
						char cmd[100];
						for(int j=0;j<strcspn(clientM.cmd,"/");j++)
						{
							cmd[j]=clientM.cmd[j];
							if(j+1==strcspn(clientM.cmd,"/"));
							cmd[j+1] = '\0';
						}
						int length = snprintf( NULL, 0, "%d", clientM.CID);
						char* string_CID = malloc( length + 1 );
						snprintf(string_CID, length + 1, "%d", clientM.CID);
						strcat(cmd,string_CID); //cmd holds cmd to be executed
						char dir[50];
						char num = i+1+'0';
						strcpy(dir,"./d");
						strncat(dir,&num,1);
						int fd = open(".", O_RDONLY);
						chdir(dir); //change directory to corresponding D folder
						printf("Exec cmd on D%d: %s\n",i+1,cmd);
						FILE* fp = popen(cmd,"r");
						fchdir(fd);
						free(string_CID);
						if(fp==NULL)
						{
							sendRes.status=1; //cmd failed
							if(msgsnd(qid5,&sendRes,sizeof(sendRes)-sizeof(long),0)==-1)
							{
								perror("msgsnd");
								exit(1);
							}
						}
						int code=fread(sendRes.result,1,sizeof(sendRes.result)-1,fp);
						if(code==-1||pclose(fp)!=0) //cmd failed
						{
							sendRes.status=1;
							if(msgsnd(qid5,&sendRes,sizeof(sendRes)-sizeof(long),0)==-1)
							{
								perror("msgsnd");
								exit(1);
							}	
						}
						else
						{
							sendRes.result[code]='\0';
							sendRes.resSize=code; //size of result
							sendRes.status=0;
							if(msgsnd(qid5,&sendRes,sizeof(sendRes)-sizeof(long),0)==-1)
							{
								perror("msgsnd");
								exit(1);
							}
						}					
					}
				}
				
				else
				{
					//RM or CP
					struct mtod rmcp;
					if(msgrcv(qid3,&rmcp,sizeof(rmcp)-sizeof(long),i+1,IPC_NOWAIT)==-1)
					{
						if(errno==ENOMSG) break;
						perror("msgrcv");
						exit(1);
					}

					switch(rmcp.op)
					{
						case 1:
						;//copy
						struct dtoc cpStat;
						cpStat.mtype=pidC;
						if(rmcp.size1==-1) //copying over, send message to C
						{
							cpStat.status=0;
							if(msgsnd(qid5,&cpStat,sizeof(cpStat)-sizeof(long),0)==-1)
							{
								perror("msgsnd");
								break;
							}
							break;
						}
						char dir[50];
						char num = i+1+'0';
						strcpy(dir,"./d");
						strncat(dir,&num,1);
						int fd = open(".", O_RDONLY);
						chdir(dir);
						for(int k=0;k<rmcp.size2;k++)
						{
							char cmd[100];
							strcpy(cmd,"cp ");
							int length1 = snprintf( NULL, 0, "%d", rmcp.chunks1[k]);
							char* string_CID1 = malloc( length1 + 1 );
							snprintf(string_CID1, length1 + 1, "%d", rmcp.chunks1[k]);
							strcat(cmd,string_CID1);
							strcat(cmd," ");
							int length2 = snprintf( NULL, 0, "%d", rmcp.chunks2[k]);
							char* string_CID2 = malloc( length2 + 1 );
							snprintf(string_CID2, length2 + 1, "%d", rmcp.chunks2[k]);
							strcat(cmd,string_CID2);
							printf("Exec cmd on D%d: %s\n",i+1,cmd);
							FILE* fp = popen(cmd,"r");
							if(fp!=NULL&&fclose(fp)!=0)
							{
								cpStat.status=1;
								if(msgsnd(qid5,&cpStat,sizeof(cpStat)-sizeof(long),0)==-1)
								{
									perror("msgsnd");
								}
								printf("Error in copying chunk %d on D%d\n",rmcp.chunks1[k],i+1);
								break;
							}
						}					
						fchdir(fd);
						break;
						case 2: //delete chunks
						;
						int flag;
						for(int k=0;k<rmcp.size1;k++)
						{	
							flag=0;
							char buff1[50];
							char number = i+1+'0';
							strcpy(buff1,"./d");
							strncat(buff1,&number,1);
							strcat(buff1,"/");
							int length = snprintf( NULL, 0, "%d", rmcp.chunks1[k]);
							char* string_CID = malloc( length + 1 );
							snprintf(string_CID, length + 1, "%d", rmcp.chunks1[k]);
							strcat(buff1,string_CID);
							if(remove(buff1)==-1){
								perror("remove");
								flag=1;
								break;
							}
						}
						if(!flag)
						printf("Deleted chunks on server D%d\n",i+1);
					}
				}				
				break;
			}
		}
	}
}