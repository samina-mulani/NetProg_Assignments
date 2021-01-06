#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <sys/msg.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#define path1 "./client.c" 
#define path2 "./d_server.c"
#define path3 "./m_server.c"

struct chunkInfo{
	int CID;
	int d1,d2,d3;
};

struct TrieNode{
	struct TrieNode *children[64]; //assume file paths are case sensitive, can contain numbers, . and /
	struct chunkInfo *CIDs;
	int size; //size of dynamic CIDs array
	int notEnd; //0 if end, nonzero if not end of path
};

struct ctom{
	long mtype;
	int pid; //pid of client sending message
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

struct mtod
{
	long mtype;
	int op; //operation, 1 for copy and 2 for delete
	int pid;
	int chunks1[100];
	int size1;
	int chunks2[100];
	int size2;
};

struct TrieNode *getNode(void) //create new TrieNode
{ 
    struct TrieNode *pNode = NULL; 
  
    pNode = (struct TrieNode *)malloc(sizeof(struct TrieNode)); 
  
    if (pNode) 
    { 
        int i; 
  
        pNode->CIDs=NULL; //default
  		pNode->notEnd=1; //default
  		pNode->size=-1; //default

        for (i=0;i<64;i++) 
            pNode->children[i] = NULL; 
    } 
  
    return pNode; 
} 

int charToInd(char a)
{
	if(a<='Z'&&a>='A') return a-'A';
	else if(a<='z'&&a>='a') return a-'a'+26;
	else if(a<='9'&&a>='0') return a-'0'+52;
	else if(a=='.') return 62;
	else if(a=='/') return 63;
}

int genCID(int *chunkIDs,int size)
{
	srand(time(0));
	int r;
	int flag=0; //set if r found in current list of CIDs
	do
	{
		r=rand();
		flag=0;
		for(int i=0;i<size;i++)
		{	
			if(chunkIDs[i]==r)
			{
				flag=1;
				break;
			}
		}	
	}while(flag);
	return r;
}

void addCID(struct TrieNode *root,char key[],int CID,int type1,int type2,int type3) //add chunk ID to file path along with the data servers they are stored at
{
	int i=0,index;
	struct TrieNode *pCrawl = root;

	while(key[i]!='\0')
	{
		index=charToInd(key[i]);
		pCrawl = pCrawl->children[index];
		i++;
	}
	pCrawl->size++;
	pCrawl->CIDs = (struct chunkInfo*)realloc(pCrawl->CIDs,sizeof(struct chunkInfo)*pCrawl->size);
	pCrawl->CIDs[pCrawl->size-1].CID = CID;
	pCrawl->CIDs[pCrawl->size-1].d1 = type1;
	pCrawl->CIDs[pCrawl->size-1].d2 = type2;
	pCrawl->CIDs[pCrawl->size-1].d3 = type3;
}

int insert(struct TrieNode *root,char key[]) //insert file path in Trie if valid
{  
    int i=0,index; 
  
    struct TrieNode *pCrawl = root; 
  
    while(key[i]!='\0')
    { 
    	if(pCrawl->notEnd==0&&key[i]=='/') return 1; //file invalid
        index = charToInd(key[i]);
        if (!pCrawl->children[index]) 
            pCrawl->children[index] = getNode();   
        pCrawl = pCrawl->children[index]; 
        i++;
    } 
  
    if(!pCrawl->notEnd) return 2; //file exists 
    if(pCrawl->children[63]) return 1; //file invalid
    pCrawl->notEnd = 0;
    pCrawl->size=0;
    return 0; //file hierarchy succesfully modified
}

struct chunkInfo* delUtil(struct TrieNode* root, char key[],int *size) //to get chunk IDs of file to be deleted and the data servers they are located on
{
	int i=0,index; 
  
    struct TrieNode *pCrawl = root; 
  
