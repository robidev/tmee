#include "libiec61850/mms_value.h"
#include "libiec61850/sv_publisher.h"
#include "libiec61850/hal_thread.h"

#include "../../module_interface.h"
#include "cfg_parse.h"
#include "types.h"
#include "mem_file.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)


typedef void (*update_func)(SVPublisher_ASDU asdu, int index, char *buffer, int buffer_index);

void update_float(SVPublisher_ASDU asdu, int index, char *buffer, int buffer_index);
void update_quality(SVPublisher_ASDU asdu, int index, char *buffer, int buffer_index);
void update_int32(SVPublisher_ASDU asdu, int index, char *buffer, int buffer_index);

typedef struct _data_inputs {
    char *filename;
    int fd;
    char *buffer;
    int buffer_size;
    update_func update_function;
} data_inputs;

struct module_private_data {
    module_callbacks *callbacks;
    CommParameters smvCommParameters;
    SVPublisher publisher;

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

    int interval;//in microseconds
    long transmit_deadline;

    SVPublisher_ASDU asdu;
    int smpCnt;
};

long current_time()
{
    struct timeval timecheck;
    gettimeofday(&timecheck, NULL);
    return (long)timecheck.tv_sec * 1000000 + (long)timecheck.tv_usec;
}

int pre_init(module_object *instance, module_callbacks *callbacks)
{
    printf("smv pub pre-init id: %s, config: %s\n", instance->config_id, instance->config_file);

    //allocate module data
    struct module_private_data * data = malloc(sizeof(struct module_private_data));
    data->publisher = 0;
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
    data->smpCnt = 4000;
    data->interval = 250;

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

    if(cfg_get_int(config,"smpCnt",&data->smpCnt) != 0) { return -1; }
    if(cfg_get_int(config,"interval",&data->interval) != 0) { return -1; }

    if(cfg_get_int(config,"input_count",&data->input_count) != 0) { return -1; }

    //TODO calculate deadline for this module from test
    int temp_deadline = 100;
    cfg_get_int(config,"deadline",&temp_deadline);
    instance->deadline = (long)temp_deadline;

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
    printf("smv pub init\n");
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

    data->smvCommParameters.appId = data->appid;
    data->smvCommParameters.dstAddress[0] = data->dstMAC[0];
    data->smvCommParameters.dstAddress[1] = data->dstMAC[1];
    data->smvCommParameters.dstAddress[2] = data->dstMAC[2];
    data->smvCommParameters.dstAddress[3] = data->dstMAC[3];
    data->smvCommParameters.dstAddress[4] = data->dstMAC[4];
    data->smvCommParameters.dstAddress[5] = data->dstMAC[5];
    data->smvCommParameters.vlanId = data->vlanid;
    data->smvCommParameters.vlanPriority = data->vlanprio;

    data->publisher = SVPublisher_create(&data->smvCommParameters, data->interface);

    if(data->publisher == NULL) {
        printf("ERROR: could not create SMV publisher\n");
        return -1;
    }

    data->asdu = SVPublisher_addASDU(data->publisher, data->gocbref, data->datasetref, data->confrev);

    for(i = 0; i < data->input_count; i++)
    {
        int type = read_type(data->inputs[i]->buffer);
        switch(type)
        {
            case FLOAT32:
                data->inputs[i]->update_function = update_float;
                SVPublisher_ASDU_addFLOAT(data->asdu);
                break;
            case QUALITY:
                data->inputs[i]->update_function = update_quality;
                SVPublisher_ASDU_addQuality(data->asdu);
                break;
            case INT32:
                data->inputs[i]->update_function = update_int32;
                SVPublisher_ASDU_addINT32(data->asdu);
                break; 
            //TODO add more types
            default:
                break;           
        }
    }
    SVPublisher_ASDU_setSmpCntWrap(data->asdu, data->smpCnt);
    SVPublisher_ASDU_setRefrTm(data->asdu, 0);

    SVPublisher_setupComplete(data->publisher);
    
    //initialise receiver
    data->transmit_deadline = current_time() + (data->interval);//schedule for half the ttl time
    return 0;
}

int run(module_object *instance)
{
    struct module_private_data * data = instance->module_data;

    //check time, if next retransmit is due
    if(current_time() >= data->transmit_deadline)
    {
        data->transmit_deadline += data->interval;
        int i;
        for(i = 0; i < data->input_count; i++)
        {
            int current_index = read_index(data->inputs[i]->buffer);
            data->inputs[i]->update_function(data->asdu, i, data->inputs[i]->buffer, current_index); 
        }
        SVPublisher_ASDU_setRefrTmNs(data->asdu, Hal_getTimeInNs());
        SVPublisher_ASDU_increaseSmpCnt(data->asdu);
        SVPublisher_publish(data->publisher);
    }

    return 0;
}

int event(module_object *instance, int event_id)
{
    struct module_private_data * data = instance->module_data;
    printf("smv pub event id:%i\n", event_id);
    return 0;
}

int test(void)
{
    printf("smv pub test\n");
    return 0;
}


int deinit(module_object *instance)
{
    printf("smv pub deinit\n");
    struct module_private_data * data = instance->module_data;
    if(data->publisher)
        SVPublisher_destroy(data->publisher);

    int i;
    for(i = 0; i < data->input_count; i++)
    {
        free(data->inputs[i]->filename);
        munmap(data->inputs[i]->buffer,data->inputs[i]->buffer_size);
    }
    free(data->inputs);

    free(data->interface);
    free(data->gocbref);
    free(data->datasetref);
    free(data);
    return 0;
}

void update_float(SVPublisher_ASDU asdu, int index, char *buffer, int buffer_index)
{
    SVPublisher_ASDU_setFLOAT(asdu, index*4, (float)read_input_int32(buffer,index));
}

void update_int32(SVPublisher_ASDU asdu, int index, char *buffer, int buffer_index)
{
    SVPublisher_ASDU_setINT32(asdu, index*4, read_input_int32(buffer,buffer_index));
}

void update_quality(SVPublisher_ASDU asdu, int index, char *buffer, int buffer_index)
{
    SVPublisher_ASDU_setQuality(asdu, index*4, (Quality)read_input_int32(buffer,buffer_index));
}