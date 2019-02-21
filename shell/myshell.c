/**
 * This module behaves shell-like, with SIGINT being handled and changed.
 * Supports background and foreground proccesses.
 * Mainly, zombie proccesses aren't created (SA_NOCLDWAIT is applied), and when they are created they are handled by their parent.
 **/ 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

static int contains_pipe(int count,char** list,int *location);
static int contains_ampersand(int count, char** list);
static int handle_pipe(int count,char** list, int location);
static int handle_ampersand(int count,char** list);
static int handle_flow(int count,char** list);
static int clean_zombie();

/**
 * prepare - changes the general behavior of SIGINT, SIGCHLD.
 * @return int - 1 if invalid, 0 if valid
 */
int prepare(void){
	struct sigaction sigint;
    struct sigaction sigchld;
	memset(&sigint,0,sizeof(sigint));
	sigint.sa_handler=SIG_IGN;
	sigint.sa_flags=SA_RESTART;
	if (sigaction(SIGINT,&sigint,NULL)!=0){
		fprintf(stderr,"%s\n",strerror(errno));
		return 1;
	}
    memset(&sigchld,0,sizeof(sigchld));
    sigchld.sa_flags=SA_NOCLDWAIT;
    sigchld.sa_handler=SIG_DFL;
    if (sigaction(SIGCHLD,&sigchld,NULL)!=0){
		fprintf(stderr,"%s\n",strerror(errno));
		return 1;
	}
    return 0;
}

int finalize(){
    return 0;
}

/**
 * process_arglist - handles the commands of the user and execute them
 * @param count - number of arguments including the name of the command
 * @param **arglist - the arguments including the command
 * @return int - 1 if valid, else 0
 */
int process_arglist(int count, char** arglist){
    if (waitpid(-1,NULL,WNOHANG)==-1 && errno!=ECHILD){ /*possibly a child that got SIGKILLED wasn't cleaned by his parent so needs to get cleaned now */
        fprintf(stderr,"%s\n",strerror(errno));
        return 0;
    }
    int location=0;
    if ((contains_pipe(count, arglist,&location))==1){
        return handle_pipe(count,arglist,location);
        
    }
    else if ((contains_ampersand(count, arglist))==1){
        return handle_ampersand(count,arglist);
    }
    else {  
        return handle_flow(count,arglist);
    }
}
static int clean_zombie(){
    int temp = wait(NULL); 
    if (temp==-1){
        fprintf(stderr,"%s\n",strerror(errno));
        return -1;
    }
    return 100;
}