    while(key[i]!='\0')
    { 
        index = charToInd(key[i]);  
        pCrawl = pCrawl->children[index]; 
        i++;
    } 
    *size=pCrawl->size;
    struct chunkInfo* retCIDs=(struct chunkInfo*)malloc(sizeof(struct chunkInfo)*(*size));
    for(int j=0;j<*size;j++)
    {
    	retCIDs[j].CID = pCrawl->CIDs[j].CID;
    	retCIDs[j].d1 = pCrawl->CIDs[j].d1;
    	retCIDs[j].d2 = pCrawl->CIDs[j].d2;
    	retCIDs[j].d3 = pCrawl->CIDs[j].d3;
    }
    return retCIDs;
}

int exist(struct TrieNode *root,char key[]) //checks if file exists in current hierarchy
{
	int i=0,index; 
    struct TrieNode *pCrawl = root; 
  	if(root==NULL) return 0;
    while(key[i]!='\0')
    { 
        index = charToInd(key[i]);
        if (!pCrawl->children[index]) 
            return 0;
        pCrawl = pCrawl->children[index]; 
        i++;
    } 
    if(!pCrawl->notEnd) return 1; //file exists
    return 0;
}


int isEmpty(struct TrieNode* root) //used in Trie delete
{ 
    for (int i = 0; i < 64; i++) 
        if (root->children[i]) 
            return 0; 
    return 1; 
} 
  
struct TrieNode* removeK(struct TrieNode* root, char key[], int len, int depth) // Recursive function to delete a key from given Trie 
{ 
    if (!root) 
        return NULL; 
  
    // If last character of key is being processed 
    if (depth == len) { 
  
        // This node is no more end of word after 
        // removal of given key 
        if (!root->notEnd) 
            root->notEnd = 1; 
  
        // If given is not prefix of any other word 
        if (isEmpty(root)) {
        	free(root->CIDs); 
            free(root); 
            root = NULL; 
        } 
  
        return root; 
    } 
  
    // If not last character, recur for the child 
    // obtained using ASCII value 
    int index = charToInd(key[depth]); 
    root->children[index] =  
          removeK(root->children[index], key, len,depth + 1); 
  
    // If root does not have any child (its only child got  
    // deleted), and it is not end of another word. 
    if (isEmpty(root) && root->notEnd) { 
    	free(root->CIDs);
        free(root); 
        root = NULL; 
    } 
  
