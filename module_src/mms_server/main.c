#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../module_interface.h"
#include "cfg_parse.h"
#include "types.h"

//https://unix.stackexchange.com/questions/223385/why-and-how-are-some-shared-libraries-runnable-as-though-they-are-executables

struct module_private_data {
    module_callbacks *callbacks;
    int example_event_id;
    int param_a;
};

const char event_id[] = "EXAMPLE_EVENT";

int pre_init(module_object *instance, module_callbacks *callbacks)
{
    printf("pre init\n");
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
    if(cfg_get_int(config,"param_a",&data->param_a) != 0) { return -1; }

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
    printf("init\n");
    struct module_private_data * data = instance->module_data;
    //register for read/write callbacks
    //

    return 0;
}

//read-callback
int read_value()
{   
    //foreacht(registered_value in values)
    //if element==registered_value
    //  get info from file, and write into model
    //
    return 0; 
}

//write-callback
int write_value()
{   
    //foreacht(registered_value in values)
    //if element==registered_value
    //  write data into file
    //  update model
    //
    return 0; 
}


int run(module_object *instance)
{
    printf("run\n");
    struct module_private_data * data = instance->module_data;
    //example callback event
    data->callbacks->callback_event_cb(data->example_event_id);
    return 0;
}

//event is called for when this module is subscribed to a certain event
int event(module_object *instance, int event_id)
{
    printf("event %i\n", event_id);
    struct module_private_data * data = instance->module_data;
    return 0;
}

int test(void)
{
    printf("test\n");
    return 0;
}


int deinit(module_object *instance)
{
    printf("deinit\n");
    struct module_private_data * data = instance->module_data;

    free(data);
    return 0;
}


/* optional, not used when compiled as module */
int main (int argc, char *argv[]) {    
    printf("main\n");    
    return 0;                       
}   
