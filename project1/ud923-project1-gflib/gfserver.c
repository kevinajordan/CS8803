#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <assert.h>

#include "gfserver.h"

#define MISC        16
#define MAX_SCHEME  7
#define MAX_METHOD  5
#define MAX_SERVER  128
#define MAX_PATH    255
#define BUFSIZE     (MAX_PATH + MAX_METHOD + MAX_SCHEME + MISC)
#define ENDSEQ_SZ   4
#define GET_SZ      4 // + space

typedef enum{
    SM_SCHEME,
    SM_METHOD,
    SM_PATH,
} gfstatemachine_t;

char*   response_status[] = {"OK", "FILE_NOT_FOUND", "ERROR"};
char*   gfs_strstatus(gfstatus_t status);
void    initialize_socket(gfserver_t * _gfs);
int     accept_rx_connection(int _socket_fd);
int     process_request(gfcontext_t* _ctx);
gfcontext_t* gfcontext_create(gfserver_t* _gfs);
void    error(const char* _message){perror(_message); exit(0);}
void    set_timeouts(gfcontext_t* _ctx);
int     parse_scheme(gfcontext_t* _ctx);
int     parse_method(gfcontext_t* _ctx);
int     parse_path(gfcontext_t* _ctx);

typedef struct gfserver_t {
    unsigned short port;
    int max_npending;
    int socket_fd;
    int status;
    ssize_t (*server_handler)(gfcontext_t *, char *, void*);
    void* handler_arg;
}gfserver_t;

typedef struct gfcontext_t{
    gfserver_t* gfs;
    int context_fd;
    char* path;
    int path_length;
    int status;
    int rx_bytes;
    int total_rx_bytes;
    char buffer[BUFSIZE];
    unsigned header_pt;
    gfstatemachine_t sm;
}gfcontext_t;


void gfserver_serve(gfserver_t *_gfs){
    initialize_socket(_gfs);
    if(_gfs->status == GF_ERROR) return;
    
    while(1){
        gfcontext_t* ctx = gfcontext_create(_gfs);
        ctx->context_fd = accept_rx_connection(_gfs->socket_fd);
        int status = process_request(ctx);
        
        //handler callback should not be called when the request is malformed.
        if(status == -1) _gfs->server_handler(ctx, ctx->path, _gfs->handler_arg);
        else             gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);

        shutdown(ctx->context_fd, SHUT_RDWR);
        free(ctx);
    }
    free(_gfs);
}

