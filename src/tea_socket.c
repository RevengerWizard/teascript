#include <stdio.h>

#ifdef __FreeBSD__
#include <netinet/in.h>
#endif

#ifdef _WIN32
#include <ws2tcpip.h>
#include <io.h>
#define setsockopt(S, LEVEL, OPTNAME, OPTVAL, OPTLEN) setsockopt(S, LEVEL, OPTNAME, (char*)(OPTVAL), OPTLEN)

#ifndef __MINGW32__
    // Fixes deprecation warning
    unsigned long inet_addr_new(const char* cp) 
    {
        unsigned long S_addr;
        inet_pton(AF_INET, cp, &S_addr);
        return S_addr;
    }
#define inet_addr(cp) inet_addr_new(cp)
#endif

#define write(fd, buffer, count) _write(fd, buffer, count)
#define close(fd) closesocket(fd)
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

#include "tea_module.h"
#include "tea_core.h"

typedef struct 
{
    int socket;
    int family;    // Address family, e.g., AF_INET 
    int type;      // Socket type, e.g., SOCK_STREAM 
    int protocol;  // Protocol type, usually 0
} TeaSocketData;

static TeaObjectInstance* new_socket(TeaState* state, int sock, int family, int type, int protocol);

static TeaValue socket_socket(TeaVM* vm, TeaValue instance)
{
    TeaValue data;
    tea_table_get(&AS_INSTANCE(instance)->fields, tea_copy_string(vm->state, "_data", 5), &data);
    TeaSocketData* socket = (TeaSocketData*)AS_USERDATA(data)->data;

    return NUMBER_VAL(socket->socket);
}

static TeaValue family_socket(TeaVM* vm, TeaValue instance)
{
    TeaValue data;
    tea_table_get(&AS_INSTANCE(instance)->fields, tea_copy_string(vm->state, "_data", 5), &data);
    TeaSocketData* socket = (TeaSocketData*)AS_USERDATA(data)->data;

    return NUMBER_VAL(socket->family);
}

static TeaValue type_socket(TeaVM* vm, TeaValue instance)
{
    TeaValue data;
    tea_table_get(&AS_INSTANCE(instance)->fields, tea_copy_string(vm->state, "_data", 5), &data);
    TeaSocketData* socket = (TeaSocketData*)AS_USERDATA(data)->data;

    return NUMBER_VAL(socket->type);
}

static TeaValue protocol_socket(TeaVM* vm, TeaValue instance)
{
    TeaValue data;
    tea_table_get(&AS_INSTANCE(instance)->fields, tea_copy_string(vm->state, "_data", 5), &data);
    TeaSocketData* socket = (TeaSocketData*)AS_USERDATA(data)->data;

    return NUMBER_VAL(socket->protocol);
}

static TeaValue bind_socket(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    TeaValue data;
    tea_table_get(&AS_INSTANCE(instance)->fields, tea_copy_string(vm->state, "_data", 5), &data);
    TeaSocketData* socket = (TeaSocketData*)AS_USERDATA(data)->data;

    char* host = AS_CSTRING(args[0]);
    int port = AS_NUMBER(args[1]);

    struct sockaddr_in server;

    server.sin_family = socket->family;
    server.sin_addr.s_addr = inet_addr(host);
    server.sin_port = htons(port);

    if(bind(socket->socket, (struct sockaddr*)&server, sizeof(server)) < 0)
    {
        return NULL_VAL;
    }

    return EMPTY_VAL;
}

static TeaValue listen_socket(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    int backlog = SOMAXCONN;

    TeaValue data;
    tea_table_get(&AS_INSTANCE(instance)->fields, tea_copy_string(vm->state, "_data", 5), &data);
    TeaSocketData* socket = (TeaSocketData*)AS_USERDATA(data)->data;

    if(listen(socket->socket, backlog) == -1)
    {
        return NULL_VAL;
    }

    return EMPTY_VAL;
}

