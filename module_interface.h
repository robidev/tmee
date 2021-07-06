#ifndef interface_h__
#define interface_h__

#include <sys/queue.h> 

typedef struct _module_object module_object;
typedef struct _module_callbacks module_callbacks;

// module functions that tmee will call
typedef int (*pre_init_func)(module_object *instance, module_callbacks *callbacks);
typedef int (*init_func)(module_object *instance, module_callbacks *callbacks);
typedef int (*run_func)(module_object *instance);
typedef int (*event_func)(module_object *instance, int event_id);
typedef int (*test_func)();
typedef int (*deinit_func)(module_object *instance);

// functions to be called from a module into the host process
typedef int (*register_event_func)(const char *event_name);
typedef int (*callback_event_func)(int id);
//typedef int (*register_callback_func)(int event_id, module_object *module);

// structures used for the module and callbacks
struct _module_object {
    // pointer to loaded library
    void * lib;
    // function pointer to functions loaded from library
    pre_init_func pre_init;
    init_func init;
    run_func run;
    event_func event;
    test_func test;
    deinit_func deinit;
    // pointer to all module specific data
    void * module_data;
    char *config_id;
    char * config_file;
    long deadline;
    LIST_ENTRY(_module_object) next;
};

struct _module_callbacks {
    register_event_func register_event_cb;
    callback_event_func callback_event_cb;
    //register_callback_func register_callback_cb;
};



//called right after module load
extern int pre_init(module_object *instance, module_callbacks *callbacks);

//called after all modules are loaded
extern int init(module_object *instance, module_callbacks *callbacks);

//real time call
extern int run(module_object *instance);

extern int event(module_object *instance, int event_id);

//called to generate real-time performance metrics on this machine
extern int test(void);

extern int deinit(module_object *instance);


#endif  // interface_h__
