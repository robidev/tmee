#include "libiec61850/iec61850_server.h"
#include "libiec61850/hal_thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../module_interface.h"
#include "cfg_parse.h"
#include "mem_file.h"
#include "types.h"

#include <signal.h>
#include <time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>


typedef struct _element {
    char * objectRef;
    ModelNode * node;
    ModelNode * Objnode;
    module_object *instance;

    char * filename;
    int fd;
    char * buffer;
    int buffer_size;
    int item_size;
    int max_items;
} element;

struct module_private_data {
    module_callbacks *callbacks;
    int example_event_id;
    char * config_file;
    int server_port;
    IedServer iedServer;
    IedModel* model;

    int element_count;
    element ** elements;

};

const char event_id[] = "MMS_EVENT";

static MmsDataAccessError
readAccessHandler(LogicalDevice* ld, LogicalNode* ln, DataObject* dataObject, FunctionalConstraint fc, ClientConnection connection, void* parameter);
static MmsDataAccessError
writeAccessHandler (DataAttribute* dataAttribute, MmsValue* value, ClientConnection connection, void* parameter);

int pre_init(module_object *instance, module_callbacks *callbacks)
{
    printf("mms pre init\n");
    printf("id: %s, config: %s\n", instance->config_id, instance->config_file);

    // allocate private data
    struct module_private_data * data = malloc(sizeof(struct module_private_data));

    //load config
    struct cfg_struct *config = cfg_init();
    if(cfg_load(config, instance->config_file) != 0)
    {
        printf("ERROR: could not load config file: %s\n", instance->config_file);
        return -1;
    }
    // retrieve a variable from config
    data->config_file = 0;
    if(cfg_get_string(config,"config_file",&data->config_file) != 0) { return -1; }

    if(cfg_get_int(config,"server_port",&data->server_port) != 0) { return -1; }

    if(cfg_get_int(config,"element_count",&data->element_count) != 0) { return -1; }

    data->elements = malloc(sizeof(element *) * data->element_count);
    int i;
    for(i = 0; i < data->element_count; i++)
    {
        data->elements[i] = malloc(sizeof(element));
        char input_f[256];
        sprintf(input_f,"element%d",i);
        data->elements[i]->objectRef = 0;
        if(cfg_get_string(config, input_f, &data->elements[i]->objectRef) != 0) { return -1; }

        sprintf(input_f,"element%d_file",i);
        data->elements[i]->filename = 0;
        if(cfg_get_string(config, input_f, &data->elements[i]->filename) != 0) { return -1; }
        //for state reference in callbacks
        data->elements[i]->instance = instance;
        data->elements[i]->node = 0;
        data->elements[i]->Objnode = 0;
    }

    //TODO calculate deadline for this module from test
    int temp_deadline = 2000;
    cfg_get_int(config,"deadline",&temp_deadline);
    instance->deadline = (long)temp_deadline;

    //register event for callbacks
    int len = strlen(instance->config_id) + strlen(event_id) + 10;
    char event_string[len]; 
    sprintf(event_string, "%s_%s",instance->config_id, event_id);
    data->example_event_id = callbacks->register_event_cb(event_string);

    data->callbacks = callbacks;
    //store module data in pointer
    instance->module_data = data;
    cfg_free(config);
    return 0;
}

int init(module_object *instance, module_callbacks *callbacks)
{
    printf("mms init\n");
    struct module_private_data * data = instance->module_data;
    /* parse the configuration file and create the data model */
    data->model = ConfigFileParser_createModelFromConfigFileEx(data->config_file);

    if (data->model == NULL) {
        printf("Error parsing config file!\n");
        return -1;
    }
    IedServerConfig config = IedServerConfig_create();
    data->iedServer = IedServer_createWithConfig(data->model, NULL, config);
    IedServerConfig_destroy(config);

    IedServer_setReadAccessHandler(data->iedServer, readAccessHandler, instance);

    int i;
    for(i = 0; i < data->element_count; i++)
    {
        data->elements[i]->node = IedModel_getModelNodeByObjectReference(data->model,data->elements[i]->objectRef);
        if(data->elements[i]->node)
        {
            data->elements[i]->Objnode = data->elements[i]->node->parent;
            IedServer_handleWriteAccess(data->iedServer, (DataAttribute *)data->elements[i]->node, writeAccessHandler, data->elements[i]);

            remove(data->elements[i]->filename);//remove existing file to prevent a permission error
            data->elements[i]->fd = open(data->elements[i]->filename, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);//perm: 644
            if (data->elements[i]->fd < 0)
            {
                perror(data->elements[i]->filename);
                return -10;
            }
            data->elements[i]->item_size = type_to_item_size(INT32);
            data->elements[i]->max_items = 10;

            data->elements[i]->buffer_size = calculate_buffer_size(data->elements[i]->item_size, data->elements[i]->max_items);
                // allocate size in file
            if(ftruncate(data->elements[i]->fd, data->elements[i]->buffer_size + 10) != 0) 
            {
                printf("ERROR: ftruncate issue\n");
                return -20;
            }
            
            data->elements[i]->buffer = mmap_fd_write(data->elements[i]->fd, data->elements[i]->buffer_size);
            write_size(data->elements[i]->buffer,data->elements[i]->item_size);
            write_items(data->elements[i]->buffer,data->elements[i]->max_items);
            write_index(data->elements[i]->buffer,0);
            write_type(data->elements[i]->buffer, INT32);
        }
        else
            printf("ERROR: %s does not exist\n",data->elements[i]->objectRef);
    }

    /* MMS server will be instructed to start listening to client connections. */
    IedServer_startThreadless(data->iedServer, data->server_port);

    if (!IedServer_isRunning(data->iedServer)) {
        printf("Starting server failed! Exit.\n");
        IedServer_destroy(data->iedServer);
        return -1;
    }

    return 0;
}

