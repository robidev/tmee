#include "../../module_interface.h"
#include "cfg_parse.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <zmq.h>


#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)


typedef struct _client {
    char * client_address;
    void * publisher;
} client;

struct module_private_data {
    module_callbacks *callbacks;
    int ipc_event_id;
    void *context;
    void *subscriber;

    char * server_address;
    int client_count;
    client ** clients;
};

const char event_id[] = "IPC_EVENT";

int pre_init(module_object *instance, module_callbacks *callbacks)
{
    printf("pre-init id: %s, config: %s\n", instance->config_id, instance->config_file);

    //allocate module data
    struct module_private_data * data = malloc(sizeof(struct module_private_data));



    struct cfg_struct *config = cfg_init();
    if(cfg_load(config, instance->config_file) != 0)
    {
        printf("ERROR: could not load config file: %s\n", instance->config_file);
        return -1;
    }
    if(cfg_get_string(config, "server_address", &data->server_address) != 0) { return -1; }

    if(cfg_get_int(config,"client_count",&data->client_count) != 0) { return -1; }

    data->clients = malloc(sizeof(client) * data->client_count);
    int i;
    for(i = 0; i < data->client_count; i++)
    {
        char input_f[256];
        sprintf(input_f,"client%d",i);
        data->clients[i]->client_address = 0;
        if(cfg_get_string(config, input_f, &data->clients[i]->client_address) != 0) { return -1; }
    }

    // start server within pre-init context
    data->context = zmq_ctx_new ();
    data->subscriber = zmq_socket (data->context, ZMQ_SUB);
    int rc = zmq_bind (data->subscriber, data->server_address);
    rc = zmq_setsockopt (data->subscriber, ZMQ_SUBSCRIBE, 0, 0);
    if(rc != 0) return -1;

    //TODO calculate deadline for this module from test
    int temp_deadline = 1000;
    cfg_get_int(config,"deadline",&temp_deadline);
    instance->deadline = (long)temp_deadline;

    //register IPC_EVENT
    int len = strlen(instance->config_id) + strlen(event_id) + 10;
    char event_string[len]; 
    sprintf(event_string, "%s_%s",instance->config_id, event_id);
    data->ipc_event_id = callbacks->register_event_cb(event_string);

    data->callbacks = callbacks;
    //store module data in pointer
    instance->module_data = data;
    return 0;
}

int init(module_object *instance, module_callbacks *callbacks)
{
    printf("IPC init\n");
    struct module_private_data * data = instance->module_data;

    //start clients
    int i, rc;
    for(i = 0; i < data->client_count; i++)
    {
        data->clients[i]->publisher = zmq_socket (zmq_ctx_new () , ZMQ_PUB);
        rc = zmq_connect (data->clients[i]->publisher, data->clients[i]->client_address);
        if(rc != 0) return -1;
    }

    return 0;
}

int run(module_object *instance)
{
    struct module_private_data * data = instance->module_data;
    //listen for incoming events (non-blocking)
    char event_name [255];
    int result = zmq_recv(data->subscriber, event_name, 255, ZMQ_NOBLOCK);
    if (result > 0 )
    {
        printf("ipc event recv: %s\n", event_name);
        // register-event to get an ID (if the event allready exists, it will return the existing ID)
        int event_id = data->callbacks->register_event_cb(event_name);
        // call the event within our process
        data->callbacks->callback_event_cb(event_id);
    }
    return 0;
}

int event(module_object *instance, int event_id)
{
    //printf("IPC event called with id: %i\n", event_id);
    //call all registered callbacks
    struct module_private_data * data = instance->module_data;
    //search event_name, via ID
    char * event_name = data->callbacks->find_event_name_cb(event_id);
    int i,rc;
    for(i = 0; i < data->client_count; i++)
    {
        // send event-name to connected clients
        rc = zmq_send(data->clients[i]->publisher, event_name, strlen(event_name), ZMQ_NOBLOCK);
        if(rc == -1)
        {
            perror("zmq_send");
        }
    }

    data->callbacks->callback_event_cb(data->ipc_event_id);
    return 0;
}

//called to generate real-time performance metrics on this machine
int test(void)
{
    printf("IPC test\n");
    return 0;
}


int deinit(module_object *instance)
{
    printf("IPC deinit\n");
    struct module_private_data * data = instance->module_data;
    zmq_close (data->subscriber);
    free(data->server_address);
    int i;
    for(i = 0; i < data->client_count; i++)
    {
        zmq_close (data->clients[i]->publisher);
        free(data->clients[i]->client_address);
    }
    zmq_ctx_destroy (data->context);
    free(data);
    return 0;
}

int main()
{
    return 0;
}
