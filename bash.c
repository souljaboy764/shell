#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <signal.h>
#include <malloc.h>

//Structure for maintaining job names and their process ID along with their job number
struct jobs
{
	pid_t pid;
	char *name;
};
typedef struct jobs jobs;
jobs job[1000000];
int njobs;//number of jobs running

int p2j[1000000];//hash table to get the job number corresponding to a process id
int flag;		//flag for checking the signal SIGTSTP
int wflag;

//function to remove a job with process id pid from the job queue
void job_remove(pid_t pid)
{
	int i=p2j[pid];
	p2j[pid]=-1;
	while(i<njobs)
	{
		job[i]=job[i+1];
		p2j[job[i].pid]--;
		i++;
	}
	njobs--;
}

//To send SIGTSTP to the latest process when Ctrl Z is pressed
void sig_handler1(int signo)
{
	if(njobs!=0)
	{	
		flag=1;
		kill(job[njobs-1].pid,SIGSTOP);
	}
	else if(signo==SIGTSTP)
	{
		sleep(0);
		printf("\n");
	}
}

//Signal Handling Function (reference: codes sent via email by ronak kogta sir)
void sig_handler(int signo)
{
	if (signo == SIGUSR1)
		sleep(0);
	else if (signo == SIGKILL)
		sleep(0);
	else if (signo == SIGSTOP)
		sleep(0);
	else if (signo == SIGINT)
		sleep(0);
	//Taking care of the termination of the child process	
	else if (signo == SIGCHLD && wflag!=0)
	{
		int status;
		pid_t pid=waitpid(-1,&status,WNOHANG);
		if(pid>0)
		{
			int i=p2j[pid];
			printf("\n%s with pid:%d terminated\n",job[i].name,pid);
			//updating the job queue by removing the terminated child process from it when it dies
			job_remove(pid);
		}
	}
}
char *user,*name,*cwd,*home,*home_rel;

//Function to print the username@hostname prompt
void PS1()
{
	gethostname(name,sizeof(name));	//getting host name
	user=getlogin();		//getting username
	getcwd(cwd,1000);		//getting current working directory
	if(strcmp(cwd,home)==0)
		strcpy(cwd,"~");	//replacing cwd with ~ if it is home directory
	else if((strncmp(cwd,home,sizeof(home)-1)==0)&& (strlen(home)<strlen(cwd)))	//checking if the path is within the home directory
	{
		//replacing th absolute path with path relative to the home directory
		home_rel=cwd+strlen(home);
		strcpy(cwd,"~");
		cwd=strcat(cwd,home_rel);
	}
	printf("<%s@%s:%s>",user,name,cwd);
}


// function to remove any extra white spaces from the command 
char* no_white(char str[])
{
	char *dst = str;
	int i,j=0;
	for (i=0;i<strlen(str);i++) 
	{
		if (isspace(str[i]) || str[i]=='\t') 
		{
			do 
				i++; 
			while (isspace(str[i]) || str[i]=='\t');
			--i;
		}
		dst[j++] = str[i];
	}
	dst[j]='\0';
	while(isspace(*dst) || *dst=='\t') dst++;
	return dst;
}


//function to split a string s according to the delimiters del and return an array of strings after splitting
int split(char *s,char *del, char *ret[])
{
	char *tmp;
	int i=0;
	tmp=strtok(s,del);
	while(tmp!=NULL)
	{
		ret[i++]=tmp;
		tmp=strtok(NULL,del);
	}
	return i;	//returning the number of elements in ret
}

//function to get the input file if not stdin
char* getin(char* cmd)
{
	int i;
	char in[1000];
	for(i=0;cmd[i]!='\0';i++)
		if(cmd[i]=='<')
		{
			while(isspace(cmd[i+1]))
				i++;
			break;
		}
	if(i==strlen(cmd))
		return "\0";
	sscanf(cmd+1+i,"%[^> \n]",in);
	return in;
}	

//function to get the output file if not stdin
char* getout(char* cmd)
{
	int i;
	char op[1000];
	for(i=0;cmd[i]!='\0';i++)
		if(cmd[i]=='>')
		{
			while(isspace(cmd[i+1]))
				i++;
			break;
		}
	if(i==strlen(cmd))
		return "\0";
	sscanf(cmd+1+i,"%[^< \n]",op);
	return op;
}

//function to get a file present in /proc/pid
char* getpath(char *src,int pid,char *file)
{
	strcpy(src,"/proc/");
	sprintf(src+6, "%d", pid);
	src=strcat(src,file);
	return src;
}