//read-callback
static MmsDataAccessError
readAccessHandler(LogicalDevice* ld, LogicalNode* ln, DataObject* dataObject, FunctionalConstraint fc, ClientConnection connection, void* parameter)
{   
    module_object *instance = parameter;
    struct module_private_data * data = instance->module_data;

    //IedServer_updateBooleanAttributeValue(data->iedServer,)
    printf("Read access to %s/%s.%s\n", ld->name, ln->name, dataObject->name);
    int i;
    for(i = 0; i < data->element_count; i++)
    {
        if(data->elements[i]->node == (ModelNode *)dataObject)
        {
            //TODO other types of data
            int val = read_current_input_int32(data->elements[i]->buffer);
            printf("read val=%i\n",val);
            //  get info from file, and write into model, jut before we need it
            IedServer_lockDataModel(data->iedServer);
            IedServer_updateInt32AttributeValue(data->iedServer,(DataAttribute *)data->elements[i]->node,val);
            IedServer_unlockDataModel(data->iedServer);
        }
    }
    return DATA_ACCESS_ERROR_SUCCESS;
}

//write-callback
static MmsDataAccessError
writeAccessHandler (DataAttribute* dataAttribute, MmsValue* value, ClientConnection connection, void* parameter)
{
    element * elem = parameter;
    module_object *instance = elem->instance;
    struct module_private_data * data = instance->module_data;

    printf("write element: %s\n", elem->objectRef);

    //TODO: support other types, match mmsvalue with filetype
    int val = MmsValue_toInt32(value);
    printf("write val: %i\n",val);
    write_output_int(elem->buffer,val);

    return DATA_ACCESS_ERROR_SUCCESS;//DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED;
}


int run(module_object *instance)
{
    struct module_private_data * data = instance->module_data;
    /* Has to be called whenever the TCP stack receives data */
    IedServer_processIncomingData(data->iedServer);
    /* Has to be called periodically */
    IedServer_performPeriodicTasks(data->iedServer);

    return 0;
}

//event is called for when this module is subscribed to a certain event
int event(module_object *instance, int event_id)
{
    printf("mms event %i\n", event_id);
    struct module_private_data * data = instance->module_data;
    int i;

    //process a data-update, if triggered
    for(i = 0; i < data->element_count; i++)
    {
        if(data->elements[i]->node != 0)
        {
            //TODO other types of data
            int val = read_current_input_int32(data->elements[i]->buffer);
            printf("read val=%i\n",val);
            //  get info from file, and write into model, jut before we need it
            IedServer_lockDataModel(data->iedServer);
            IedServer_updateInt32AttributeValue(data->iedServer,(DataAttribute *)data->elements[i]->node,val);
            IedServer_unlockDataModel(data->iedServer);
        }
    }
    return 0;
}

int test(void)
{
    printf("mms test\n");
    return 0;
}


int deinit(module_object *instance)
{
    printf("mms deinit\n");
    struct module_private_data * data = instance->module_data;
    if(data->iedServer)
    {
        /* stop MMS server - close TCP server socket and all client sockets */
        IedServer_stopThreadless(data->iedServer);
        /* Cleanup - free all resources */
        IedServer_destroy(data->iedServer);
    }
    if(data->model)
        IedModel_destroy(data->model);

    int i;
    for(i = 0; i < data->element_count; i++)
    {
        free(data->elements[i]->objectRef);
        free(data->elements[i]);
    }
    free(data->elements);
    free(data->config_file);
    free(data);
    return 0;
}


/* optional, not used when compiled as module */
int main (int argc, char *argv[]) {    
    printf("main\n");    
    return 0;                       
}   
