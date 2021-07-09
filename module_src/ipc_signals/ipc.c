#include "../../module_interface.h"
#include "cfg_parse.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>


#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)


struct module_private_data {
    module_callbacks *callbacks;
    int ipc_event_id;

    char *input_file;
    char *output_file;

    unsigned int item_size;
    unsigned int max_items;
    unsigned int buffer_index;
    unsigned int * buffer;
    unsigned int output_buffer_size;
    int fd;
    int fd_config;

};

const char event_id[] = "IPC_EVENT";

int pre_init(module_object *instance, module_callbacks *callbacks)
{
    printf("pre-init id: %s, config: %s\n", instance->config_id, instance->config_file);

    //allocate module data
    struct module_private_data * data = malloc(sizeof(struct module_private_data));
    data->item_size = 0;
    data->max_items = 4000;
    data->buffer_index = -1;
    data->fd = 0;
    data->fd_config = 0;

    data->input_file = 0;
    data->output_file = 0;


    struct cfg_struct *config = cfg_init();
    if(cfg_load(config, instance->config_file) != 0)
    {
        printf("ERROR: could not load config file: %s\n", instance->config_file);
        return -1;
    }

    if(cfg_get_string(config, "input_file", &data->input_file) != 0) { return -1; }
    printf("Set input_file: %s\n", data->input_file);

    if(cfg_get_string(config, "output_file", &data->output_file) != 0) { return -1; }
    printf("Set output_file: %s\n", data->output_file);

    //create/mmap output files
    data->fd = open(data->output_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );//shm_open(STORAGE_ID, O_RDWR, S_IRUSR | S_IWUSR);
    if (data->fd < 0)
    {
        perror("open");
        return -10;
    }
    unsigned char file_name_buf[256];
    sprintf(file_name_buf, "%s_config", data->output_file);
    printf("output config name: %s\n", file_name_buf);
    data->fd_config = open(file_name_buf, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);//perm: 644
    if (data->fd_config == -1)
    {
        perror("open");
        return -11;
    }

    //register TRIP_VALUE_CHANGE event
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
    printf("init\n");
    struct module_private_data * data = instance->module_data;
    return 0;
}

int run(module_object *instance)
{
    printf("SI run\n");
    struct module_private_data * data = instance->module_data;
    return 0;
}

int event(module_object *instance, int event_id)
{
    printf("SI event called with id: %i\n", event_id);
    //call all registered callbacks
    struct module_private_data * data = instance->module_data;
    data->callbacks->callback_event_cb(data->ipc_event_id);
    return 0;
}

//called to generate real-time performance metrics on this machine
int test(void)
{
    printf("SI test\n");
    return 0;
}


int deinit(module_object *instance)
{
    printf("SI deinit\n");
    struct module_private_data * data = instance->module_data;


    close(data->fd);
    close(data->fd_config);

    munmap(data->buffer, data->output_buffer_size);

    free(data->input_file);
    free(data->output_file);

    free(data);
    return 0;
}

int main()
{
    return 0;
}
