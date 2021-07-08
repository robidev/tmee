#include <libiec61850/hal_thread.h>
#include <libiec61850/sv_subscriber.h>
#include "../../module_interface.h"
#include "../../cfg_parse.h"

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
item_size       4       64(8x int+q ) + 4(SmpCnt) = 68
max_items       4       4000
index           4       0
semaphore+type  4       0/1 + smv
buffer[]        68*4000 [data_items+smpcnt]
*/


struct module_private_data {
    int sample_received_id;
    SVReceiver receiver;
    SVSubscriber subscriber;
    module_callbacks *callbacks;

    uint32_t item_size;
    uint32_t max_items;
    uint32_t buffer_index;
    uint32_t * buffer;
    uint32_t buffer_size;
    int fd;
    int fd_config;

    char * interface;
    char * shm_name;
    unsigned short appid;
    unsigned char * dstMac;
    uint8_t MAC[6];
};

const char event_id[] = "SAMPLE_RECEIVED";

static void
svUpdateListener (SVSubscriber subscriber, void* parameter, SVSubscriber_ASDU asdu);

int pre_init(module_object *instance, module_callbacks *callbacks)
{
    printf("pre-init id: %s, config: %s\n", instance->config_id, instance->config_file);

    //allocate module data
    struct module_private_data * data = malloc(sizeof(struct module_private_data));
    data->item_size = 0;
    data->max_items = 4000;
    data->buffer_index = -1;
    data->fd = 0;
    data->fd_config = 0;

    data->interface = 0;
    data->shm_name = 0;
    data->appid = 0x4000;
    data->dstMac = NULL;

    struct cfg_struct *config = cfg_init();
    if(cfg_load(config, instance->config_file) != 0)
    {
        printf("ERROR: could not load config file: %s\n", instance->config_file);
        return -1;
    }

    if(cfg_get_string(config, "interface", &data->interface) != 0) { return -1; }
    printf("Set interface id: %s\n", data->interface);

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

    //create/mmap output files
    data->fd = open(data->shm_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );//shm_open(STORAGE_ID, O_RDWR, S_IRUSR | S_IWUSR);
    if (data->fd < 0)
    {
        perror("open");
        return -10;
    }
    uint8_t file_name_buf[256];
    sprintf(file_name_buf, "%s_config", data->shm_name);
    printf("shm config name: %s\n", file_name_buf);
    data->fd_config = open(file_name_buf, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);//perm: 644
    if (data->fd_config == -1)
    {
        perror("open");
        return -11;
    }

    //register SAMPLE_RECEIVED event
    int len = strlen(instance->config_id) + strlen(event_id) + 10;
    char event_string[len]; 
    sprintf(event_string, "%s_%s",instance->config_id, event_id);
    data->sample_received_id = callbacks->register_event_cb(event_string);

    data->callbacks = callbacks;
    //store module data in pointer
    instance->module_data = data;
    return 0;
}

int init(module_object *instance, module_callbacks *callbacks)
{
    printf("init\n");
    struct module_private_data * data = instance->module_data;

    uint8_t bb[1024] = "";
    data->receiver = SVReceiver_create();

    SVReceiver_setInterfaceId(data->receiver, data->interface);

    sprintf(bb, "eth=%s\nshm name=%s\n",data->interface, data->shm_name);
    write(data->fd_config, bb, strlen(bb));    

    if( data->dstMac != NULL)
    {
        sprintf(bb, "mac=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",data->MAC[0],data->MAC[1],data->MAC[2],data->MAC[3],data->MAC[4],data->MAC[5]);
        write(data->fd_config, bb, strlen(bb)); 
    }
    sprintf(bb, "appid=%.4x\n",data->appid);
    write(data->fd_config, bb, strlen(bb));  

    /* Create a subscriber listening to SV messages with APPID 0x-xxxx */
    data->subscriber = SVSubscriber_create(data->dstMac, data->appid);

    /* Install a callback handler for the subscriber */
    SVSubscriber_setListener(data->subscriber, svUpdateListener, instance);

    /* Connect the subscriber to the receiver */
    SVReceiver_addSubscriber(data->receiver, data->subscriber);

    /* Start listening to SV messages - starts a new receiver background thread */
    //SVReceiver_start(data->receiver);
    if(SVReceiver_startThreadless(data->receiver) == NULL)
    {
         printf("ERROR: Starting SV receiver failed for interface %s\n", data->interface);
    }
    return 0;
}

int run(module_object *instance)
{
    struct module_private_data * data = instance->module_data;
    SVReceiver_tick(data->receiver);
    return 0;
}

int event(module_object *instance, int event_id)
{
    printf("smv event id: %i\n", event_id);
    return 0;
}

//called to generate real-time performance metrics on this machine
int test(void)
{
    printf("smv test\n");
    return 0;
}


int deinit(module_object *instance)
{
    printf("deinit\n");
    struct module_private_data * data = instance->module_data;

    /* Stop listening to SV messages */
    //SVReceiver_stop(data->receiver);
    SVReceiver_stopThreadless(data->receiver);

    /* Cleanup and free resources */
    SVReceiver_destroy(data->receiver);

    close(data->fd);
    close(data->fd_config);

    munmap(data->buffer, data->buffer_size);

    free(data->interface);
    free(data->shm_name);

    free(data);
    return 0;
}



/* Callback handler for received SV messages */
static void
svUpdateListener (SVSubscriber subscriber, void* parameter, SVSubscriber_ASDU asdu)
{
    module_object *instance = parameter;
    struct module_private_data * data = instance->module_data;

    uint32_t data_size = SVSubscriber_ASDU_getDataSize(asdu);
    if(unlikely( data->item_size == 0 ) && data_size > 0)//init 
    {
	    data->item_size = data_size;
        data->buffer_size = ( ( data->item_size + 4 ) * data->max_items) + 16;

        if(ftruncate(data->fd, data->buffer_size) != 0)
        {
            printf("ftruncate issue\n");
            exit(30);
        }
        // map shared memory to process address space
        data->buffer = (uint32_t*)mmap(NULL, data->buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, data->fd, 0);
        if (data->buffer == MAP_FAILED)
        {
            printf("mmap issue:\n");
            perror("mmap");
            exit(40);
        }
        *data->buffer = data->item_size;
        *(data->buffer + 1) = data->max_items;
        *(data->buffer + 2) = 0;

        //additional data
        const char* svID = SVSubscriber_ASDU_getSvId(asdu);
        if (svID != NULL)
        {
            printf("svID=%s\n", svID);
            uint8_t bb[1024] = "";
            sprintf(bb, "svID=%s\npacket size=%u\nitems:%u\n",svID, data->item_size, data->max_items);
            write(data->fd_config, bb, strlen(bb));
        }

    }
    if(data->item_size != data_size)
    {
        printf("packet size mismatch\n");
        return;//only skip this packet
    }
    uint32_t item_size_int = data->item_size / 4;

    data->buffer_index = (data->buffer_index + 1) % data->max_items;

    printf("buffer index: %u\n", data->buffer_index);
    for(uint32_t i = 0; i < item_size_int; i += 1)
    {
        *( (data->buffer + 4) + (data->buffer_index * (item_size_int + 1)) + i) = SVSubscriber_ASDU_getINT32(asdu, i*4);
    }
    *( (data->buffer + 4) + (data->buffer_index * (item_size_int + 1)) + item_size_int) = SVSubscriber_ASDU_getSmpCnt(asdu);
    //update the buffer index
    *(data->buffer + 2) = data->buffer_index;
   
    //call all registered callbacks
    data->callbacks->callback_event_cb(data->sample_received_id);
}


