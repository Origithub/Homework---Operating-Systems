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
    if (argc!=3){
        fprintf(stderr, "Not enough arguments were given\n");
        return -1;
    }
    int ipf=open(argv[1],O_RDONLY);
    if (ipf==-1){
        fprintf(stderr,"%s\n",strerror(errno));
        return -1;
    }
    if (ioctl(ipf,MSG_SLOT_CHANNEL, (unsigned int)strtoul(argv[2],NULL,10))==-1){ /* set channel id to the specified one*/
        close(ipf);
        fprintf(stderr,"%s\n",strerror(errno));
        return -1;
    }
    char buffer[129]={'\0'};
    if (read(ipf,buffer,128)==-1){
        close(ipf);
        fprintf(stderr,"%s\n",strerror(errno));
        return -1;
    }
    close(ipf);
    printf("%s\n",buffer);
    return 0;
}