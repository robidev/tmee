#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
//#include <sys/time.h>
#include <time.h>
#include <sys/queue.h> //http://infnis.wikidot.com/list-from-sys-queue-h
#include <string.h>
#include <signal.h>
#include "cfg_parse.h"
#include "module_interface.h"

#include <xpedite/framework/Framework.H>
#include <xpedite/framework/Probes.H>
#include <xpedite/framework/Options.H>

#define MAX_EVENT_IDS 100

//TODO
//test function
//log function
//other timing-scheme (interrupt)

int init_modules();
void clean_all();
void unload_modules();
int register_event_id(const char *event_name);
int subscribe_event_id(int event_id, module_object *module);
char *find_event_name(int id);
int run_callback_func(int event_id);
long current_time();
void run_asap();
void run_deadline(long interval);
void profile(char * scope, long start,long end);
void test_modules();


typedef struct _event_table {
    char *name;
    int id;
    LIST_ENTRY(_event_table) next;
} event_table;

typedef struct _event_chain {
    module_object *module;
    struct _event_chain *next;
} event_chain;

/* Globals */
LIST_HEAD(event_list, _event_table) event_ids;
LIST_HEAD(module_list, _module_object) registered_modules;
event_chain *event_chains[MAX_EVENT_IDS];
module_callbacks callbacks;
int deadlines_skipped = 0;
/* End globals */

static int running = 1;

void sigint_handler(int signalId)
{
    running = 0;
}

/*long current_time()
{
    struct timeval timecheck;
    gettimeofday(&timecheck, NULL);
    return (long)timecheck.tv_sec * 1000000 + (long)timecheck.tv_usec;
}*/
long current_time()
{
    struct timespec x;
    clock_gettime(CLOCK_MONOTONIC, &x);
    return (long) (x.tv_sec * 1000000 + x.tv_nsec/1000);
}


