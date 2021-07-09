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

typedef struct _data_inputs {
    char *filename;
    int fd;
    char *buffer;
    int buffer_size;
    MmsValue *element;
} data_inputs;

struct module_private_data {
    module_callbacks *callbacks;
    CommParameters gooseCommParameters;
    GoosePublisher publisher;
    LinkedList dataSetValues;

    int input_count;
    data_inputs **inputs;

    char * interface;

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
    data->input_count = 0;
    data->interface = 0;

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

    if(cfg_get_int(config,"input_count",&data->input_count) != 0) { return -1; }

    data->inputs = malloc(sizeof(data_inputs) * data->input_count);
    int i;
    for(i = 0; i < data->input_count; i++)
    {
        char input_f[256];
        sprintf(input_f,"input_file%d",i);
        data->inputs[i]->filename = 0;
        if(cfg_get_string(config, input_f, &data->inputs[i]->filename) != 0) { return -1; }
    }
    //dataset={int,bool,float,dbpos,string,time}

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

    //allocate mmaped buffers for each input
    int i;
    for(i = 0; i < data->input_count; i++)
    {
        data->inputs[i]->fd = open(data->inputs[i]->filename, O_RDWR, S_IRUSR );
        if (data->inputs[i]->fd < 0)
        {
            perror("open");
            return -10;
        }
        
        data->inputs[i]->buffer_size = calculate_buffer_size_from_file(data->inputs[i]->fd );
        data->inputs[i]->buffer = mmap_fd_write(data->inputs[i]->fd, data->inputs[i]->buffer_size);
    }

    data->gooseCommParameters.appId = data->appid;
    data->gooseCommParameters.dstAddress[0] = data->dstMAC[0];
    data->gooseCommParameters.dstAddress[1] = data->dstMAC[1];
    data->gooseCommParameters.dstAddress[2] = data->dstMAC[2];
    data->gooseCommParameters.dstAddress[3] = data->dstMAC[3];
    data->gooseCommParameters.dstAddress[4] = data->dstMAC[4];
    data->gooseCommParameters.dstAddress[5] = data->dstMAC[5];
    data->gooseCommParameters.vlanId = data->vlanid;
    data->gooseCommParameters.vlanPriority = data->vlanprio;

    data->publisher = GoosePublisher_create(&data->gooseCommParameters, data->interface);

    if (data->publisher) {
        GoosePublisher_setGoCbRef(data->publisher, data->gocbref);
        GoosePublisher_setConfRev(data->publisher, data->confrev);
        GoosePublisher_setDataSetRef(data->publisher, data->datasetref);
        GoosePublisher_setTimeAllowedToLive(data->publisher, data->ttl);
    }
   
    data->dataSetValues = LinkedList_create();
    for(i = 0; i < data->input_count; i++)
    {
        int type = read_type(data->inputs[i]->buffer);
        switch(type)
        {
            case BOOL:
                data->inputs[i]->element = MmsValue_newBoolean( read_current_input_bool(data->inputs[i]->buffer) );
                LinkedList_add(data->dataSetValues, data->inputs[i]->element);
                break;
            case INT8:
                data->inputs[i]->element = MmsValue_newIntegerFromInt8( read_current_input_int8(data->inputs[i]->buffer) );
                LinkedList_add(data->dataSetValues, data->inputs[i]->element);
                break;
            case INT32:
                data->inputs[i]->element = MmsValue_newIntegerFromInt32( read_current_input_int32(data->inputs[i]->buffer) );
                LinkedList_add(data->dataSetValues, data->inputs[i]->element);
                break; 
            //TODO add more types
            default:
                break;           
        }
    }
    //initialise receiver

    return 0;
}

int run(module_object *instance)
{
    printf("goose run\n");
    struct module_private_data * data = instance->module_data;
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
        //retransmit*/

        
        GoosePublisher_publish(data->publisher, data->dataSetValues);
        break;
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
    struct module_private_data * data = instance->module_data;
    LinkedList_destroyDeep(data->dataSetValues, (LinkedListValueDeleteFunction) MmsValue_delete);

    free(data->interface);
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

