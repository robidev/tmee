#include <libiec61850/hal_thread.h>
#include <libiec61850/goose_subscriber.h>
#include <libiec61850/goose_receiver.h>
#include "../../module_interface.h"
#include "cfg_parse.h"
#include "types.h"
#include "mem_file.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

/*              size    value
item_size       4       ...
max_items       4       10
index           4       0
semaphore+type  4       0/1 + goose
buffer[]        
*/


struct module_private_data {
    int goose_received_id;
    GooseReceiver receiver;
    GooseSubscriber subscriber;
    module_callbacks *callbacks;

    uint32_t item_size;
    uint32_t max_items;
    uint32_t buffer_index;
    char * output_buffer;
    uint32_t output_buffer_size;
    int fd;
    int fd_config;

    char * interface;
    char * shm_name;
    unsigned short appid;
    unsigned char * dstMac;
    char * goCbRef;

    int old_stNum;
    uint8_t MAC[6];
};

const char event_id[] = "GOOSE_RECEIVED";

static void
gooseListener(GooseSubscriber subscriber, void* parameter);

int pre_init(module_object *instance, module_callbacks *callbacks)
{
    printf("goose pre-init id: %s, config: %s\n", instance->config_id, instance->config_file);

    //allocate module data
    struct module_private_data * data = malloc(sizeof(struct module_private_data));
    data->item_size = 0;
    data->max_items = 0;
    data->buffer_index = -1;
    data->fd = 0;
    data->fd_config = 0;

    data->interface = 0;
    data->shm_name = 0;
    data->appid = 0x4000;
    data->dstMac = NULL;
    data->goCbRef = 0;
    data->old_stNum = 0;

    struct cfg_struct *config = cfg_init();
    if(cfg_load(config, instance->config_file) != 0)
    {
        printf("ERROR: could not load config file: %s\n", instance->config_file);
        return -1;
    }

    if(cfg_get_int(config,"buffer_entries",&data->max_items) != 0) { return -1; }
    
    if(cfg_get_string(config, "interface", &data->interface) != 0) { return -1; }
    printf("Set interface id: %s\n", data->interface);

    if(cfg_get_string(config, "gocbref", &data->goCbRef) != 0) { return -1; }
    printf("Set gocbref: %s\n", data->goCbRef);

    if(cfg_get_hex(config, "appid", &data->appid) != 0) { return -1; }
    printf("appid: 0x%.4x\n", data->appid);

    if(cfg_get_mac(config, "dstmac", data->MAC) != 0) 
    { 
        printf("no mac address set\n");
        data->dstMac = NULL;
    }
    else
    {
        printf("mac address is: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",data->MAC[0],data->MAC[1],data->MAC[2],data->MAC[3],data->MAC[4],data->MAC[5]);
        data->dstMac = data->MAC;
    }

    if(cfg_get_string(config, "output_file", &data->shm_name) != 0) { return -1; }
    printf("output shm name: %s\n", data->shm_name);

    //TODO calculate deadline for this module from test
    int temp_deadline = 100;
    cfg_get_int(config,"deadline",&temp_deadline);
    instance->deadline = (long)temp_deadline;

    //create/mmap output files
    remove(data->shm_name);
    data->fd = open(data->shm_name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR );//shm_open(STORAGE_ID, O_RDWR, S_IRUSR | S_IWUSR);
    if (data->fd < 0)
    {
        perror("open");
        return -10;
    }
    uint8_t file_name_buf[256];
    sprintf(file_name_buf, "%s_config", data->shm_name);
    printf("shm config name: %s\n", file_name_buf);
    remove(file_name_buf);//remove existing file to prevent a permission error
    data->fd_config = open(file_name_buf, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);//perm: 644
    if (data->fd_config == -1)
    {
        perror("open");
        return -11;
    }

    //register SAMPLE_RECEIVED event
    int len = strlen(instance->config_id) + strlen(event_id) + 10;
    char event_string[len]; 
    sprintf(event_string, "%s_%s",instance->config_id, event_id);
    data->goose_received_id = callbacks->register_event_cb(event_string);

    data->callbacks = callbacks;
    //store module data in pointer
    instance->module_data = data;
    cfg_free(config);

    return 0;
}

