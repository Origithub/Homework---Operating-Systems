#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H
#include <linux/ioctl.h>

#define MSG_SLOT_CHANNEL _IOW(0, 0, unsigned long)
#define DEVICE_NAME "message_slot"
#define BUFFER_SIZE 128

typedef struct channel{ /* linked list of the channels */
    struct channel* next;
    char buffer[BUFFER_SIZE];
    int message_length;
    int channel_id;
}Channel;

typedef struct msg_slot { /* linked list of all the slots */
    struct msg_slot* next;
    unsigned int minor_number;
    Channel* channels;
}Slot;

typedef struct msg_module{
    Slot* slots;
}Module;


#endif /* MESSAGE_SLOT */