static int handle_pipe(int count,char** list, int location){
    int fd[2];
    if (pipe(fd)==-1){
        fprintf(stderr,"%s\n",strerror(errno));
        return 0;
    }
    if ((signal(SIGCHLD,SIG_DFL))==SIG_ERR){ /* foreground proccess here, so return the SIGCHLD to default handler */
        fprintf(stderr,"%s\n",strerror(errno));
	    return 0;
	}
    pid_t pid = fork();
    if (pid>0){
        pid=fork();
        if (pid>0){
            if (close(fd[0])==-1|| close(fd[1])==-1 ) {fprintf(stderr,"%s\n",strerror(errno)); return 0; }
            if (clean_zombie()==-1){ /* clean the first zombie process */ 
                return 0;
            }
            if (clean_zombie()==-1){ /* clean the second zombie process */
                return 0;
            }
            struct sigaction sigchld; memset(&sigchld,0,sizeof(sigchld));
            sigchld.sa_flags=SA_NOCLDWAIT; sigchld.sa_handler=SIG_DFL;
            if (sigaction(SIGCHLD,&sigchld,NULL)!=0){ /* change back SIGCHLD  flag to SA_NOCHDWAIT */
		        fprintf(stderr,"%s\n",strerror(errno));
		        return 0;
	        }
            return 1;
        }
        else if (pid==0){
            if ((signal(SIGINT,SIG_DFL))==SIG_ERR){ /* should terminate upon SIGINT */
		        fprintf(stderr,"%s\n",strerror(errno));
		        _exit(1);
	        }
            if (close(fd[0])==-1) {fprintf(stderr,"%s\n",strerror(errno)); _exit(1);} 
            if ((dup2(fd[1],STDOUT_FILENO))==-1){
                fprintf(stderr,"%s\n",strerror(errno));
                _exit(1);
            }
            if (close(fd[1])==-1) {fprintf(stderr,"%s\n",strerror(errno)); _exit(1);}
            list[location]=NULL;
            if ((execvp(list[0],list)==-1)){
                fprintf(stderr,"%s\n",strerror(errno));
                _exit(1);
            }
        }
        else {
            fprintf(stderr,"%s\n",strerror(errno));
            return 0;
        }
    } 
    else if (pid==0){
        if ((signal(SIGINT,SIG_DFL))==SIG_ERR){ /* should terminate upon SIGINT */
		    fprintf(stderr,"%s\n",strerror(errno));
		    _exit(1);
	    }
        if (close(fd[1])==-1) {fprintf(stderr,"%s\n",strerror(errno)); _exit(1);}  
        if (dup2(fd[0],STDIN_FILENO)==-1){
            fprintf(stderr,"%s\n",strerror(errno));
            _exit(1);
        }
        if (close(fd[0])==-1) {fprintf(stderr,"%s\n",strerror(errno)); _exit(1);} 
        if ((execvp(list[location+1],list+location+1))==-1){ 
            fprintf(stderr,"%s\n",strerror(errno));
            _exit(1);
        }
    }  
    else{
        fprintf(stderr,"%s\n",strerror(errno));
        return 0;
    }
    return 1;
}
static int handle_ampersand(int count,char** list){
    pid_t pid = fork();
    if (pid>0){
        return 1;
    }
    else if (pid==0){
        list[count-1]=NULL;
        if ((execvp(list[0],list))==-1){
            fprintf(stderr,"%s\n",strerror(errno));
            _exit(1);
        }
    }
    else {
        fprintf(stderr,"%s\n",strerror(errno));
        return 0;
    }
    return 1;
}

static int handle_flow(int count,char** list){
    if ((signal(SIGCHLD,SIG_DFL))==SIG_ERR){
		fprintf(stderr,"%s\n",strerror(errno));
		return 0;
	}
    pid_t pid = fork();
    if (pid==0){
        if ((signal(SIGINT,SIG_DFL))==SIG_ERR){
		    fprintf(stderr,"%s\n",strerror(errno));
		    _exit(1);
	    }
        if ((execvp(list[0],list))==-1){
            fprintf(stderr,"%s\n",strerror(errno));
            _exit(1);
        }
    }
    else if (pid>0){
        if (waitpid(pid,NULL,0)==-1){
            fprintf(stderr,"%s\n",strerror(errno));
            _exit(1);
        }
        struct sigaction sigchld;
        memset(&sigchld,0,sizeof(sigchld));
        sigchld.sa_flags=SA_NOCLDWAIT; sigchld.sa_handler=SIG_DFL;
        if (sigaction(SIGCHLD,&sigchld,NULL)!=0){ /* change back SIGCHLD  flag to SA_NOCHDWAIT */
		    fprintf(stderr,"%s\n",strerror(errno));
		    return 0;
	    }
        return 1;
    }
    else {
        fprintf(stderr,"%s\n",strerror(errno));
        return 0;
    }
    return 1;
}

static int contains_pipe(int count,char** list,int *location){
    for (int i=1 ; i < count ; i++){
        if (strcmp(list[i],"|")==0){
            *location=i;
            return 1;
        }
    }
    return 0;
}
static int contains_ampersand(int count, char** list){
    if (strcmp(list[count-1],"&")==0){
        return 1;
    }
    else{
        return 0;
    }
}