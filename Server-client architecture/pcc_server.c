/*
 * pcc_server summary:
 * 
 *  
 * The server computes the amount of printable character that was sent from a certain client and send back the answer to the client. Furthermore, it increments and save each 
 * character occurrence and print it out when the user signal SIGINT to the server.
 *
 * 
 * 
 ***Signal handling: Upon receiving SIGINT, the program terminates and prints the amount of printable character per character.
 ***                 Upon recieving SIGPIPE, the program prints an error to stderr and continues handling other clients.
 */


#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>

#define OFFSET 32
#define BUFFER_SIZE 256

int recv_and_analyze_info(int pcc_total[95], int listenfd,char* buffer, int len,unsigned int* counter);
void analyze_data(int pcc_total[95], char* buffer,int bytes_read,unsigned int* counter);
int send_analyzed_info(int connfd,int counter);
void sum_up_and_exit();
int change_sigint();
void signal_handler(int signum, siginfo_t* info, void* ptr);

static int accepted=0;
static int pcc_total[95] ={0};
static int SIGINT_INVOKED=0;

/*
* main - Initiate connections, recieve, analyze and send back information to the client
*/
int main(int argc, char** argv){
    int listenfd   = -1;
    int connfd     = -1;

    struct sockaddr_in serv_addr;
    struct sockaddr_in peer_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in);
    unsigned int port = (unsigned int) strtoul(argv[1], NULL, 10);

    char buffer[BUFFER_SIZE];
    /********** Init **********/
    listenfd = socket( AF_INET, SOCK_STREAM, 0 );
    if (listenfd==-1){
        fprintf(stderr, "%s\n", strerror(errno));
        return 1;
    }
    memset( &serv_addr, 0, addrsize);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);


    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR ) {
        fprintf(stderr, "Unable to disable SIGPIPE signal \n");
        return 1;
    }

    if( 0 != bind( listenfd, (struct sockaddr*) &serv_addr, addrsize ) )
    {
        fprintf(stderr, "%s\n", strerror(errno));
        return 1;
    }
    if( 0 != listen( listenfd, 10 ) )
    {
        fprintf(stderr, "%s\n", strerror(errno));
        return 1;
    }
    /***********************/
    if ( change_sigint()== -1 ) {
        return 1;
    }
    while (1){
        accepted=0;
        connfd = accept( listenfd, (struct sockaddr*) &peer_addr, &addrsize);
        accepted=1;
        if( connfd < 0 )
        {
            fprintf(stderr, "%s\n", strerror(errno)); 
            close(connfd);
            continue;
        }
        unsigned int counter=0;
        int ret_val = recv_and_analyze_info(pcc_total,connfd, buffer, BUFFER_SIZE, &counter); 
        if (ret_val==-1){
            fprintf(stderr, "An error occurred (perhaps client unexpectedly closed connection): %s\n", strerror(errno));
            close(connfd);
            continue;
        }
        else { /* send to the user amount of printable characters */
            if ( send_analyzed_info(connfd,counter) ==-1 ){
                fprintf(stderr, "An error occurred (perhaps client unexpectedly closed connection): %s\n", strerror(errno));
                close(connfd);
                continue; 
            }
 
        }
        close(connfd);
        if (SIGINT_INVOKED){ /* if the server is in the middle of processing a client, it will finish and enter this condition */
            sum_up_and_exit();
        } 
    }
}

/**
 * recv_and_analyze_info - recieve information from a client and analyze it
 * @param *pcc_total - the data structure that holds the analyzed information
 * @param connfd - the file descriptor of the tcp connection
 * @param *buffer - the buffer that the information is being written to
 * @param len - buffers size
 * @param *counter - amount of printable characters recieved from the client
 * @return int - 0 if successfull, else -1
 */
int recv_and_analyze_info(int pcc_total[95], int connfd,char* buffer, int len, unsigned int* counter){
        int bytes_read;
        while (1){
            bytes_read = recv(connfd,buffer,len, 0);
            if (bytes_read==-1){
                return -1;
            }
            if (bytes_read==0) break;
            analyze_data(pcc_total,buffer,bytes_read, counter);
        }
        return 0;

}

/**
 * analyze_data - analyze the information
 * @param *pcc_total - the data structure that holds the analyzed information
 * @param *buffer - the buffer that the information is being written to
 * @param bytes_read - amount of bytes that currently recieved from client
 * @param *counter - amount of printable characters recieved from the client
 * @return void
 */
void analyze_data(int pcc_total[95], char* buffer,int bytes_read, unsigned int* counter){
    int i;
    for (i=0 ; i < bytes_read ; i++){
        char value = buffer[i];
        if (value>126 || value < 32){
            continue;
        }
        else{
            pcc_total[value - OFFSET]++;
            *counter = *counter + 1;
        }
    }
}

/**
 * send_analyzed_info - send the analyzed information back to the client
 * @param connfd - the file descriptor of the tcp connection
 * @param counter - amount of printable characters recieved from the client
 * @return int - 0 if successfull, else -1
 */
int send_analyzed_info(int connfd,int counter){
        int length = snprintf(NULL,0, "%d", counter);
        char* transmitted_data = (char*)malloc(length + 1);
        if (transmitted_data==NULL){
            return -1;
        }
        snprintf(transmitted_data, length+1,"%d", counter);
        int val;
        if ((val=send(connfd, transmitted_data , length, 0)) < 1 ) { 
            free(transmitted_data);
            return -1;
        }
        if (send(connfd, NULL, 0, 0) ==-1 ){ /* checking if client unexpectedly closed the connection */
            free(transmitted_data);
            return -1;
        }
        free(transmitted_data);
        return 0;
}

/**
 * signal_handler -  If recieved SIGINT while processing a client, wait untill finishing and then terminates. Otherwise immediatly exits.
 * This function arguments are determinted by struct sigaction and are not used in this module
 */
void signal_handler(int signum, siginfo_t* info, void* ptr){ 
    if (accepted==0){
        sum_up_and_exit();
    }
    else { /* in the middile of processing a client */
        SIGINT_INVOKED=1;
    }
}

/**
 * change_sigint - Registers the handler for SIGINT
 * @return void
 */
int change_sigint(){
    struct sigaction sigint;
    memset(&sigint, 0 , sizeof(sigint));
    sigint.sa_sigaction = signal_handler;
    if (0!= sigaction(SIGINT, &sigint, NULL)){
        printf("%s\n",strerror(errno));
        return -1;
    }
    return 0;
}

/**
 * sum_up_and_exit -  prints the information collected to stdout and exits program
 * @return void
 */
void sum_up_and_exit(){
    int i;
    for (i=0 ; i < 95 ; i++){
        printf("char '%c' : %u times\n", i+OFFSET,pcc_total[i]);
    }
    exit(0);
}