#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
 
 #include "message_slot.h"

int main(int argc, char** argv){
    if (argc!=4){
        fprintf(stderr, "Not enough arguments were given\n");
        return -1;
    }
    int opf=open(argv[1],O_WRONLY);
    if (opf==-1){
        fprintf(stderr,"%s\n",strerror(errno));
        return -1;
    }
   
    if (ioctl(opf,MSG_SLOT_CHANNEL, (unsigned int)strtoul(argv[2],NULL,10))==-1){ /* set channel id to the specified one*/
        close(opf);
        fprintf(stderr,"%s\n",strerror(errno));
        return -1;
    }
    int bytes_written=write(opf,argv[3],strlen(argv[3]));
    if (bytes_written==-1) { 
        close(opf);
        fprintf(stderr,"%s\n",strerror(errno));
        return -1;
    }
    close(opf);
    printf("Successfully wrote %d bytes to %s\n",bytes_written,argv[1]); /* print status message?*/
    return 0;
}   
