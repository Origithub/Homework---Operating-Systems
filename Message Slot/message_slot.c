#undef __KERNEL__
#define __KERNEL__

#undef MODULE
#define MODULE

#include <linux/kernel.h>   
#include <linux/module.h>   
#include <linux/fs.h>       
#include <linux/uaccess.h>
#include <linux/string.h> 
#include <linux/init.h>
#include <linux/slab.h>
MODULE_LICENSE("GPL");
#include "message_slot.h"


int check_if_slot_exists(unsigned int minor_number);
int allocate_msg_slot(int minor_number);
Channel* get_or_create_channel(Slot* slot,int channel_id);
Slot* get_slot(int minor_number);
Channel* allocate_channel(Slot *slot,int channel_id);
Channel* get_channel(Slot* slot, int channel_id);
void free_all_slots_and_channels(Slot* slot);
void add_channel_to_slot(Slot* slot,Channel* new_channel,int channel_id);
void free_all_channels(Channel* channels);

static int majorNumber;
static Module* module=NULL;

//===================================== DEVICE FUNCTIONS ==============================================//
static int device_open(struct inode* inode, struct file*  file ){
    int res, temp;
    unsigned int minor=iminor(inode);
    if (minor<0) return -EINVAL;
    file->private_data=NULL;
    res=check_if_slot_exists(minor);
    if (res==0){ /* msg_slot already exists for the minor number */
        return 0;
    }
    if (res==2){ /*first call to open for the specific major number, so allocate module and a msg_slot node */
        module=kmalloc(sizeof(Module), GFP_KERNEL); /* module allocation */
        if (module==NULL){
            return -1;
        }
        module->slots=NULL;
    }
    temp = allocate_msg_slot(minor);
    if (temp==-1){
        return -1;
    }
    return 0;
}

static ssize_t device_read(struct file* file,char __user* buffer,size_t length, loff_t* offset ){
    Channel* temp = (Channel*)file->private_data;
    Slot* slot = get_slot(iminor(file_inode(file)));
    if (slot==NULL || temp==NULL){/* check if a device file with the specified minor has been created */
        return -EINVAL;
    }
    if (temp->message_length==-1){ /* means no one wrote to the channel */
        return -EWOULDBLOCK;
    }
    if (temp->message_length>length){
        return -ENOSPC;
    }
    if ( copy_to_user(buffer,temp->buffer, temp->message_length) ) { /* copy_to_user returns amount of bytes not successfully written */
        return -EINVAL;
    }
    return (ssize_t)temp->message_length;
}
static int device_release( struct inode* inode,
                           struct file*  file){
                               return 0;
}
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset){
    Channel* temp = (Channel*)file->private_data;
	char prev_msg[BUFFER_SIZE];
    Slot* slot = get_slot(iminor(file_inode(file))); /* check if a device file with the specified minor has been created */
    if (slot==NULL || temp==NULL){
        return -EINVAL;
    }
    if (length<=0 || length>BUFFER_SIZE){
        return -EMSGSIZE;
    }
	memcpy(prev_msg, temp->buffer , BUFFER_SIZE);
    if (copy_from_user(temp->buffer,buffer,length)){ /* copy_from_user returns amount of bytes not successfully written */
        memcpy(temp->buffer , prev_msg, BUFFER_SIZE); /* atomic writing to the channel */
        return -EINVAL;
    }
    temp->message_length=length;
    return (ssize_t)length; /* this is the amount of bytes written because otherwise copy_from_user would have returned something bigger than 0 and we wouldn't have got here */
}

static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param){
    Slot* slot;
    if (ioctl_command_id!=MSG_SLOT_CHANNEL || ioctl_param<=0){
        return -EINVAL;
    }
    slot = get_slot(iminor(file_inode(file)));
    if (slot==NULL){
        return -1;
    }  
    file->private_data=get_or_create_channel(slot, ioctl_param);
    if (file->private_data==NULL){
        return -1;
    }  
    return 0;
}


