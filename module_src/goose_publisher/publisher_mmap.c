#include "libiec61850/mms_value.h"
#include "libiec61850/goose_publisher.h"
#include "libiec61850/hal_thread.h"

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


struct module_private_data {
    module_callbacks *callbacks;

    uint32_t item_size;
    uint32_t max_items;
    uint32_t buffer_index;
    char ** input_buffers;
    uint32_t input_buffer_size;
    int fd;
    int fd_config;

    char * interface;
    char * shm_name;

    unsigned char * srcMac;
    unsigned char * dstMac;
    uint8_t srcMAC[6];
    uint8_t dstMAC[6];

    int usevlantag;
    int vlanprio;
    short vlanid;

    unsigned short appid;
    char *gocbref;
    char *datasetref;
    int confrev;
    int simulation;
    int ndscomm;
    int ttl;

    int stNum;
    int sqNum;
};

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

    data->srcMac = NULL;
    data->dstMac = NULL;

    data->usevlantag = 0;
    data->vlanprio = 0;
    data->vlanid = 0;

    data->appid = 0x4000;

    data->gocbref = 0;
    data->datasetref = 0;
    data->confrev = 1;
    data->simulation = 0;
    data->ndscomm = 0;
    data->ttl = 500;

    data->stNum = 0;
    data->sqNum = 0;

    struct cfg_struct *config = cfg_init();
    if(cfg_load(config, instance->config_file) != 0)
    {
        printf("ERROR: could not load config file: %s\n", instance->config_file);
        return -1;
    }

    if(cfg_get_string(config, "interface", &data->interface) != 0) { return -1; }
    printf("Set interface id: %s\n", data->interface);

    if(cfg_get_mac(config, "srcmac", data->srcMAC) != 0) 
    { 
        printf("no mac address set\n");
        data->srcMac = NULL;
    }
    else
    {
        printf("mac address is: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",data->srcMAC[0],data->srcMAC[1],data->srcMAC[2],data->srcMAC[3],data->srcMAC[4],data->srcMAC[5]);
        data->srcMac = data->srcMAC;
    }

    if(cfg_get_mac(config, "dstmac", data->dstMAC) != 0) 
    { 
        printf("no mac address set\n");
        data->dstMac = NULL;
    }
    else
    {
        printf("mac address is: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",data->dstMAC[0],data->dstMAC[1],data->dstMAC[2],data->dstMAC[3],data->dstMAC[4],data->dstMAC[5]);
        data->dstMac = data->dstMAC;
    }

    if(cfg_get_int(config,"usevlantag",&data->usevlantag) != 0) { return -1; }
    if(data->usevlantag)
    {
        if(cfg_get_int(config,"vlanprio",&data->vlanprio) != 0) { return -1; }
        if(cfg_get_hex(config,"vlanid",&data->vlanid) != 0) { return -1; }
    }
    if(cfg_get_hex(config,"appid",&data->appid) != 0) { return -1; }

    if(cfg_get_string(config, "gocbref", &data->gocbref) != 0) { return -1; }
    if(cfg_get_string(config, "datasetref", &data->datasetref) != 0) { return -1; }

    if(cfg_get_int(config,"confrev",&data->confrev) != 0) { return -1; }
    if(cfg_get_int(config,"simulation",&data->simulation) != 0) { return -1; }
    if(cfg_get_int(config,"ndscomm",&data->ndscomm) != 0) { return -1; }
    if(cfg_get_int(config,"ttl",&data->ttl) != 0) { return -1; }

    //dataset={int,bool,float,dbpos,string,time}

    data->callbacks = callbacks;
    //store module data in pointer
    instance->module_data = data;
    return 0;
}

int init(module_object *instance, module_callbacks *callbacks)
{
    printf("goose init\n");
    struct module_private_data * data = instance->module_data;

    //allocate mmaped buffers

    CommParameters gooseCommParameters;

    gooseCommParameters.appId = 1000;
    gooseCommParameters.dstAddress[0] = 0x01;
    gooseCommParameters.dstAddress[1] = 0x0c;
    gooseCommParameters.dstAddress[2] = 0xcd;
    gooseCommParameters.dstAddress[3] = 0x01;
    gooseCommParameters.dstAddress[4] = 0x00;
    gooseCommParameters.dstAddress[5] = 0x01;
    gooseCommParameters.vlanId = 0;
    gooseCommParameters.vlanPriority = 4;

    GoosePublisher publisher = GoosePublisher_create(&gooseCommParameters, "eth0");

    if (publisher) {
        GoosePublisher_setGoCbRef(publisher, "simpleIOGenericIO/LLN0$GO$gcbAnalogValues");
        GoosePublisher_setConfRev(publisher, 1);
        GoosePublisher_setDataSetRef(publisher, "simpleIOGenericIO/LLN0$AnalogValues");
        GoosePublisher_setTimeAllowedToLive(publisher, 500);
    }

    //datasets
    /*
    alloc mmap buffer for element-values (add sizeof of all together for buf-size)
    alloc array1 for element-position in buffer
    alloc array2 for mmsvalue pointer
   
    LinkedList dataSetValues = LinkedList_create();
    for (index in array)
    {
        set value in buffer
        create mmsvalue(value)
        assign mmsvalue to array2
        add to dataset linked list
        LinkedList_add(dataSetValues, mmsval);
    }
    */
    //initialise receiver

    return 0;
}

int run(module_object *instance)
{
    printf("goose run\n");

    //during operation
    while(true)
    {
        /*if(index != oldindex)//index is updated
        {
            for(idx in array)//check complete buffer
            {
                value = buffer[array1[idx]]
                setmmsvalue(array2[idx], value)
            }
            //send goose
            set fast transmit
        }
        retransmit
        GoosePublisher_publish(publisher, dataSetValues);*/
    }
    //have thread handle retransmission scheme
    return 0;
}

int event(module_object *instance, int event_id)
{
    printf("goose event %i\n", event_id);
    return 0;
}

int test(void)
{
    printf("goose test\n");
    return 0;
}


int deinit(module_object *instance)
{
    printf("goose deinit\n");

    //LinkedList_destroyDeep(dataSetValues, (LinkedListValueDeleteFunction) MmsValue_delete);
    //cfg_free(config);
    struct module_private_data * data = instance->module_data;
    free(data);
    return 0;
}

void updateElement(int index, void * value)
{
    //write data to mmaped buffer
    //trigger trigger fast retransmit
    //increment stval index
}

void TriggerRestransmit()
{
    //send out new goose
    //set timeout for next goose
}

void slowRetransmit()
{
    //send goose
    //if retransmit-time < max, double it
    //if retransmit-time > max; retransmit-time = max;
}

void sendGoose()
{
}