int main(int argc, char ** argv)
{
    const xpedite::framework::Options options = {xpedite::framework::AWAIT_PROFILE_BEGIN};
    if(!xpedite::framework::initialize("/tmp/xpedite-appinfo.txt", options)) 
    { 
        throw std::runtime_error {"failed to init xpedite"}; 
    }


    char *config_file = "config/main_config.ini";
    int modules = 0;
    for(int i = 0; i< MAX_EVENT_IDS; i++)
        event_chains[i] = NULL;

    if(argc > 1)
    {
        config_file = argv[1];
    }
    printf("INFO: loading config file: %s\n",config_file);

    struct cfg_struct *config = cfg_init();
    if(cfg_load(config, config_file) != 0)
    {
        printf("ERROR: could not load config file: %s\n", config_file);
        return -1;
    }

    int interval = 0; 
    char *interval_type=0;
    char *test_filename=0;
    char *logfile=0;

    if(cfg_get_int(config,"interval",&interval) != 0) { return -1; }
    if(cfg_get_string(config, "interval_type", &interval_type) != 0) { return -1; }
    if(cfg_get_string(config, "test_filename",&test_filename) != 0) { return -1; }
    if(cfg_get_string(config, "logfile",&logfile) != 0) { return -1; }

    printf("interval: %d, type: %s, test: %s, log: %s\n",interval, interval_type, test_filename, logfile);

    //load callbacks
    callbacks.callback_event_cb = run_callback_func;
    callbacks.register_event_cb = register_event_id;
    callbacks.find_event_name_cb = find_event_name;

    //load modules
    if(cfg_get_int(config,"module_count",&modules) != 0)
    {
        return -1;
    }
    if(modules > 10000)
    {
        return -1;
    }

    printf("modules = %d\n", modules);

    for(int module_index = 0; module_index < modules; module_index++)
    {
        char module_i[256] = "";

        char *module_file=0;

        //load the module filename
        sprintf(module_i, "module%d_file", module_index);
        printf("\nLOADING: %s\n", module_i);
        if(cfg_get_string(config, module_i, &module_file) !=0) {continue; }

        //allocate module instance
        module_object *module_instance = (module_object *) malloc(sizeof(module_object));

        //load library
        module_instance->lib = dlopen(module_file, RTLD_NOW);
        if(module_instance->lib == 0)
        {
            printf("ERROR: cannot load library %s\n", module_file);
            fprintf(stderr, "%s\n", dlerror()); 
            free(module_file);
            free(module_instance);
            continue;
        }
        free(module_file);

        //load settings
        module_instance->pre_init = dlsym(module_instance->lib, "pre_init");
        if(*module_instance->pre_init == 0){printf("pre_init error\n"); free(module_instance); continue;}

        module_instance->init = dlsym(module_instance->lib, "init");
        if(*module_instance->init == 0){printf("init error\n"); free(module_instance); continue;}

        module_instance->run = dlsym(module_instance->lib, "run");
        if(*module_instance->run == 0){printf("run error\n"); free(module_instance); continue;}

        module_instance->test = dlsym(module_instance->lib, "test");
        if(*module_instance->test == 0){printf("test error\n"); free(module_instance); continue;}

        module_instance->event = dlsym(module_instance->lib, "event");
        if(*module_instance->event == 0){printf("event error\n"); free(module_instance); continue;}

        module_instance->deinit = dlsym(module_instance->lib, "deinit");
        if(*module_instance->deinit == 0){printf("deinit error\n"); free(module_instance); continue;}

        //set global config data
        char *module_id=0;
        sprintf(module_i, "module%d_id", module_index);
        if(cfg_get_string(config, module_i,&module_id) !=0) 
        { 
            free(module_instance);
            continue; 
        }
        module_instance->config_id = module_id;

        char *module_config=0;
        sprintf(module_i, "module%d_config", module_index);
        if(cfg_get_string(config, module_i, &module_config) !=0) 
        { 
            free(module_id);
            free(module_instance);
            continue; 
        }        
        module_instance->config_file = module_config;
        
        //check if module->run should be called during runtime, or only events should be active
        module_instance->run_enabled=0;
        sprintf(module_i, "module%d_register_run", module_index);
        cfg_get_int(config, module_i, &module_instance->run_enabled);
        printf("run is set to %d\n",module_instance->run_enabled);     

        //initialise deadline
        module_instance->deadline = -1; //deadline will be set by init;

        //initialise output files
        printf("-- calling pre_init() --\n"); 
        if(module_instance->pre_init(module_instance,&callbacks) != 0)
        {
            free(module_id);
            free(module_config);
            free(module_instance);
            printf("pre_init error\n"); 
            continue;
        }

        //register this module to be called when a specific event is published 
        char * event_name = 0;
        int idx = 1;
        sprintf(module_i, "module%d_event_trigger_id%d",module_index, idx);
        while(cfg_get_string(config, module_i, &event_name) == 0)
        {
            printf("event trigger: %s\n",event_name);
            int id = register_event_id(event_name);
            subscribe_event_id(id, module_instance);
            //clean up
            free(event_name);
            event_name = 0;
            //prepare next item
            idx++;
            sprintf(module_i, "module%d_event_trigger_id%d",module_index, idx);
        }

        //add module to end of list
        if(LIST_FIRST(&registered_modules) == NULL)
        {
            LIST_INSERT_HEAD(&registered_modules, module_instance, next);
        }
        else
        {
            module_object *last;
            LIST_FOREACH(last, &registered_modules, next)
            {
                if(LIST_NEXT(last,next) == NULL)//find last entry in list
                {
                    LIST_INSERT_AFTER(last, module_instance, next);
                    break;
                }
            }            
        }
        printf("MODULE REGISTERED: module%d_file\n", module_index);
    }

    if(LIST_FIRST(&registered_modules) == NULL)
    {
        printf("ERROR: no modules loaded for init\n");
        clean_all();
        //free allocated memory
        free(interval_type);
        free(test_filename);
        free(logfile);
        cfg_free(config);
        return -1;
    }

    //initialise modules in loaded order
    init_modules();

    //all modules were unloaded due to unsuccesfull init
    if(LIST_FIRST(&registered_modules) == NULL)
    {
        printf("ERROR: no modules loaded for run\n");
        clean_all();
        //free allocated memory
        free(interval_type);
        free(test_filename);
        free(logfile);
        cfg_free(config);
        return -1;
    }

    //register ctr-c handler
    signal(SIGINT, sigint_handler);

    //execute run loop
    printf("\ncalling run()\n");
    if(interval > 0)
    {
        //busy-wait loop
        run_deadline(interval);    
    }
    else
    {
        run_asap();
    }

    //cleanup everything nicely
    printf("\ncleaning up\n");
    clean_all();
    //free allocated memory
    free(interval_type);
    free(test_filename);
    free(logfile);
    cfg_free(config);

    printf("\n--- finished ---\n");
    return 0;
}

void clean_all()
{
    //deinit and free loaded modules
    unload_modules();

    //free event id's
    event_table * event_id = LIST_FIRST(&event_ids);
    while(event_id != NULL) {
        event_table * temp = event_id;
        event_id = LIST_NEXT(event_id, next);
        free(temp->name);
        free(temp);
    }

    //free event chains
    for(int i = 0; i < MAX_EVENT_IDS; i++)
    {
        event_chain *chain = event_chains[i];
        while(chain)
        {
            event_chain *temp = chain;
            chain = chain->next;
            free(temp);
        }
    }
}

int init_modules()
{
    //initialise modules in correct order
    module_object * module;
    LIST_FOREACH(module, &registered_modules, next)
    {
        printf("\nINIT: %s\n", module->config_id);
        if(module->init(module,&callbacks) != 0)
        {
            printf("ERROR: init() failed for module: %s, removing from list\n", module->config_id);
            LIST_REMOVE(module,next);
            module->deinit(module);
            free(module->config_id);
            free(module->config_file);
            dlclose(module->lib);
            free(module);
        }
    }
    return 0;
}

void unload_modules()
{
    module_object * module;
    LIST_FOREACH(module, &registered_modules, next)
    {
        module->deinit(module);
        free(module->config_id);
        free(module->config_file);
        dlclose(module->lib);
        free(module);
    }
}