/* Function intializes socket structures */
void initialize_socket(gfserver_t * _gfs){
    _gfs->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_gfs->socket_fd < 0)
        _gfs->status = GF_ERROR;
    
    struct sockaddr_in server_addr;
    bzero((char *) &server_addr, sizeof(server_addr));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(_gfs->port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    int enable = 1;
    setsockopt(_gfs->socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    
    int success = bind(_gfs->socket_fd, (struct sockaddr*) &server_addr, sizeof(server_addr));
    if(success < 0) _gfs->status = GF_ERROR;
    
    int n = listen(_gfs->socket_fd, _gfs->max_npending);
    if(n < 0) _gfs->status = GF_ERROR;
}

/* Accept incoming connect, accept() is guaranteed to pass if valid socket_fd */
int accept_rx_connection(int _socket_fd){
    char buffer[BUFSIZE];
    bzero(buffer, BUFSIZE);

    struct sockaddr_in client_addr;
    socklen_t client_size = sizeof(client_addr);
    
    return accept(_socket_fd, (struct sockaddr*) &client_addr, &client_size);
}

/* Process request, follows 'GETFILE' protocol
 * returns -1 if error occurred but handler should still be called
 * returns -2 if error occurred and handler should NOT be called. */
int process_request(gfcontext_t* _ctx){
    int r = 0;
    set_timeouts(_ctx);
    
    while(1){
        int n = read(_ctx->context_fd, _ctx->buffer + _ctx->header_pt, BUFSIZE - _ctx->header_pt);

        if(n < 0)  _ctx->status = GF_ERROR;
        if(n == 0) _ctx->status = GF_FILE_NOT_FOUND;
        
        if(_ctx->status != GF_OK) return -1;
        
        _ctx->rx_bytes += n;
        _ctx->total_rx_bytes += n;
        
        if(_ctx->sm == SM_SCHEME)  r = parse_scheme(_ctx);
        if(_ctx->sm == SM_METHOD)  r = parse_method(_ctx);
        if(_ctx->sm == SM_PATH)    r = parse_path(_ctx);
        
        if(r == -1) return -2;

    }
}

/*
 * Sendheader and send are used by callback routine to send protocol data to client
 */
ssize_t gfs_sendheader(gfcontext_t* _ctx, gfstatus_t _status, size_t _file_len){
    char buffer[BUFSIZE];
    bzero(buffer, BUFSIZE);
    sprintf(buffer, "GETFILE %s %zu\r\n\r\n", gfs_strstatus(_status), _file_len);
    
    return write(_ctx->context_fd, buffer, strlen(buffer));
}

ssize_t gfs_send(gfcontext_t* _ctx, void* _data, size_t _len){
    char buffer[BUFSIZE];
    bzero(buffer, BUFSIZE);
    
    return write(_ctx->context_fd, _data, _len);
}


/* Parse scheme part of the protocol */
int parse_scheme(gfcontext_t* _ctx){
    if(_ctx->rx_bytes >= MAX_SCHEME) {
        char* tok = strtok (_ctx->buffer," ");;
        if(!tok || strncmp(_ctx->buffer, "GETFILE", MAX_SCHEME)){
            _ctx->status = GF_FILE_NOT_FOUND;
            return -1;
        }
        else{
            _ctx->header_pt = (int)strlen(tok) + 1;
            _ctx->sm = 1;
        }
    }
    return 0;
}

/* Parse method */
int parse_method(gfcontext_t* _ctx){
    if(_ctx->rx_bytes - _ctx->header_pt >= GET_SZ){
    
        char* tok = strtok (NULL, " ");
        if(tok && !strcmp(tok, "GET")){
            _ctx->header_pt += (int)strlen(tok) + 1;  //covers spaces, points to first digit
            _ctx->sm = 2;
        }
        else{
            _ctx->status = GF_FILE_NOT_FOUND;
            return -1;
        }
    }
    return 0;
}

/* Parse path and end sequence */
int parse_path(gfcontext_t* _ctx){
    
    //Look for whitespace end sequence
    char* tok = strtok (NULL, " ");
    if(tok){
        _ctx->path = strdup(tok);
        _ctx->path_length = _ctx->header_pt + (int)strlen(tok);
    }
    else{
        //Look for \r\n\r\n end sequence
        tok = strtok (NULL, "\r");
        if(tok){
            _ctx->path = strdup(tok);
            _ctx->path_length = _ctx->header_pt + (int)strlen(tok);
            _ctx->header_pt += (int)strlen(tok) + 1;
            if(strcmp(&(_ctx->buffer[_ctx->header_pt]), "\r\n\r\n") != 0)
                _ctx->status = GF_FILE_NOT_FOUND;
        }
        else if(_ctx->total_rx_bytes > (MAX_PATH + GET_SZ + MAX_SCHEME))
            _ctx->status = GF_FILE_NOT_FOUND;
    }
    
    if(_ctx->status == GF_FILE_NOT_FOUND) return -1;
    else return 0;
}

void gfs_abort(gfcontext_t *ctx){
    shutdown(ctx->context_fd, SHUT_RDWR);
    shutdown(ctx->gfs->socket_fd, SHUT_RDWR);
}

gfserver_t* gfserver_create(){
    gfserver_t* gfs = malloc(sizeof(gfserver_t));
    memset(gfs,0,sizeof(gfserver_t));
    gfs->port = 80;
    gfs->max_npending = 5;
    gfs->status = GF_OK;
    gfs->socket_fd = -1;
    gfs->server_handler = NULL;
    gfs->handler_arg = NULL;
    return gfs;
}

gfcontext_t* gfcontext_create(gfserver_t* _gfs){
    gfcontext_t* ctx = malloc(sizeof(gfcontext_t));
    memset(ctx,0,sizeof(gfcontext_t));
    ctx->gfs = _gfs;
    ctx->context_fd = -1;
    ctx->path = NULL;
    ctx->path_length = -1;
    ctx->status = GF_OK;
    ctx->rx_bytes = 0;
    ctx->total_rx_bytes = 0;
    ctx->header_pt = 0;
    ctx->sm = SM_SCHEME;
    bzero(ctx->buffer, BUFSIZE);
    return ctx;
}


char* gfs_strstatus(gfstatus_t status){
    char* ret;
    switch(status){
        case   GF_OK:
            ret = response_status[0];
            break;
        case GF_FILE_NOT_FOUND:
            ret = response_status[1];
            break;
        case GF_ERROR:
            ret = response_status[2];
            break;
        default:
            ret = response_status[3];
    }
    
    return ret;
}

void gfserver_set_port(gfserver_t *_gfs, unsigned short _port){
    _gfs->port = _port;
}

void gfserver_set_maxpending(gfserver_t * _gfs, int _max_npending){
    _gfs->max_npending = _max_npending;
}

void gfserver_set_handler(gfserver_t* _gfs, ssize_t (*_handler)(gfcontext_t *, char *, void*)){
    _gfs->server_handler = _handler;
}

void gfserver_set_handlerarg(gfserver_t* _gfs, void* _arg){
    _gfs->handler_arg = _arg;
}

void set_timeouts(gfcontext_t* _ctx){
    struct timeval timeout = {1, 0};  //Timeout = 1s
    
    setsockopt(_ctx->context_fd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&timeout, sizeof(struct timeval));
    setsockopt(_ctx->context_fd, SOL_SOCKET, SO_SNDTIMEO, (struct timeval *)&timeout, sizeof(struct timeval));
}