int check_if_slot_exists(unsigned int minor_number){
    Slot* temp;
    if (module==NULL){ /* first open request for the specific major number */
        return 2;
    }
    temp = module->slots;
    if (temp==NULL){
        return 1;
    }
    while (temp!=NULL){
        if (temp->minor_number==minor_number){
            return 0;
        }
        temp=temp->next;
    }
    return 1; /* the minor_number wasn't found, meaning there was no open request for the specific minor_number */
}

int allocate_msg_slot(int minor_number){
    Slot* temp=kmalloc(sizeof(Slot), GFP_KERNEL);
    if (temp==NULL){
        return -1;
    }
    if (module->slots==NULL){
        module->slots=temp;
        temp->next=NULL; /* now there is only one node in module */
    }
    else{
        temp->next=module->slots; /* LIFO*/
        module->slots=temp;
    }
    module->slots->minor_number=minor_number;
    module->slots->channels=NULL;
    return 0;
}

Slot* get_slot(int minor_number){
    Slot* temp = module->slots;
    if (temp==NULL){
        return NULL;
    }
    while (temp!=NULL){
        if (temp->minor_number==minor_number){
            return temp;
        }
        temp=temp->next;
    }
    return NULL;
}

void add_channel_to_slot(Slot* slot,Channel* new_channel,int channel_id){
    Channel *temp;
    if (slot->channels==NULL){
        slot->channels=new_channel;
    }
    else{
        temp=slot->channels;
        slot->channels=new_channel;
        new_channel->next=temp;
    }
}

Channel* get_or_create_channel(Slot* slot,int channel_id){
    Channel *channel; Channel *new_channel;
    if (slot==NULL) return NULL;
    channel = get_channel(slot, channel_id);
    if (channel==NULL){ /* no channel wih the specific channel_id exists yet */
        new_channel=allocate_channel(slot,channel_id);
        if (new_channel==NULL){ /* allocation failure */
            return NULL;
        }
        add_channel_to_slot(slot, new_channel, channel_id);
        return new_channel;
    }
    return channel;
}
Channel* get_channel(Slot* slot, int channel_id){
    Channel* channel = slot->channels;
    while (channel!=NULL){
        if (channel->channel_id == channel_id){
            return channel;
        }
        channel=channel->next;
    }
    return NULL;
}

Channel* allocate_channel(Slot *slot,int channel_id){
    Channel* temp=kmalloc(sizeof(Channel),GFP_KERNEL);
    if (temp==NULL) return NULL;
    temp->next=NULL;
    temp->message_length=-1;
    temp->channel_id=channel_id;
    return temp;
}


void free_all_channels(Channel* channels){
    Channel *temp;
    while (channels!=NULL){
        temp=channels;
        channels=channels->next;
        kfree(temp);
    }
}
void free_all_slots_and_channels(Slot* slot){
    Slot* temp;
    while (slot!=NULL){
        free_all_channels(slot->channels);
        temp=slot;
        slot=slot->next;
        kfree(temp);
    }
}






//==================== DEVICE SETUP =========================//
struct file_operations Fops = {
    .read           = device_read,
    .write          = device_write,
    .open           = device_open,
    .release        = device_release,
    .unlocked_ioctl = device_ioctl,
};



static int __init message_slot_init(void){
    majorNumber=register_chrdev(0, DEVICE_NAME, &Fops);
    if (majorNumber < 0 ){
        printk(KERN_ERR "Module initialization failed!\n");
        return majorNumber;
    }
    printk(KERN_INFO "message_slot: registered major number %d\n", majorNumber);
    return 0;
} 

static void __exit cleanup(void){
    if (module!=NULL) {
        if (module->slots!=NULL){
            free_all_slots_and_channels(module->slots);
        }
        kfree(module);
    }
    unregister_chrdev(majorNumber,DEVICE_NAME);
}

//-------------------------------
module_init(message_slot_init);
module_exit(cleanup);