static TeaValue accept_socket(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    TeaValue data;
    tea_table_get(&AS_INSTANCE(instance)->fields, tea_copy_string(vm->state, "_data", 5), &data);
    TeaSocketData* socket = (TeaSocketData*)AS_USERDATA(data)->data;

    struct sockaddr_in client;
    int c = sizeof(struct sockaddr_in);
    int new = accept(socket->socket, (struct sockaddr*)&client, (socklen_t*)&c);

    if(new < 0)
    {
        return NULL_VAL;
    }

    TeaObjectList* list = tea_new_list(vm->state);
    TeaObjectInstance* socket_instance = new_socket(vm->state, new, socket->family, socket->protocol, 0);

    tea_write_value_array(vm->state, &list->items, OBJECT_VAL(socket_instance));

    // IPv6 is 39 chars
    char ip[40];
    inet_ntop(socket->family, &client.sin_addr, ip, 40);
    TeaObjectString* string = tea_copy_string(vm->state, ip, strlen(ip));

    tea_write_value_array(vm->state, &list->items, OBJECT_VAL(string));

    return OBJECT_VAL(list);
}

static TeaValue write_socket(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    TeaValue data;
    tea_table_get(&AS_INSTANCE(instance)->fields, tea_copy_string(vm->state, "_data", 5), &data);
    TeaSocketData* socket = (TeaSocketData*)AS_USERDATA(data)->data;

    TeaObjectString* message = AS_STRING(args[0]);

    int write_ret = write(socket->socket, message->chars, message->length);

    if(write_ret == -1)
    {
        return NULL_VAL;
    }

    return NUMBER_VAL(write_ret);
}

static TeaValue recv_socket(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{

}

static TeaValue connect_socket(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{

}

static TeaValue close_socket(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{

}

static TeaValue setsockopt_socket(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{

}

static TeaObjectInstance* new_socket(TeaState* state, int sock, int family, int type, int protocol) 
{
    TeaObjectClass* klass = tea_new_class(state, tea_copy_string(state, "Socket", 6));
    TeaObjectInstance* instance = tea_new_instance(state, klass);

    TeaObjectUserdata* data = tea_new_userdata(state, sizeof(TeaSocketData));
    tea_table_set(state, &instance->fields, tea_copy_string(state, "_data", 5), OBJECT_VAL(data));

    TeaSocketData* socket = (TeaSocketData*)data->data;
    socket->socket = sock;
    socket->family = family;
    socket->type = type;
    socket->protocol = protocol;

    //tea_native_method(state->vm, &instance->fields, "bind", bind_socket);
    //tea_native_method(state->vm, &instance->fields, "listen", listen_socket);
    //tea_native_method(state->vm, &instance->fields, "accept", accept_socket);
    //tea_native_method(state->vm, &instance->fields, "write", write_socket);
    //tea_native_method(state->vm, &instance->fields, "recv", recv_socket);
    //tea_native_method(state->vm, &instance->fields, "connect", connect_socket);
    //tea_native_method(state->vm, &instance->fields, "close", close_socket);
    //tea_native_method(state->vm, &instance->fields, "setsockopt", setsockopt_socket);

    tea_native_property(state->vm, &instance->fields, "socket", socket_socket);
    tea_native_property(state->vm, &instance->fields, "family", family_socket);
    tea_native_property(state->vm, &instance->fields, "type", type_socket);
    tea_native_property(state->vm, &instance->fields, "protocol", protocol_socket);

    return instance;
}

static TeaValue create_socket(TeaVM* vm, int count, TeaValue* args)
{
    TeaObjectInstance* socket = new_socket(vm->state, 123, 454, 1212, 0);

    return OBJECT_VAL(socket);
}

TeaValue tea_import_socket(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string(vm->state, TEA_SOCKET_MODULE, 6);
    TeaObjectModule* module = tea_new_module(vm->state, name);

    tea_native_function(vm, &module->values, "create", create_socket);

    tea_native_value(vm, &module->values, "AF_INET", NUMBER_VAL(AF_INET));
    tea_native_value(vm, &module->values, "SOCK_STREAM", NUMBER_VAL(SOCK_STREAM));
    tea_native_value(vm, &module->values, "SOL_SOCKET", NUMBER_VAL(SOL_SOCKET));
    tea_native_value(vm, &module->values, "SO_REUSEADDR", NUMBER_VAL(SO_REUSEADDR));

    return OBJECT_VAL(module);
}