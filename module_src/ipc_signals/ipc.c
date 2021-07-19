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


struct module_private_data {
    module_callbacks *callbacks;
    int ipc_event_id;
    void *context;
    void *responder;
    void *requester;

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


    data->context = zmq_ctx_new ();
    data->responder = zmq_socket (data->context, ZMQ_SUB);
    int rc = zmq_bind (data->responder, "tcp://*:5555");
    rc = zmq_setsockopt (data->responder, ZMQ_SUBSCRIBE, 0, 0);
    if(rc != 0) return -1;

    data->requester = zmq_socket (zmq_ctx_new () , ZMQ_PUB);
    rc = zmq_connect (data->requester, "tcp://localhost:5555");
    if(rc != 0) return -1;

    instance->deadline = 1000;

    return 0;
}

int run(module_object *instance)
{
    struct module_private_data * data = instance->module_data;
    //listen for incoming events (non-blocking)
    char event_name [255];
    int result = zmq_recv(data->responder, event_name, 255, ZMQ_NOBLOCK);
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
    //foreach client
    {
        // send event-name
        zmq_send(data->requester, event_name, strlen(event_name), ZMQ_NOBLOCK);
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
    zmq_close (data->responder);
    zmq_close (data->requester);
    zmq_ctx_destroy (data->context);
    free(data);
    return 0;
}

int main()
{
    return 0;
}
