/*
 * pcc_client summary:
 *  
 * The client reads N characters from /dev/urandom and sends it to the server to get analyzed. The server send back the number of printable character that was sent.
 *
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
#include <fcntl.h>

#define BUFFER_SIZE 34
#define READ_CHUNK 3355442
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int read_and_send_length_bytes_from_file(int sockfd, unsigned int len);

char big_buffer[READ_CHUNK] = {'\0'};

/*
* main - Initiate connections, read N bytes from the file, send the information to the server and prints back the answer
*/
int main(int argc, char** argv){
    int  sockfd     = -1;
    int  bytes_read =  0;
    struct sockaddr_in serv_addr;

    
    char buffer[BUFFER_SIZE] = {'\0'};
    unsigned int length = (unsigned int) strtoul(argv[3], NULL, 10);
    if( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      fprintf(stderr, "%s\n", strerror(errno));
      return 1;
    }

    /* server setup */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    unsigned int port = (unsigned int) strtoul(argv[2], NULL, 10);
    serv_addr.sin_port = htons(port); 
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    

    if( connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    {
        fprintf(stderr, "%s\n", strerror(errno));
        return 1;
    }
    /***************/
    if (-1==read_and_send_length_bytes_from_file(sockfd, length)){
        return 1;
    }
    if (shutdown(sockfd, SHUT_WR) ==-1 ){
        fprintf(stderr, "%s\n", strerror(errno));
        return 1;
    }

    /* recieve the computation done by the server */
    bytes_read = recv(sockfd,buffer ,BUFFER_SIZE ,0);
    if( bytes_read < 0 ){
        fprintf(stderr, "%s\n", strerror(errno));
        return 1;
    }
    unsigned int C = (unsigned int) strtoul(buffer, NULL, 10);
    printf("# of printable characters: %u\n", C);
    close(sockfd);
    return 0;
}

/**
 * read_and_send_length_bytes_from_file - reads the right amount of bytes from the file and sends it to the server
 * @param sockfd - the file descriptor of the tcp connection
 * @param len - the total amount of bytes that needs to be read from the file
 * @return int - 0 if successfull, else -1
 */
int read_and_send_length_bytes_from_file(int sockfd, unsigned int len){
    int fd = open("/dev/urandom", O_RDONLY); 
    if (fd ==-1){
        fprintf(stderr, "%s\n", strerror(errno));
        return -1;
    }

    unsigned int bytes_read=0, total_bytes_read=0, size_to_read;
    while (1){
        size_to_read = MIN(READ_CHUNK, len - total_bytes_read);
        bytes_read=read(fd,big_buffer,size_to_read);
        if (bytes_read==-1){
            fprintf(stderr, "%s\n", strerror(errno));
            close(fd);
            return -1;
        }
        else {
            total_bytes_read+=bytes_read;
            if (send(sockfd, big_buffer , bytes_read, 0) < 1 ) {
                fprintf(stderr, "%s\n", strerror(errno));
                return -1;
            }
            if (total_bytes_read==len) break;
        }
    }
    close(fd);
    return 0;
    
}