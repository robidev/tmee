#include <hal_thread.h>
#include <signal.h>
#include <stdio.h>
#include <sv_subscriber.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>


#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

static bool running = true;

void sigint_handler(int signalId)
{
    running = 0;
}

uint32_t item_size = 0;
uint32_t max_items = 4000;
uint32_t buffer_index = -1;
uint32_t * buffer;
int fd = 0;
int fd_config = 0;

typedef void (*cb_t)(void *userdata);

struct callback_struct {
    cb_t callback;
    struct callback_struct *next;
};

struct callback_struct *callback_list;


/* Callback handler for received SV messages */
static void
svUpdateListener (SVSubscriber subscriber, void* parameter, SVSubscriber_ASDU asdu)
{
    uint32_t data_size = SVSubscriber_ASDU_getDataSize(asdu);
    if(unlikely( item_size == 0 ) && data_size > 0)//init 
    {
	item_size = data_size;
        uint32_t buffer_size = ( ( item_size + 4 ) * max_items) + 16;

        if(ftruncate(fd, buffer_size) != 0)
	{
            printf("ftruncate issue\n");
            exit(30);
	}
	// map shared memory to process address space
	buffer = (uint32_t*)mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (buffer == MAP_FAILED)
	{
            printf("mmap issue:\n");
            perror("mmap");
            exit(40);
	}
        *buffer = item_size;
        *(buffer + 1) = max_items;
        *(buffer + 2) = 0;

        //additional data
        const char* svID = SVSubscriber_ASDU_getSvId(asdu);
        if (svID != NULL)
        {
            printf("svID=%s\n", svID);
            uint8_t bb[1024] = "";
            sprintf(bb, "svID=%s\npacket size=%u\nitems:%u\n",svID, item_size, max_items);
            write(fd_config, bb, strlen(bb));
        }

    }
    if(item_size != data_size)
    {
        printf("packet size mismatch\n");
        return;//only skip this packet
    }
    uint32_t item_size_int = item_size / 4;

    buffer_index = (buffer_index + 1) % max_items;

    printf("buffer index: %u\n", buffer_index);
    for(uint32_t i = 0; i < item_size_int; i += 1)
    {
        *( (buffer + 4) + (buffer_index * (item_size_int + 1)) + i) = SVSubscriber_ASDU_getINT32(asdu, i*4);
    }
    *( (buffer + 4) + (buffer_index * (item_size_int + 1)) + item_size_int) = SVSubscriber_ASDU_getSmpCnt(asdu);
    //update the buffer index
    *(buffer + 2) = buffer_index;
   
    //call all registered callbacks
    struct callback_struct *callbacks = callback_list;

    while(callbacks != NULL)
    {
        callbacks->callback(parameter);
        callbacks = callbacks->next;
    }
}

int register_callback(cb_t func)
{
    struct callback_struct *current_node = callback_list;
    
    while(current_node != NULL && current_node->next != NULL) // find last valid entry
    {
        current_node = current_node->next;
    }
        
    struct callback_struct *new_node = (struct callback_struct*) malloc(sizeof(struct callback_struct));
    new_node->callback = func;
    new_node->next = NULL;  
 
    if(current_node != NULL)
    {
        current_node->next = new_node;
    }
    else
    {
        callback_list = new_node;
    }
}


int
main(int argc, char** argv)
{
    unsigned char * shm_name = "/dev/shm/iec61850-9-2";
    unsigned short appid = 0x4000;
    unsigned char * dstMac = NULL;
    uint8_t MAC[6];
    uint8_t bb[1024] = "";


    SVReceiver receiver = SVReceiver_create();

    if (argc > 1) {
        SVReceiver_setInterfaceId(receiver, argv[1]);
		printf("Set interface id: %s\n", argv[1]);
    }
    else {
        printf("Using interface eth0\n");
        SVReceiver_setInterfaceId(receiver, "eth0");
    }

    if(argc > 2)
    {
        shm_name = argv[2];
    }
    printf("shm name: %s\n", shm_name);
    fd = open(shm_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );//shm_open(STORAGE_ID, O_RDWR, S_IRUSR | S_IWUSR);
    if (fd < 0)
    {
        perror("open");
        return 10;
    }

    uint8_t file_name_buf[256];
    sprintf(file_name_buf, "%s_config", shm_name);
    printf("shm config name: %s\n", file_name_buf);
    fd_config = open(file_name_buf, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);//perm: 644

    if (fd_config == -1)
    {
        perror("open");
        return 11;
    }

    sprintf(bb, "eth=%s\nshm name=%s\n",argv[1], shm_name);
    write(fd_config, bb, strlen(bb));    

    
    if (argc > 3) {
        appid = (unsigned short)strtoul(argv[3], NULL, 16);
    }
    printf("appid: 0x%.4x\n", appid);

    if (argc > 4) {
        int values[6];
        if( 6 == sscanf( argv[4], "%x:%x:%x:%x:%x:%x%*c",
            &values[0], &values[1], &values[2],
            &values[3], &values[4], &values[5] ) )
        {
            /* convert to uint8_t */
            for(int i = 0; i < 6; ++i )
                MAC[i] = (uint8_t) values[i];
        }
        else
        {
            printf("coud not parse mac. pleae use format 'aa:bb:cc:dd:ee:ff'\n");/* invalid mac */
            return -1;
        }
        dstMac = MAC;
    }
    if( dstMac == NULL)
    {
        printf("no mac address set\n");
    }
    else
    {
        printf("mac address is: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n",MAC[0],MAC[1],MAC[2],MAC[3],MAC[4],MAC[5]);
        sprintf(bb, "mac=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x\nappid=%.4x\n",MAC[0],MAC[1],MAC[2],MAC[3],MAC[4],MAC[5], appid);
        write(fd_config, bb, strlen(bb));  
    }

    
    callback_list = NULL;

    /* Create a subscriber listening to SV messages with APPID 4000h */
    SVSubscriber subscriber = SVSubscriber_create(dstMac, appid);

    /* Install a callback handler for the subscriber */
    SVSubscriber_setListener(subscriber, svUpdateListener, NULL);

    /* Connect the subscriber to the receiver */
    SVReceiver_addSubscriber(receiver, subscriber);

    /* Start listening to SV messages - starts a new receiver background thread */
    SVReceiver_start(receiver);

    if (SVReceiver_isRunning(receiver)) {
        signal(SIGINT, sigint_handler);

        while (running)
            Thread_sleep(1);

        /* Stop listening to SV messages */
        SVReceiver_stop(receiver);
    }
    else {
        printf("Failed to start SV subscriber. Reason can be that the Ethernet interface doesn't exist or root permission are required.\n");
    }

    /* Cleanup and free resources */
    SVReceiver_destroy(receiver);
    return 0;
}