    return root; 
}

int* copyFile(struct TrieNode* root,char cpsrc[],char cpdest[],int *chunkIDs,int *assigned,int qid3,int qid2,int pidClient,int N) 
{
	int i=0,index;
	struct TrieNode *pCrawl = root;

	while(cpsrc[i]!='\0')
	{
		index=charToInd(cpsrc[i]);
		pCrawl = pCrawl->children[index];
		i++;
	}

	struct mtoc cpStat;
	cpStat.mtype=pidClient;
	cpStat.statFor=3;
	cpStat.status=0;

	//procedure to find where the chunks are deleted and store them according to data server ID aka type
	struct mtod cpChunks[N];
	int indicesArr[N];
	int noOfD[N]; //marks which d servers have atleast one chunk
	for(int j=0;j<N;j++) //initialisation
	{
		cpChunks[j].mtype=j+1;
		cpChunks[j].op=1;
		cpChunks[j].pid=pidClient;
		cpChunks[j].size1=0;
		cpChunks[j].size2=0;
		indicesArr[j]=0;
		noOfD[j]=0;
	} 

	for(int j=0;j<pCrawl->size;j++)
	{
		(*assigned)++;
		chunkIDs = (int*) realloc(chunkIDs,sizeof(int)*(*assigned));
		int new_cid = genCID(chunkIDs,*assigned-1); //generate new cid for the copy of chunk
		chunkIDs[*assigned-1] = new_cid;
		addCID(root,cpdest,new_cid,pCrawl->CIDs[j].d1,pCrawl->CIDs[j].d2,pCrawl->CIDs[j].d3); //adds this copy to fileH

		int ds1 = pCrawl->CIDs[j].d1-1;
		noOfD[ds1]=1;
		cpChunks[ds1].chunks1[indicesArr[ds1]]=pCrawl->CIDs[j].CID;
		cpChunks[ds1].chunks2[indicesArr[ds1]]=new_cid;
		cpChunks[ds1].size1++;
		cpChunks[ds1].size2++;
		indicesArr[ds1]++;
		if(cpChunks[ds1].size1==99) //if message capacity reached, send message to D and reset buffers
		{
			if(msgsnd(qid3,&cpChunks[ds1],sizeof(cpChunks[ds1])-sizeof(long),0)==-1)
			{
				perror("msgsnd");
				exit(1);
			}
			indicesArr[ds1]=0;
			cpChunks[ds1].size1=0;
			cpChunks[ds1].size2=0;
		}

		int ds2 = pCrawl->CIDs[j].d2-1;
		noOfD[ds2]=1;
		cpChunks[ds2].chunks1[indicesArr[ds2]]=pCrawl->CIDs[j].CID;
		cpChunks[ds2].chunks2[indicesArr[ds2]]=new_cid;
		cpChunks[ds2].size1++;
		cpChunks[ds2].size2++;
		indicesArr[ds2]++;
		if(cpChunks[ds2].size1==99) //if message capacity reached, send message to D and reset buffers
		{
			if(msgsnd(qid3,&cpChunks[ds2],sizeof(cpChunks[ds2])-sizeof(long),0)==-1)
			{
				perror("msgsnd");
				exit(1);
			}
			indicesArr[ds2]=0;
			cpChunks[ds2].size1=0;
			cpChunks[ds2].size2=0;
		}

		int ds3 = pCrawl->CIDs[j].d3-1;
		noOfD[ds3]=1;
		cpChunks[ds3].chunks1[indicesArr[ds3]]=pCrawl->CIDs[j].CID;
		cpChunks[ds3].chunks2[indicesArr[ds3]]=new_cid;
		cpChunks[ds3].size1++;
		cpChunks[ds3].size2++;
		indicesArr[ds3]++;
		if(cpChunks[ds3].size1==99) //if message capacity reached, send message to D and reset buffers
		{
			if(msgsnd(qid3,&cpChunks[ds3],sizeof(cpChunks[ds3])-sizeof(long),0)==-1)
			{
				perror("msgsnd");
				exit(1);
			}
			indicesArr[ds3]=0;
			cpChunks[ds3].size1=0;
			cpChunks[ds3].size2=0;
		}
	}

	int count=0; //to store number of responses C should receive from D
	for(int j=0;j<N;j++) if(noOfD[j]==1) count++;
		cpStat.size=count;
	if(msgsnd(qid2,&cpStat,sizeof(cpStat)-sizeof(long),0)==-1) //send status to C
	{
		perror("msgsnd");
		exit(1);
	}

	for(int j=0;j<N;j++) 
	{
		if(noOfD[j]==0) continue;
		if(msgsnd(qid3,&cpChunks[j],sizeof(cpChunks[j])-sizeof(long),0)==-1)
		{
			perror("msgsnd");
			exit(1);
		}
		cpChunks[j].size1=-1;
		cpChunks[j].size2=-1;
		
		if(msgsnd(qid3,&cpChunks[j],sizeof(cpChunks[j])-sizeof(long),0)==-1) //to let D server j+1 know that copying operation is over
		{
			perror("msgsnd");
			exit(1);
		}	

	}

	return chunkIDs;
}

int moveFile(struct TrieNode **root,char mvsrc[],char mvdst[]) //simply modifies file hierarchy
{
	int i=0,index,len;
	struct TrieNode *pCrawl = *root;

	if(root==NULL||*root==NULL) return 1;

	while(mvsrc[i]!='\0')
	{
		index=charToInd(mvsrc[i]);
		if(!pCrawl->children[index]) return 1;
		pCrawl = pCrawl->children[index];
		i++;
	}
	len=i;
	if(pCrawl->notEnd) return 1;
	int size = pCrawl->size;
	struct chunkInfo CIDs[size];
	for(int k=0;k<size;k++)
	{
		CIDs[k]=pCrawl->CIDs[k];
	}	
	*root=removeK(*root,mvsrc,len,0); //deletes mvsrc key
	if(*root==NULL) *root = getNode(); //do not allow root to be null
	if(insert(*root,mvdst)) 
	{
		//dst not valid so return to previous state
		insert(*root,mvsrc);
		for(int k=0;k<size;k++)
		{
			addCID(*root,mvsrc,CIDs[k].CID,CIDs[k].d1,CIDs[k].d2,CIDs[k].d3);
		}
		return 2; 
	}
	
	for(int k=0;k<size;k++)
	{
		addCID(*root,mvdst,CIDs[k].CID,CIDs[k].d1,CIDs[k].d2,CIDs[k].d3);	
	}
	return 0;	
}


int checkValidPath(char filep[])
{
	int i=0;
	if(filep==NULL||filep[0]!='/') return 0; //full file path has to start with /
	while(filep[i]!='\0')
	{
		if(!((filep[i]<='Z'&&filep[i]>='A')||(filep[i]<='z'&&filep[i]>='a')||(filep[i]=='/'||filep[i]=='.')||(filep[i]<='9'&&filep[i]>='0')))
			return 0;
		i++;
	}
	if(filep[i-1]=='/') return 0; //if file name ends in / implying it is a folder, it is not valid
	return 1;
}


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
		printf("Invalid value of N (Enter a value >=3 and divisible by 3)\n"); //for simplicity
		exit(1);
	}

	struct TrieNode *fileH = getNode(); //structure to hold file hierarchy
	int initSize=5,assigned=0;
	int *chunkIDs = (int*)malloc(sizeof(int)*initSize);
	//Create or open message queues 1 (C to M), 2 (M to C) and 6 (D to M)
	key_t key1,key2,key3;
	int qid1,qid2,qid3;
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
	if((key3=ftok(path3,3))==-1)
	{
		perror("ftok");
		exit(1);
	}
	//C to M queue
	if((qid1=msgget(key1,IPC_CREAT|0666))==-1)
	{
		perror("msgget");
		exit(1);
	}
	//M to C queue
	if((qid2=msgget(key2,IPC_CREAT|0666))==-1)
	{
		perror("msgget");
		exit(1);
	}
	//M to D queue
	if((qid3=msgget(key3,IPC_CREAT|0666))==-1)
	{
		perror("msgget");
		exit(1);
	}
	

	struct ctom clientM;
	int pidClient=-1;
	while(1){
		if(msgrcv(qid1,&clientM,sizeof(clientM)-sizeof(long),0,0)==-1)
		{
			perror("msgrcv");
			exit(1);
		}
		pidClient=clientM.pid;

		switch(clientM.mtype)
		{
			case 1: //Modify file hierarchy
			{
				struct mtoc sendStat;
				sendStat.mtype=pidClient;
				sendStat.statFor=1;
				if(!checkValidPath(clientM.forc))
				{
					sendStat.status=1; //path not valid
					if(msgsnd(qid2,&sendStat,sizeof(sendStat)-sizeof(long),0)==-1)
					{
						perror("msgsnd");
						exit(1);
					}
					break;
				}
				if(fileH==NULL) fileH=getNode();
				int ret;
				if(ret=insert(fileH,clientM.forc))
				{
					sendStat.status=ret; //file path exists or is invalid
					if(msgsnd(qid2,&sendStat,sizeof(sendStat)-sizeof(long),0)==-1)
					{
						perror("msgsnd");
						exit(1);
					}
					break;
				}
				else
				{
					sendStat.status=0; //success
					if(msgsnd(qid2,&sendStat,sizeof(sendStat)-sizeof(long),0)==-1)
					{
						perror("msgsnd");
						exit(1);
					}
					break;
				}
			}
			break;
			case 2: //add chunk
			{
				struct mtoc dInfo;
				dInfo.mtype=pidClient;
				dInfo.statFor=2;

				//any 3 of N addresses of D servers can be assigned here
				srand(time(0));
				int r = 3*(rand()%(N/3));
				r++;
				dInfo.d1=r;
				dInfo.d2=r+1;
				dInfo.d3=r+2;

				assigned++;
				if(assigned>initSize)
					chunkIDs = (int*) realloc(chunkIDs,sizeof(int)*assigned);
				chunkIDs[assigned-1] = genCID(chunkIDs,assigned-1);
				dInfo.CID = chunkIDs[assigned-1];
				//modify file hierachy to store chunk ID of corresponding file
				addCID(fileH,clientM.forc,chunkIDs[assigned-1],dInfo.d1,dInfo.d2,dInfo.d3);
				if(msgsnd(qid2,&dInfo,sizeof(dInfo)-sizeof(long),0)==-1)
					{
						perror("msgsnd");
						exit(1);
					}
			}
			break;
			case 3: //copy
			{
				char cpsrc[100],cpdest[100];
				struct mtoc cpStat;
				cpStat.mtype=pidClient;
				cpStat.statFor=3;
				char *tokenCP,*srcf,*dstf;
				tokenCP=strtok(clientM.forc," ");
				if(tokenCP==NULL||strcmp(tokenCP,"cp")!=0)
				{
					cpStat.status=3; //invalid cmd
					if(msgsnd(qid2,&cpStat,sizeof(cpStat)-sizeof(long),0)==-1)
					{
						perror("msgsnd");
						exit(1);
					}
					break;
				}

				//cp source
				srcf = strtok(NULL," ");
				if(srcf==NULL)
				{
					cpStat.status=3; //invalid cmd
					if(msgsnd(qid2,&cpStat,sizeof(cpStat)-sizeof(long),0)==-1)
					{
						perror("msgsnd");
						exit(1);
					}
					break;
				}
				strcpy(cpsrc,srcf);
				if(checkValidPath(cpsrc)==0)
				{
					cpStat.status=2; //src path not valid
					if(msgsnd(qid2,&cpStat,sizeof(cpStat)-sizeof(long),0)==-1)
					{
						perror("msgsnd");
						exit(1);
					}
					break;
				}
				if(!exist(fileH,cpsrc))
				{
					cpStat.status=1; //src does not exist
					if(msgsnd(qid2,&cpStat,sizeof(cpStat)-sizeof(long),0)==-1)
					{
						perror("msgsnd");
						exit(1);
					}
					break;

				}

				//cp destination
				dstf = strtok(NULL," ");
				if(dstf==NULL)
				{
					cpStat.status=3; //cmd not valid
					if(msgsnd(qid2,&cpStat,sizeof(cpStat)-sizeof(long),0)==-1)
					{
						perror("msgsnd");
						exit(1);
					}
					break;
				}
				strcpy(cpdest,dstf);
				if(checkValidPath(cpdest)==0)
				{
					cpStat.status=2; //dst not valid
					if(msgsnd(qid2,&cpStat,sizeof(cpStat)-sizeof(long),0)==-1)
					{
						perror("msgsnd");
						exit(1);
					}
					break;
				}
				if(exist(fileH,cpdest))
				{
					cpStat.status=2; //dst already exists
					if(msgsnd(qid2,&cpStat,sizeof(cpStat)-sizeof(long),0)==-1)
					{
						perror("msgsnd");
						exit(1);
					}
					break;
				}

				int retval=0;
				if(retval=insert(fileH,cpdest))
				{
					cpStat.status=2; //src or dst not valid
					if(msgsnd(qid2,&cpStat,sizeof(cpStat)-sizeof(long),0)==-1)
					{
						perror("msgsnd");
						exit(1);
					}
					break;
				}

				chunkIDs=copyFile(fileH,cpsrc,cpdest,chunkIDs,&assigned,qid3,qid2,pidClient,N);
			}
			

			break;		

			case 4: //move
			{
				char mvsrc[100],mvdest[100];
				struct mtoc mvStat;
				mvStat.mtype=pidClient;
				mvStat.statFor=4;
				char *token,*src,*dst;
				token=strtok(clientM.forc," ");
				if(token==NULL||strcmp(token,"mv")!=0)
				{
					mvStat.status=3; //invalid cmd
					if(msgsnd(qid2,&mvStat,sizeof(mvStat)-sizeof(long),0)==-1)
					{
						perror("msgsnd");
						exit(1);
					}
					break;
				}

				//mv source
				src = strtok(NULL," ");
				if(src==NULL)
				{
					mvStat.status=3; //invalid cmd
					if(msgsnd(qid2,&mvStat,sizeof(mvStat)-sizeof(long),0)==-1)
					{
						perror("msgsnd");
						exit(1);
					}
					break;
				}
				strcpy(mvsrc,src);
				if(checkValidPath(mvsrc)==0)
				{
					mvStat.status=2; //src path not valid
					if(msgsnd(qid2,&mvStat,sizeof(mvStat)-sizeof(long),0)==-1)
					{
						perror("msgsnd");
						exit(1);
					}
					break;
				}

				//mv destination
				dst = strtok(NULL," ");
				if(dst==NULL)
				{
					mvStat.status=3; //cmd not valid
					if(msgsnd(qid2,&mvStat,sizeof(mvStat)-sizeof(long),0)==-1)
					{
						perror("msgsnd");
						exit(1);
					}
					break;
				}
				strcpy(mvdest,dst);
				if(checkValidPath(mvdest)==0)
				{
					mvStat.status=2; //dst path not valid
					if(msgsnd(qid2,&mvStat,sizeof(mvStat)-sizeof(long),0)==-1)
					{
						perror("msgsnd");
						exit(1);
					}
					break;
				}

				int res = moveFile(&fileH,mvsrc,mvdest); //willreturn appropriate status if src file exists or not, dst file exists or not, or success
				mvStat.status=res;
				if(msgsnd(qid2,&mvStat,sizeof(mvStat)-sizeof(long),0)==-1)
				{
					perror("msgsnd");
					exit(1);
				}
			}
			
			break;
			case 5: //remove
			{
					char rfile[100];
					struct mtoc rmStat;
					rmStat.mtype=pidClient;
					rmStat.statFor=5;
					char *tokenRM,*rf;
					tokenRM=strtok(clientM.forc," ");
					if(tokenRM==NULL||strcmp(tokenRM,"rm")!=0)
					{
						rmStat.status=3; //invalid cmd
						if(msgsnd(qid2,&rmStat,sizeof(rmStat)-sizeof(long),0)==-1)
						{
							perror("msgsnd");
							exit(1);
						}
						break;
					}

					//rm source
					rf = strtok(NULL," ");
					if(rf==NULL)
					{
						rmStat.status=3; //invalid cmd
						if(msgsnd(qid2,&rmStat,sizeof(rmStat)-sizeof(long),0)==-1)
						{
							perror("msgsnd");
							exit(1);
						}
						break;
					}
					strcpy(rfile,rf);
					if(checkValidPath(rfile)==0)
					{
						rmStat.status=2; //file path not valid
						if(msgsnd(qid2,&rmStat,sizeof(rmStat)-sizeof(long),0)==-1)
						{
							perror("msgsnd");
							exit(1);
						}
						break;
					}

					if(!exist(fileH,rfile))
					{
						rmStat.status=1; //file path does not exist
						if(msgsnd(qid2,&rmStat,sizeof(rmStat)-sizeof(long),0)==-1)
						{
							perror("msgsnd");
							exit(1);
						}
						break;
					}
					else
					{
						rmStat.status=0; //success
						if(msgsnd(qid2,&rmStat,sizeof(rmStat)-sizeof(long),0)==-1)
						{
							perror("msgsnd");
							exit(1);
						}
					}
					int size=0,len=0;
					while(rfile[len++]!='\0');
					len--;
					struct chunkInfo *CIDs = delUtil(fileH,rfile,&size);
					fileH=removeK(fileH,rfile,len,0);

					//same logic as copy file, except no message to indicate deletion is over is sent
					struct mtod delC[N];
					int indicesArr[N];
					for(int j=0;j<N;j++)
					{
						delC[j].mtype=j+1;
						delC[j].op=2;
						delC[j].size1=0;
						indicesArr[j]=0;
					} 
					for(int j=0;j<size;j++) 
					{
						int ds1 = CIDs[j].d1-1;
						delC[ds1].chunks1[indicesArr[ds1]++]=CIDs[j].CID;
						delC[ds1].size1++;
						if(delC[ds1].size1==99)
						{
							if(msgsnd(qid3,&delC[ds1],sizeof(delC[ds1])-sizeof(long),0)==-1)
							{
								perror("msgsnd");
								exit(1);
							}
							indicesArr[ds1]=0;
							delC[ds1].size1=0;
						}

						int ds2 = CIDs[j].d2-1;
						delC[ds2].chunks1[indicesArr[ds2]++]=CIDs[j].CID;
						delC[ds2].size1++;
						if(delC[ds2].size1==99)
						{
							if(msgsnd(qid3,&delC[ds2],sizeof(delC[ds2])-sizeof(long),0)==-1)
							{
								perror("msgsnd");
								exit(1);
							}
							indicesArr[ds2]=0;
							delC[ds2].size1=0;
						}

						int ds3 = CIDs[j].d3-1;
						delC[ds3].chunks1[indicesArr[ds3]++]=CIDs[j].CID;
						delC[ds3].size1++;
						if(delC[ds3].size1==99)
						{
							if(msgsnd(qid3,&delC[ds3],sizeof(delC[ds3])-sizeof(long),0)==-1)
							{
								perror("msgsnd");
								exit(1);
							}
							indicesArr[ds3]=0;
							delC[ds3].size1=0;
						}
					}

						for(int j=0;j<N;j++)
						{
							if(delC[j].size1==0) continue;
							if(msgsnd(qid3,&delC[j],sizeof(delC[j])-sizeof(long),0)==-1)
								{
									perror("msgsnd");
									exit(1);
								}

						}
						free(CIDs);

			}
							break;

				case 6: //send relevant chunk IDs
				{

					struct mtoc sendCIDs;
					sendCIDs.mtype=pidClient;
					sendCIDs.statFor=6;
				
					if(!checkValidPath(clientM.forc))
					{
						sendCIDs.status=1; //file path invalid
						if(msgsnd(qid2,&sendCIDs,sizeof(sendCIDs)-sizeof(long),0)==-1)
						{
							perror("msgsnd");
							exit(1);
						}
						break;
					}
					else if(!exist(fileH,clientM.forc))
					{
						sendCIDs.status=2; //file path does not exist
						if(msgsnd(qid2,&sendCIDs,sizeof(sendCIDs)-sizeof(long),0)==-1)
						{
							perror("msgsnd");
							exit(1);
						}
						break;

					}
					else
					{
						sendCIDs.status=0; //success
						sendCIDs.size=0;	

						int i=0,index;
						struct TrieNode *pCrawl = fileH;

						while(clientM.forc[i]!='\0')
						{
							index=charToInd(clientM.forc[i]);
							pCrawl = pCrawl->children[index];
							i++;
						}

						for(int j=0;j<pCrawl->size;j++)
						{
							if(j&&j%100==0) //if current msg already full ,send it and reset buffers
							{
								if(msgsnd(qid2,&sendCIDs,sizeof(sendCIDs)-sizeof(long),0)==-1)
								{
									perror("msgsnd");
									exit(1);
								}
								sendCIDs.size=0;
								
							}
							sendCIDs.CIDs[j%100] = pCrawl->CIDs[j];
							sendCIDs.size++;
						}

						if(msgsnd(qid2,&sendCIDs,sizeof(sendCIDs)-sizeof(long),0)==-1)
						{
							perror("msgsnd");
							exit(1);
						}
						sendCIDs.size=-1;
						if(msgsnd(qid2,&sendCIDs,sizeof(sendCIDs)-sizeof(long),0)==-1) //to let C know M has sent whole list of CIDs
						{
							perror("msgsnd");
							exit(1);
						}
						
					}
				}
		}	
	}
	

}