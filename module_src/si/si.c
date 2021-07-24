#include "../../module_interface.h"
#include "cfg_parse.h"
#include "types.h"
#include "mem_file.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)


struct module_private_data {
    module_callbacks *callbacks;
    int trip_value_changed_id;
    int param_a;
    int param_b;

    char *input_file;
    char *output_file;

    unsigned int item_size;
    unsigned int max_items;
    unsigned int buffer_index;
    unsigned char * input_buffer;
    unsigned int input_buffer_size;
    unsigned char * output_buffer;
    unsigned int output_buffer_size;
    int in;
    int fd;
    int fd_config;
    int old_index;
    char trip;

    int is_recording;
    float total;
    float sample_count;
    long start_time;
    int vector;
};

const char event_id[] = "TRIP_VALUE_CHANGE";

long current_time()
{
    struct timespec x;
    clock_gettime(CLOCK_MONOTONIC, &x);
    return (long) (x.tv_sec * 1000000 + x.tv_nsec/1000);
}

int pre_init(module_object *instance, module_callbacks *callbacks)
{
    printf("pre-init SI id: %s, config: %s\n", instance->config_id, instance->config_file);

    //allocate module data
    struct module_private_data * data = malloc(sizeof(struct module_private_data));
    data->item_size = 0;
    data->max_items = 10;
    data->buffer_index = -1;
    data->in = 0;
    data->fd = 0;
    data->fd_config = 0;

    data->input_file = 0;
    data->output_file = 0;
    data->trip = 0;
    data->is_recording = 0;
    data->vector = 80; //80 samples per cycle


    struct cfg_struct *config = cfg_init();
    if(cfg_load(config, instance->config_file) != 0)
    {
        printf("ERROR: could not load config file: %s\n", instance->config_file);
        return -1;
    }
    if(cfg_get_int(config,"param_a",&data->param_a) != 0) { return -1; }
    if(cfg_get_int(config,"param_b",&data->param_b) != 0) { return -1; }
    printf("param a: %i, param b: %i\n", data->param_a, data->param_b);

    if(cfg_get_int(config,"buffer_entries",&data->max_items) != 0) { return -1; }

    if(cfg_get_string(config, "input_file", &data->input_file) != 0) { return -1; }
    printf("Set input_file: %s\n", data->input_file);

    if(cfg_get_string(config, "output_file", &data->output_file) != 0) { return -1; }
    printf("Set output_file: %s\n", data->output_file);

    //TODO calculate deadline for this module from test
    int temp_deadline = 100;
    cfg_get_int(config,"deadline",&temp_deadline);
    instance->deadline = (long)temp_deadline;

    //create output file(s)
    remove(data->output_file);
    data->fd = open(data->output_file, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR );//shm_open(STORAGE_ID, O_RDWR, S_IRUSR | S_IWUSR);
    if (data->fd < 0)
    {
        perror("open");
        return -10;
    }
    unsigned char file_name_buf[256];
    sprintf(file_name_buf, "%s_config", data->output_file);
    printf("output config name: %s\n", file_name_buf);
    remove(file_name_buf);
    data->fd_config = open(file_name_buf, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);//perm: 644
    if (data->fd_config == -1)
    {
        perror("open");
        return -11;
    }

    //register TRIP_VALUE_CHANGE event
    int len = strlen(instance->config_id) + strlen(event_id) + 10;
    char event_string[len]; 
    sprintf(event_string, "%s_%s",instance->config_id, event_id);
    data->trip_value_changed_id = callbacks->register_event_cb(event_string);

    data->callbacks = callbacks;
    //store module data in pointer
    instance->module_data = data;
    cfg_free(config);
    return 0;
}

int init(module_object *instance, module_callbacks *callbacks)
{
    printf("init SI\n");
    struct module_private_data * data = instance->module_data;

    //mmap input file
    data->in = open(data->input_file, O_RDWR, S_IRUSR );//shm_open(STORAGE_ID, O_RDWR, S_IRUSR | S_IWUSR);
    if (data->in < 0)
    {
        perror("open");
        return -10;
    }
    //map input file
    data->input_buffer_size = calculate_buffer_size_from_file(data->in);
    data->input_buffer = mmap_fd_write(data->in, data->input_buffer_size);
    data->old_index = read_index(data->input_buffer);

    //map output file
    data->item_size = type_to_item_size(BOOL);
    data->output_buffer_size = calculate_buffer_size(data->item_size, data->max_items);
    // allocate size in file
    if(ftruncate(data->fd, data->output_buffer_size+10) != 0) 
    {
        printf("ERROR: ftruncate issue\n");
        return -20;
    }
    data->output_buffer = mmap_fd_write(data->fd, data->output_buffer_size);

    write_type(data->output_buffer, BOOL);
    write_size(data->output_buffer,data->item_size);
    write_items(data->output_buffer,data->max_items);
    write_index(data->output_buffer,0);
    
    return 0;
}

int run(module_object *instance)
{
    event(instance, 42);
    return 0;
}

int event(module_object *instance, int event_id)
{
    struct module_private_data * data = instance->module_data;

    int index = read_index(data->input_buffer);
    if(data->old_index == index)
    {
        return -1; // no updated value
    }

    // new data from input
    while(data->old_index != index)
    {
        if(data->trip == 1) // dont calculate if we have tripped until reset
        {
            return 0;
        }
               //   si       VI  EI  LTI
        float a = 0.02; //    1,  2,   1
        float k = 0.14; // 13,5, 80, 120

        float c = 0; //fixed for all IEC curves
        float P = 1; //fixed for all IEC curves

        float time_dial = data->param_a; //time dial setting (2)
        float treshold = data->param_b; // treshold current  (800)


        //calculate RMS
        int i;
        float total = 0;

        //TODO: improve this by adding the newest sample, and removing the 80th
        for(i=0; i<data->vector; i++)
        {
            // measured current for vector amount of samples
            float current = (float)read_input_smv(data->input_buffer,0, (data->old_index - i) % data->max_items);
            total += pow(current,2);
        }
        float current_rms = sqrt(total/(float)data->vector);
        //float current_rms = current;

        //calculate pickup
        if(current_rms > treshold && data->is_recording == 0)// start recording
        {
            data->start_time=current_time();
            data->total=0;
            data->sample_count=0;
            data->is_recording=1;
        }
        //calculate trip
        if(data->is_recording)
        {
            data->total += current_rms;
            data->sample_count++;
        
            float average_current = data->total/data->sample_count;

            if(average_current > treshold)// if the average current is larger then the treshold (and stays there)
            {
                //calculate time to trip for average current
                float trip_time = time_dial * ((k / (pow( ( average_current/treshold ), a) - P) ) + c); 

                // check if the time that the average value is above the treshold value, is longer then the SI trip time
                if((data->start_time - current_time()) > trip_time)
                {
                    //trip!
                    data->trip = 1;
                    write_output_bool(data->output_buffer,1);
                    //call all registered callbacks
                    data->callbacks->callback_event_cb(data->trip_value_changed_id);
                }
            }
            else //current average is below treshold, so reset pickup
            {
                data->is_recording = 0;
            }  
        }
        data->old_index = (data->old_index + 1) % read_items(data->input_buffer);
    }  
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

    munmap(data->output_buffer, data->output_buffer_size);

    free(data->input_file);
    free(data->output_file);

    free(data);
    return 0;
}


