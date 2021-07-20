#include "libiec61850/iec61850_server.h"
#include "libiec61850/hal_thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../module_interface.h"
#include "cfg_parse.h"
#include "types.h"


typedef struct _element {
    char * objectRef;
    ModelNode * node;
    module_object *instance;
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

    data->elements = malloc(sizeof(element) * data->element_count);
    int i;
    for(i = 0; i < data->element_count; i++)
    {
        char input_f[256];
        sprintf(input_f,"element%d",i);
        data->elements[i]->objectRef = 0;
        if(cfg_get_string(config, input_f, &data->elements[i]->objectRef) != 0) { return -1; }

        //for state reference in callbacks
        data->elements[i]->instance = instance;
    }

    //TODO calculate deadline for this module from test
    int temp_deadline = 100;
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
        data->elements[i]->node = IedModel_getModelNodeByShortObjectReference(data->model,data->elements[i]->objectRef);
        IedServer_handleWriteAccess(data->iedServer, (DataAttribute *)data->elements[i]->node, writeAccessHandler, data->elements[i]);
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
    element * elem = parameter;
    module_object *instance = elem->instance;
    struct module_private_data * data = instance->module_data;

    printf("read element: %s\n", elem->objectRef);

    printf("Read access to %s/%s.%s\n", ld->name, ln->name, dataObject->name);

    //foreacht(registered_value in values)
    //if element==registered_value
    //  get info from file, and write into model
    //
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

    //foreacht(registered_value in values)
    //if element==registered_value
    //  write data into file
    //  update model
    //
    return DATA_ACCESS_ERROR_OBJECT_ACCESS_DENIED;
}


int run(module_object *instance)
{
    printf("run mms server\n");
    struct module_private_data * data = instance->module_data;

    //uint64_t timeval = Hal_getTimeInMs();
    //IedServer_lockDataModel(data->iedServer);
    //IedServer_unlockDataModel(data->iedServer);

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

    int i;
    for(i = 0; i < data->element_count; i++)
    {
        free(data->elements[i]->objectRef);
    }
    free(data->elements);
    free(data->config_file);
    /* stop MMS server - close TCP server socket and all client sockets */
    IedServer_stopThreadless(data->iedServer);
    IedModel_destroy(data->model);
    /* Cleanup - free all resources */
    IedServer_destroy(data->iedServer);

    free(data);
    return 0;
}


/* optional, not used when compiled as module */
int main (int argc, char *argv[]) {    
    printf("main\n");    
    return 0;                       
}   