//Functio to execute piped commands
void piped(char *cmd)
{
	int status,ncom,background,i,in,out,n,j,k;
	pid_t pid,pid1;
	char *ipfile=malloc(sizeof(char));
	char *opfile=malloc(sizeof(char));
	char *cmd1=malloc(sizeof(char)); 
	char *com[1000],*par[1000],cmd2[1000];
	background=0;
	for(i=strlen(cmd)-1;i>=0;i--)
		if(cmd[i]=='&')
		{
			background=1;
			break;
		}
	ncom=split(cmd,"|",com);	//splitting the piped command to get each individual command	
	int pipefd[2*(ncom-1)];		
	for(i=0;i<ncom-1;i++)
		pipe(pipefd+2*i);	//Pipe creation
	for(i=0;i<ncom;i++)
	{
		ipfile=malloc(sizeof(char));
		opfile=malloc(sizeof(char));
		strcpy(ipfile,getin(com[i]));
		strcpy(opfile,getout(com[i]));
		sscanf(com[i],"\n%[^<>]",cmd2);
		strcpy(com[i],cmd2);
		cmd1=no_white(com[i]);
		n=split(com[i]," \t\n",par);
		//checking for background process
		if(strcmp(par[n-1],"&")==0)
		{
			n--;
			par[n]=NULL;
		}
		else if(par[n-1][strlen(par[n-1])]=='&')
			par[n-1][strlen(par[n-1])-1]='\0';
		else
			par[n]=NULL;
		pid=fork();	//creating a child
		if(pid<0)
			printf("Fork unsuccessful\n");
		else if(pid==0)	//If child process
		{
			if(i!=ncom-1)			//creating output pipe for all commands except the last one
				dup2(pipefd[2*i+1],1);
			if(i!=0)			//creating input pipes for all commands except the first one
				dup2(pipefd[2*(i-1)],0);
			for(j=0;j<2*(ncom-1);j++)
				close(pipefd[j]);
			// if input is to be read from a redirected file
			if(strcmp(ipfile,"\0"))
			{
				if(access(ipfile,F_OK)!=0)
				{
					printf("File %s doesn't exist\n",ipfile);
					continue;
				}
				else if(access(ipfile,R_OK)!=0)
				{
					printf("You dont have permission to access %s\n",ipfile);
					continue;
				}
				else
				{
					in=open(ipfile,O_RDONLY,0600);
					if(in<0)
						printf("1 File error\n");
					else if(dup2(in,0) < 0)
						printf("2 File error\n");
					close(in);
				}
			}
			// if output is to be printed to a redirected file
			if(strcmp(opfile,"\0"))
			{
				if(access(opfile,F_OK)==0 && access(opfile,W_OK)!=0)
				{
					printf("You dont have permission to access %s\n",opfile);
					continue;
				}
				else
				{
					if(access(opfile,F_OK))
						creat(opfile,0600);
					out=open(opfile,O_WRONLY,0600);
					if(out<0)
						printf("1 File error\n");
					if(dup2(out,1)<0)
						printf("2 File error\n");
					close(out);
				}
			}
			execvp(par[0],par);
			_exit(1);	// exiting if exec() wasnt successful
		}
		else if (pid>0)	//If parent process
		{
			//adding the latest child created to the job queue
			job[njobs].pid=pid;
			sprintf(cmd1,par[0]);
			cmd1[strlen(par[0])]='\0';
			for(j=1;j<n;j++)
			{
				sprintf(cmd1+strlen(cmd1)," %s\0",par[j]);
			}
			job[njobs].name=(char *)malloc(sizeof(char));
			strcpy(job[njobs].name,cmd1);
			njobs++;
			p2j[pid]=njobs-1;
			wflag=1;
			//checking for a foreground process				
			if(background!=1)
			{
				wflag=0;
				flag=0;
				//checking if the process was terminated or stopped by SIGTSTP signal
				if(flag!=1)
				{
					//removing the job from the job queue if it was terminated
					job_remove(pid);
				}
			}
		}
		ipfile=NULL;
		opfile=NULL;
	}
	for(j=0;j<2*(ncom-1);j++)
		close(pipefd[j]);
	if(background==0)
		for(j=0;j<ncom;j++)
			waitpid(0,0,0);
}

