#include "libiec61850/mms_value.h"
#include "libiec61850/goose_publisher.h"
#include "libiec61850/hal_thread.h"
#include "../../cfg_parse.h"

#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>



#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

int stNum;
int sqNum;
int restransmit_time;

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


// file buffer
//  - size per item
//  - maxitems
//  - index, incremented for transmission, after write
//  - semaphore + type
//  - array


int
main(int argc, char** argv)
{
    //create mmaped dataset from config-file
    //mmap data buffer for GOOSE
    // poll for index change in data buffer, if so, trigger fast retransmit
    // functioncall with update
    struct cfg_struct *config;

    // Initialize config struct
    config = cfg_init();
    cfg_load(config,"config.ini");
    

    cfg_get(config,"interface"); // eth
    cfg_get(config,"srcmac");    // 00:11:22:33:44:55
    cfg_get(config,"dstmac");    // aa:bb:cc:dd:ee:ff
    cfg_get(config,"usevlantag");// true/false
    cfg_get(config,"vlanprio");  // 4
    cfg_get(config,"vlanid");    // 0x00
    cfg_get(config,"appid");     // 0x4000
    cfg_get(config,"gocbref");   // gocb1
    cfg_get(config,"datasetref");// simplevalues 
    cfg_get(config,"dataset");   // {1:int,2:bool,3:float,4:dbpos,5:string,6:time}
    cfg_get(config,"confrev");   // value
    cfg_get(config,"simulation");// true/false
    cfg_get(config,"ndscomm");   // true/false
    cfg_get(config,"ttl");       // 500


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

    //during operation
    while(true)
    {
        if(index != oldindex)//index is updated
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
        GoosePublisher_publish(publisher, dataSetValues);
    }
    //have thread handle retransmission scheme


    LinkedList_destroyDeep(dataSetValues, (LinkedListValueDeleteFunction) MmsValue_delete);
    cfg_free(config);
    return 0;
}
