#include <stdio.h>
#ifdef __FreeBSD__
#include <netinet/in.h>
#endif
#include <ws2tcpip.h>
#include <io.h>

#include "tea_module.h"
#include "tea_core.h"

static TeaValue bind_socket(TeaVM* vm, int arg_count, TeaValue* args, bool* error);
/*static TeaValue listen_socket(TeaVM* vm, int arg_count, TeaValue* args);
static TeaValue accept_socket(TeaVM* vm, int arg_count, TeaValue* args);
static TeaValue write_socket(TeaVM* vm, int arg_count, TeaValue* args);
static TeaValue recv_socket(TeaVM* vm, int arg_count, TeaValue* args);
static TeaValue connect_socket(TeaVM* vm, int arg_count, TeaValue* args);
static TeaValue close_socket(TeaVM* vm, int arg_count, TeaValue* args);
static TeaValue setsockopt_socket(TeaVM* vm, int arg_count, TeaValue* args);*/

typedef struct 
{
    int socket;
    int family;    // Address family, e.g., AF_INET 
    int type;      // Socket type, e.g., SOCK_STREAM 
    int protocol;  // Protocol type, usually 0
} TeaSocketData;

static TeaObjectInstance* new_socket(TeaState* state, int sock, int family, int type, int protocol) 
{
    TeaObjectClass* klass = tea_new_class(state, tea_copy_string(state, "Socket", 6));

    //tea_native_function(state->vm, &klass->methods, "bind", bind_socket);
    /*tea_native_function(state->vm, &klass->methods, "listen", listen_socket);
    tea_native_function(state->vm, &klass->methods, "accept", accept_socket);
    tea_native_function(state->vm, &klass->methods, "write", write_socket);
    tea_native_function(state->vm, &klass->methods, "recv", recv_socket);
    tea_native_function(state->vm, &klass->methods, "connect", connect_socket);
    tea_native_function(state->vm, &klass->methods, "close", close_socket);
    tea_native_function(state->vm, &klass->methods, "setsockopt", setsockopt_socket);*/

    TeaObjectInstance* instance = tea_new_instance(state, klass);

    TeaObjectUserdata* data = tea_new_userdata(state, sizeof(TeaSocketData));
    tea_table_set(state, &instance->fields, tea_copy_string(state, "_data", 5), OBJECT_VAL(data));
    
    TeaSocketData* socket = (TeaSocketData*)data->data;
    socket->socket = sock;
    socket->family = family;
    socket->type = type;
    socket->protocol = protocol;

    return instance;
}

static TeaValue create_socket(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    TeaObjectInstance* socket = new_socket(vm->state, 0,0,0,0);

    return OBJECT_VAL(socket);
}

TeaValue tea_import_socket(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string(vm->state, TEA_SOCKET_MODULE, 6);
    TeaObjectModule* module = tea_new_module(vm->state, name);

    tea_native_function(vm, &module->values, "create", create_socket);

    return OBJECT_VAL(module);
}