int main()
{
	write(1,"\033[2J\033[0;0H",sizeof("\033[2J\033[0;0H")); //clearing the screen
	setenv("HOME",getenv("PWD"),1);	//setting the cwd to home
	home=getenv("HOME");
	memset(p2j,-1,100000);
	njobs=0;
	int piper;
	char *file=malloc(sizeof(char));
	char *ipfile;
	char *opfile;
	int in,out;
	pid_t pid,pid1;
	char *par[1000],exe[10000],pth[1000],*com[1000],cmd2[1000];
	char *cmd=(char *)malloc(sizeof(char));
	char *cmd1=(char *)malloc(sizeof(char));
	char* mem=malloc(sizeof(char));
	char *stat=malloc(sizeof(char));
	char *str=malloc(sizeof(char));
	char *tmp=malloc(sizeof(char));
	char *path=malloc(sizeof(char));
	char *home_rel=malloc(sizeof(char));
	name=(char *)malloc(sizeof(char));
	cwd=(char *)malloc(sizeof(char));
	int i,n,j,background,sig,jno,status,ncom;
	//signal handling
	if (signal(SIGUSR1, sig_handler) == SIG_ERR)
		sleep(0);
	if (signal(SIGKILL, sig_handler) == SIG_ERR)
		sleep(0);
	if (signal(SIGSTOP, sig_handler) == SIG_ERR)
		sleep(0);
	if (signal(SIGINT, sig_handler) == SIG_ERR)
		sleep(0);
	if (signal(SIGCHLD, sig_handler) == SIG_ERR)
		sleep(0);
	signal(SIGTSTP,SIG_IGN);
	if (signal(SIGTSTP, sig_handler1))
	{;}

	while(strcmp(cmd,"quit")!=0)
	{

		wflag=1;
		PS1();	//printing the prompt
		i=0;
		background=0;
		while((cmd[i++]=getchar())!='\n');
		cmd[i]='\0';
		if(i==1)
			continue;
		//Checking if a given command is a piped command
		piper=0;
		for(i=0;cmd[i]!='\0';i++)	
			if(cmd[i]=='|')
			{
				piper=1;
				break;
			}
		if(piper==1)
		{
			piped(cmd);	//function to execute piped commands
			continue;
		}
		ipfile=malloc(sizeof(char));
		opfile=malloc(sizeof(char));
		strcpy(ipfile,getin(cmd));
		strcpy(opfile,getout(cmd));
		sscanf(cmd,"\n%[^<>]",cmd2);
		strcpy(cmd,cmd2);
		cmd1=no_white(cmd);
		n=split(cmd," \t\n",par);
		if(strcmp(par[n-1],"&")==0)
		{
			n--;
			par[n]=NULL;
			background=1;
		}
		else if(par[n-1][strlen(par[n-1])]=='&')
		{
			par[n-1][strlen(par[n-1])-1]='\0';
			background=1;
		}
		else
		{
			par[n]=NULL;
			background=0;
		}

		//CD
		if(strcmp(par[0],"cd")==0)
		{
			//if only cd is typed without arguments then changing target to home directory
			if(n==1)
				par[1]=home;
			else if(par[1]==NULL || strcmp(par[1],"~")==0)
				par[1]=home;
			//getting the absolute path of the argument
			else if(par[1][0]!='/')
			{
				tmp=getcwd(cwd,1000);
				strcat(tmp,"/");
				strcat(tmp,par[1]);
				strcpy(par[1],tmp);

			}
			else if(par[1][0]=='~')
			{
				strcpy(tmp,home);
				strcat(tmp,"/");
				strcat(tmp,par[1]+1);
				strcpy(par[1],tmp);
			}

			//checking if the given folder exists and if we have permission to access it or not
			if(access(par[1],F_OK&X_OK)!=0)
				printf("Invalid Access\n");
			else
				chdir(par[1]);
		}

		//JOBS	
		else if(strcmp(par[0],"jobs")==0)
		{
			if(njobs>0)
				for(i=0;i<njobs;i++)
					printf("%d. %s %d\n",p2j[job[i].pid]+1,job[i].name,job[i].pid);
			else printf("No background process running\n");
		}

		//KJOB
		else if(strcmp(par[0],"kjob")==0)
		{
			//if the user doesn't enter the command properly
			if(n!=3)
				printf("Usage kjob <job_number> <signal_number>\n");
			else
			{
				sig=atoi(par[2]);
				jno=atoi(par[1])-1;
				if(jno>=njobs)	//if the job specified is greater than the number of jobs runing
					printf("The job doesn't exist\n");
				else
					kill(job[jno].pid,sig);
			}
		}

		//OVERKILL
		else if(strcmp(par[0],"overkill")==0)
		{
			while(njobs>0)
				kill(job[0].pid,9);	// signal handler for SIGCHLD handles the updation of the job queue
		}

		//PINFO
		else if(strcmp(par[0],"pinfo")==0)
		{
			pid1=(n==1)?getpid():atoi(par[1]);	//getting the pid of the process whose info is to be displayed
			path=getpath(path,pid1,"/status");	//getting path to /proc/pid/status file
			tmp=getpath(tmp,pid1,"/exe");		//getting path to /proc/pid/exe file
			if(access(path,F_OK&X_OK)!=0 || access(tmp,F_OK&X_OK)!=0)	//checking if the process exists or not
				printf("The specified Process doesn't exist\n");
			else
			{
				FILE* f=fopen(path,"r");
				memset(exe, 0, sizeof(exe));
				readlink(tmp,exe,sizeof(exe));		//reading the path of the executable for the given process
				while(fscanf(f, "\n%[^\n]", str)==1)	//reading one line a a time
				{
					if(strncmp(str,"State:",6)==0)	//getting the current state
						sscanf(str+7,"%s",stat);
					if(strncmp(str,"VmSize:",7)==0)	//getting the size of virtual memory occupied
					{
						sscanf(str+8,"%s",mem);
						break;			//breaking as virtual memory size is the last information we need
					}
				}
				//getting the relative path of the executable file w.r.t the home directory if it is within the home directory
				if(strncmp(exe,home,strlen(home))==0)	
				{
					home_rel=exe+strlen(home);
					strcpy(exe,"~");
					strcat(exe,home_rel);
				}
				printf("Pid: %d\nProcess Status: %s\nMemory: %s\nExecutable Path: %s\n",pid1,stat,mem,exe);
				fclose(f);
			}
		}

		//FG
		else if(strcmp(par[0],"fg")==0)
		{
			if(n!=2)	//if no job number is given
				printf("Usage fg <job_number>\n");
			else
			{
				i=atoi(par[1])-1;
				if(i>=njobs)	//if an invalid job number is given
					printf("The job doesn't exist\n");
				else
				{
					pid1=job[i].pid;	// getting the pid of the process
					flag=0;			
					kill(pid1,SIGCONT);	//sending a signal to the process to continue
					waitpid(pid1,&status,WUNTRACED);	// waiting for process to complete 
					if(flag!=1)				// checking if the process terminated or stopped by user
					{
						//updating the jobs queue if the process was terminated and not stopped
						job_remove(pid1);
					}
				}
			}
		}

		//FOR OTHER PROCESSES
		else
		{
			pid=fork();	//creating a child
			if(pid<0)
				printf("Fork unsuccessful\n");
			else if(pid==0)	//If child process
			{
				if(strcmp(ipfile,"\0"))
				{
					if(access(ipfile,F_OK)!=0)
					{
						printf("File %s doesn't exist\n",ipfile);
						continue;
					}
					else if(access(ipfile,R_OK)!=0)
					{
						printf("You dont have permission to access %s\n",ipfile);
						continue;
					}
					else
					{
						in=open(ipfile,O_RDONLY,0600);
						if(in<0)
							printf("1 File error\n");
						else if(dup2(in,0) < 0)
							printf("2 File error\n");
						close(in);
					}
				}
				if(strcmp(opfile,"\0"))
				{
					if(access(opfile,F_OK)==0 && access(opfile,W_OK)!=0)
					{
						printf("You dont have permission to access %s\n",opfile);
						continue;
					}
					else
					{
						if(access(opfile,F_OK))
							creat(opfile,0600);
						out=open(opfile,O_WRONLY,0600);
						if(out<0)
							printf("1 File error\n");
						if(dup2(out,1)<0)
							printf("2 File error\n");
						close(out);
					}
				}
				execvp(par[0],par);
				_exit(1);	// exiting if exec() wasnt successful
			}
			else if (pid>0)	//If parent process
			{
				//adding the latest child created to the job queue
				job[njobs].pid=pid;
				sprintf(cmd1,par[0]);
				cmd1[strlen(par[0])]='\0';
				for(j=1;j<n;j++)
				{
					sprintf(cmd1+strlen(cmd1)," %s\0",par[j]);
				}
				job[njobs].name=(char *)malloc(sizeof(char));
				strcpy(job[njobs].name,cmd1);
				njobs++;
				p2j[pid]=njobs-1;
				//checking for a foreground process				
				if(background!=1)
				{
					flag=0;
					waitpid(pid,&status,WUNTRACED);
					//checking if the process was terminated or stopped by SIGTSTP signal
					if(flag!=1)
					{
						//removing the job from the job queue if it was terminated
						job_remove(pid);
					}
				}
			}

		}
		ipfile=NULL;
		opfile=NULL;
	}
	return 0;
}