int init(module_object *instance, module_callbacks *callbacks)
{
    printf("goose init\n");
    struct module_private_data * data = instance->module_data;

    //allocate mmaped buffer
    data->item_size = type_to_item_size(GOOSE);
    data->output_buffer_size = calculate_buffer_size(data->item_size, data->max_items);
    // allocate size in file
    if(ftruncate(data->fd, data->output_buffer_size + 10) != 0) 
    {
        printf("ERROR: ftruncate issue\n");
        return -20;
    }
    data->output_buffer = mmap_fd_write(data->fd, data->output_buffer_size);


    write_size(data->output_buffer,data->item_size);
    write_items(data->output_buffer,data->max_items);
    write_index(data->output_buffer,0);
    write_type(data->output_buffer, GOOSE);

    //initialise receiver
    uint8_t bb[1024] = "";
    data->receiver = GooseReceiver_create();

    GooseReceiver_setInterfaceId(data->receiver, data->interface);

    sprintf(bb, "eth=%s\nshm name=%s\n",data->interface, data->shm_name);
    if(write(data->fd_config, bb, strlen(bb)) < strlen(bb) )
    {
        printf("ERROR: could not write to %s_config\n", data->shm_name);
    }

    if( data->dstMac != NULL)
    {
        sprintf(bb, "mac=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",data->MAC[0],data->MAC[1],data->MAC[2],data->MAC[3],data->MAC[4],data->MAC[5]);
        if(write(data->fd_config, bb, strlen(bb)) < strlen(bb) )
        {
            printf("ERROR: could not write to %s_config\n", data->shm_name);
        }
    }
    sprintf(bb, "appid=%.4x\n",data->appid);
    if(write(data->fd_config, bb, strlen(bb)) < strlen(bb) )
    {
        printf("ERROR: could not write to %s_config\n", data->shm_name);
    }  

    /* Create a subscriber listening to SV messages with APPID 0x-xxxx */
    data->subscriber = GooseSubscriber_create(data->goCbRef, NULL);

    GooseSubscriber_setDstMac(data->subscriber, data->dstMac);
    GooseSubscriber_setAppId(data->subscriber, data->appid);

    /* Install a callback handler for the subscriber */
    GooseSubscriber_setListener(data->subscriber, gooseListener, instance);

    /* Connect the subscriber to the receiver */
    GooseReceiver_addSubscriber(data->receiver, data->subscriber);

    /* Start listening to SV messages - starts a new receiver background thread */
    //SVReceiver_start(data->receiver);
    if(GooseReceiver_startThreadless(data->receiver) == NULL)
    {
         printf("ERROR: Starting SV receiver failed for interface %s\n", data->interface);
         return -1;
    }

    return 0;
}

int run(module_object *instance)
{
    struct module_private_data * data = instance->module_data;
    GooseReceiver_tick(data->receiver);
    return 0;
}

int event(module_object *instance, int event_id)
{
    printf("goose event id: %i\n", event_id);
    return 0;
}

//called to generate real-time performance metrics on this machine
int test(void)
{
    printf("goose test\n");
    return 0;
}


int deinit(module_object *instance)
{
    printf("goose sub deinit\n");
    struct module_private_data * data = instance->module_data;

    /* Stop listening to SV messages */
    //SVReceiver_stop(data->receiver);
    if(data->receiver)
    {
        GooseReceiver_stopThreadless(data->receiver);
        /* Cleanup and free resources */
        GooseReceiver_destroy(data->receiver);
    }

    close(data->fd);
    close(data->fd_config);

    munmap(data->output_buffer, data->output_buffer_size);

    free(data->goCbRef);
    free(data->interface);
    free(data->shm_name);

    free(data);
    return 0;
}



/* Callback handler for received SV messages */
static void
gooseListener (GooseSubscriber subscriber, void* parameter)
{
    module_object *instance = parameter;
    struct module_private_data * data = instance->module_data;

    //printf("GOOSE event:\n");
    //printf("  stNum: %u sqNum: %u\n", GooseSubscriber_getStNum(subscriber),
    //        GooseSubscriber_getSqNum(subscriber));
    //printf("  timeToLive: %u\n", GooseSubscriber_getTimeAllowedToLive(subscriber));

    //uint64_t timestamp = GooseSubscriber_getTimestamp(subscriber);

    //printf("  timestamp: %u.%u\n", (uint32_t) (timestamp / 1000), (uint32_t) (timestamp % 1000));
    //printf("  message is %s\n", GooseSubscriber_isValid(subscriber) ? "valid" : "INVALID");

    int stNum = GooseSubscriber_getStNum(subscriber);

    if(stNum != data->old_stNum)
    {
        data->old_stNum = stNum;

        MmsValue* values = GooseSubscriber_getDataSetValues(subscriber);

        register uint32_t item_size = data->item_size;

        data->buffer_index = (data->buffer_index + 1) % data->max_items;

        MmsValue_printToBuffer(values, (data->output_buffer + 16) + (data->buffer_index * (item_size)), 1024);

        //update the output_buffer index
        *(data->output_buffer + 8) = data->buffer_index;
    
        //call all registered callbacks
        data->callbacks->callback_event_cb(data->goose_received_id);
    }

}


