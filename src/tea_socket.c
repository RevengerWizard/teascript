#include <stdio.h>

#ifdef __FreeBSD__
#include <netinet/in.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
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
    if(count != 2)
    {
        tea_runtime_error(vm, "bind() takes 2 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[0]))
    {
        tea_runtime_error(vm, "host passed to bind() must be a string");
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[1]))
    {
        tea_runtime_error(vm, "port passed to bind() must be a number");
        return EMPTY_VAL;
    }

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
    if(count < 0 || count > 1)
    {
        tea_runtime_error(vm, "listen() expected either 0 or 1 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    int backlog = SOMAXCONN;

    if(count == 1)
    {
        if(!IS_NUMBER(args[0]))
        {
            tea_runtime_error(vm, "listen() argument must be a string");
            return EMPTY_VAL;
        }

        backlog = AS_NUMBER(args[0]);
    }

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
    if(count != 0)
    {
        tea_runtime_error(vm, "accept() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }

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
    if(count != 1)
    {
        tea_runtime_error(vm, "write() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[0]))
    {
        tea_runtime_error(vm, "write() argument must be a string");
        return EMPTY_VAL;
    }

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
    if(count != 1)
    {
        tea_runtime_error(vm, "recv() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_NUMBER(args[0]))
    {
        tea_runtime_error(vm, "recv() argument must be a number");
        return EMPTY_VAL;
    }

    TeaValue data;
    tea_table_get(&AS_INSTANCE(instance)->fields, tea_copy_string(vm->state, "_data", 5), &data);
    TeaSocketData* socket = (TeaSocketData*)AS_USERDATA(data)->data;

    int buffer_size = AS_NUMBER(args[0]) + 1;

    if(buffer_size < 1)
    {
        tea_runtime_error(vm, "recv() argument must be greater than 1");
        return EMPTY_VAL;
    }

    char* buffer = ALLOCATE(vm->state, char, buffer_size);
    int read_size = recv(socket->socket, buffer, buffer_size - 1, 0);

    if(read_size == -1)
    {
        FREE_ARRAY(vm->state, char, buffer, buffer_size);
        return EMPTY_VAL;
    }

    // Resize string
    if(read_size != buffer_size)
    {
        buffer = GROW_ARRAY(vm->state, char, buffer, buffer_size, read_size + 1);
    }

    buffer[read_size] = '\0';
    
    return OBJECT_VAL(tea_take_string(vm->state, buffer, read_size));
}

static TeaValue connect_socket(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 2)
    {
        tea_runtime_error(vm, "recv() takes 2 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[0]))
    {
        tea_runtime_error(vm, "host passed to connect() must be a string");
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[1]))
    {
        tea_runtime_error(vm, "port passed to connect() must be a number");
        return EMPTY_VAL;
    }

    TeaValue data;
    tea_table_get(&AS_INSTANCE(instance)->fields, tea_copy_string(vm->state, "_data", 5), &data);
    TeaSocketData* socket = (TeaSocketData*)AS_USERDATA(data)->data;

    struct sockaddr_in server;

    server.sin_family = socket->family;
    server.sin_addr.s_addr = inet_addr(AS_CSTRING(args[0]));
    server.sin_port = htons(AS_NUMBER(args[1]));

    if(connect(socket->socket, (struct sockaddr*)&server, sizeof(server)) < 0)
    {
        return NULL_VAL;
    }

    return EMPTY_VAL;
}

static TeaValue close_socket(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "close() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    TeaValue data;
    tea_table_get(&AS_INSTANCE(instance)->fields, tea_copy_string(vm->state, "_data", 5), &data);
    TeaSocketData* socket = (TeaSocketData*)AS_USERDATA(data)->data;

    close(socket->socket);
    return EMPTY_VAL;
}

static TeaValue setsockopt_socket(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 2)
    {
        tea_runtime_error(vm, "setsockopt() takes 2 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_NUMBER(args[0]) && !IS_NUMBER(args[1]))
    {
        tea_runtime_error(vm, "setsockopt() arguments must be numbers");
        return EMPTY_VAL;
    }

    TeaValue data;
    tea_table_get(&AS_INSTANCE(instance)->fields, tea_copy_string(vm->state, "_data", 5), &data);
    TeaSocketData* socket = (TeaSocketData*)AS_USERDATA(data)->data;

    int level = AS_NUMBER(args[1]);
    int option = AS_NUMBER(args[2]);

    if(setsockopt(socket->socket, level, option, &(int){1}, sizeof(int)) == -1)
    {
        return NULL_VAL;
    }

    return EMPTY_VAL;
}

static TeaObjectInstance* new_socket(TeaState* state, int sock, int family, int type, int protocol) 
{
    TeaObjectClass* klass = tea_new_class(state, tea_copy_string(state, "Socket", 6), NULL);
    TeaObjectInstance* instance = tea_new_instance(state, klass);

    TeaObjectData* data = tea_new_data(state, sizeof(TeaSocketData));
    tea_table_set(state, &instance->fields, tea_copy_string(state, "_data", 5), OBJECT_VAL(data));

    TeaSocketData* socket = (TeaSocketData*)data->data;
    socket->socket = sock;
    socket->family = family;
    socket->type = type;
    socket->protocol = protocol;

    tea_native_method(state->vm, &instance->fields, "bind", bind_socket);
    tea_native_method(state->vm, &instance->fields, "listen", listen_socket);
    tea_native_method(state->vm, &instance->fields, "accept", accept_socket);
    tea_native_method(state->vm, &instance->fields, "write", write_socket);
    tea_native_method(state->vm, &instance->fields, "recv", recv_socket);
    tea_native_method(state->vm, &instance->fields, "connect", connect_socket);
    tea_native_method(state->vm, &instance->fields, "close", close_socket);
    tea_native_method(state->vm, &instance->fields, "setsockopt", setsockopt_socket);

    tea_native_property(state->vm, &instance->fields, "socket", socket_socket);
    tea_native_property(state->vm, &instance->fields, "family", family_socket);
    tea_native_property(state->vm, &instance->fields, "type", type_socket);
    tea_native_property(state->vm, &instance->fields, "protocol", protocol_socket);

    return instance;
}

static TeaValue create_socket(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 2)
    {
        tea_runtime_error(vm, "create() takes 2 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_NUMBER(args[0]) || !IS_NUMBER(args[1]))
    {
        tea_runtime_error(vm, "create() arguments must be numbers");
        return EMPTY_VAL;
    }

    int socket_family = AS_NUMBER(args[0]);
    int socket_type = AS_NUMBER(args[1]);

    int sock = socket(socket_family, socket_type, 0);
    if(sock == -1)
    {
        return NULL_VAL;
    }

    TeaObjectInstance* socket = new_socket(vm->state, sock, socket_family, socket_type, 0);

    return OBJECT_VAL(socket);
}

#ifdef _WIN32
void cleanup_sockets(void) 
{
    // Calls WSACleanup until an error occurs.
    // Avoids issues if WSAStartup is called multiple times.
    while(!WSACleanup());
}
#endif

TeaValue tea_import_socket(TeaVM* vm)
{
    #ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    atexit(cleanup_sockets);
    WORD version_wanted = MAKEWORD(2, 2);
    WSADATA wsa_data;
    WSAStartup(version_wanted, &wsa_data);
    #endif

    TeaObjectString* name = tea_copy_string(vm->state, TEA_SOCKET_MODULE, 6);
    TeaObjectModule* module = tea_new_module(vm->state, name);

    tea_native_function(vm, &module->values, "create", create_socket);

    tea_native_value(vm, &module->values, "AF_INET", NUMBER_VAL(AF_INET));
    tea_native_value(vm, &module->values, "SOCK_STREAM", NUMBER_VAL(SOCK_STREAM));
    tea_native_value(vm, &module->values, "SOL_SOCKET", NUMBER_VAL(SOL_SOCKET));
    tea_native_value(vm, &module->values, "SO_REUSEADDR", NUMBER_VAL(SO_REUSEADDR));

    return OBJECT_VAL(module);
}