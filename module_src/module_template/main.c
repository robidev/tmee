#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../module_interface.h"

//https://unix.stackexchange.com/questions/223385/why-and-how-are-some-shared-libraries-runnable-as-though-they-are-executables

struct module_private_data {
    int jaja_id;
};

const char event_id[] = "SAMPLE_RECEIVED";

int pre_init(module_object *instance, module_callbacks *callbacks)
{
    printf("pre init\n");
    printf("id: %s, config: %s\n", instance->config_id, instance->config_file);

    struct module_private_data * data = malloc(sizeof(struct module_private_data));

    int len = strlen(instance->config_id) + strlen(event_id) + 10;
    char event_string[len]; 
    sprintf(event_string, "%s_%s",instance->config_id, event_id);
    data->jaja_id = callbacks->register_event_cb(event_string);

    instance->module_data = data;
    return 0;
}

int init(module_object *instance, module_callbacks *callbacks)
{
    printf("init\n");
    struct module_private_data * data = instance->module_data;
    callbacks->callback_event_cb(data->jaja_id);
    return 0;
}

int run(module_object *instance)
{
    printf("run\n");
    return 0;
}

int event(module_object *instance, int event_id)
{
    printf("event %i\n", event_id);
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