int register_event_id(const char *event_name)
{
    printf("register_event_id:%s\n", event_name);
    event_table * event_id;
    int index = 0;
    LIST_FOREACH(event_id, &event_ids, next) {
        if(strcmp(event_id->name,event_name) == 0)
        {
            return event_id->id;
        }
        index++;
    }
    //if no match found, add it, if id's still available
    if(index >= MAX_EVENT_IDS)
    {
        printf("ERROR: could not allocate event ID, maximum amount of ID's reached (MAX_EVENT_IDS=%i)\n",MAX_EVENT_IDS);
        return -1;
    }
    event_table * new_event = malloc(sizeof(event_table));

    new_event->id = index;

    new_event->name = malloc(strlen(event_name));
    strcpy(new_event->name,event_name);

    LIST_INSERT_HEAD(&event_ids, new_event, next);
    return new_event->id;
}

char *find_event_name(int id)
{
    event_table * event_id;
    int index = 0;
    LIST_FOREACH(event_id, &event_ids, next) {
        if(event_id->id == id)
        {
            return event_id->name;
        }
        index++;
    }
    return 0;
}

int subscribe_event_id(int event_id, module_object *module)
{
    if(event_id >= MAX_EVENT_IDS)
    {
        printf("ERROR: requested event id(%i) is out of bounds(MAX_EVENT_IDS=%i)\n",event_id,MAX_EVENT_IDS);
        return -1;
    }
    event_chain *chain = event_chains[event_id];
    if(chain == NULL)
    {
        event_chains[event_id] = malloc(sizeof(event_chain));
        event_chains[event_id]->module = module;
        event_chains[event_id]->next = 0;
    }
    else
    {
        while(chain->next) // get the last valid entry
        {
            chain = chain->next;
        }
        chain->next = malloc(sizeof(event_chain));
        chain->next->module = module;
        chain->next->next = 0;
    }
    return 0;
}

//call all mapped event items
int run_callback_func(int event_id)
{
    if(event_id >= MAX_EVENT_IDS || event_id < 0)
        return -1;

    event_chain *chain = event_chains[event_id];
    while(chain)
    {
        if(chain->module->event(chain->module,event_id) != 0)
        {
            printf("ERROR during event\n");
        }
        chain = chain->next;
    }
    return 0;
}



void run_asap()
{
    while(running)
    {
    XPEDITE_TXN_SCOPE(Life);
        long start_time = current_time();
        module_object * module;
        LIST_FOREACH(module, &registered_modules, next)
        {   XPEDITE_PROBE(CodeBegin);
            if(module->run_enabled == 0)
                continue;
                    
            long start = current_time();
            if(module->run(module) != 0)
            {
                printf("ERROR during run\n");
            }
            long end = current_time();
            if(end - start > module->deadline)
            {
                printf("ERROR: module '%s' deadline(%li) overshot by %li\n",module->config_id, module->deadline, (end - start) - module->deadline);
            }
            profile(module->config_id,start,end);//log timing of this run
        }
        profile("--- overall ---",start_time, current_time());//log whole run time
        XPEDITE_PROBE(SleepBegin);
    }
}

// list modules to load, in that order, and related config-files for initialisation and instansiation
// create linked list of modules to call during run
// based on arg, set a timer to call all loaded run's, or call it continuously
// measure time of each run
// measure total time taken

//run on set interval
void run_deadline(long interval)
{
    const long slack = 10;
    long next_deadline = interval + current_time();

    while(running)
    {      
    XPEDITE_TXN_SCOPE(Life);
        long start_time = current_time();
        if(start_time >= next_deadline)
        {   XPEDITE_PROBE(CodeBegin);
            next_deadline += interval;

            module_object * module;
            LIST_FOREACH(module, &registered_modules, next)
            {
                if(module->run_enabled == 0)
                    continue;

                long start = current_time();
                if(module->run(module) != 0)
                {
                    printf("ERROR during run\n");
                }
                long end = current_time();
                if(end - start > module->deadline)
                {
                    printf("ERROR: module '%s' deadline(%li) overshot by %li\n",module->config_id, module->deadline, (end - start) - module->deadline);
                }
                profile(module->config_id,start,end);//log timing of this run
            }
            long end_time = current_time();
            profile("--- overall ---",start_time, end_time);//log whole run time

            //check next deadline criteria
            long spare = next_deadline - (end_time + slack);
            if(spare < 0)
            {
                printf("ERROR: overall deadline(%li) overshot by %li\n",interval, 0 - spare);
                //ensure the next deadline can be met
                while(spare < slack)
                {
                    next_deadline += interval;
                    spare += interval;
                    deadlines_skipped++; //global stats counter for deadlines skipped
                }
            }XPEDITE_PROBE(SleepBegin);
        }
    } 
}


void profile(char * scope, long start,long end)
{
    //printf("%s - start: %li end: %li\n", scope, start,end);
}

void test_modules()
{
    //foreach item in config
    //{
    //    test(result_file)
    //}
    //check if all results fall within acceptable range
    //check if added wordt case falls within deadline
}




