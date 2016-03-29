#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gfserver.h"
#include "content.h"
#include <pthread.h>
#include "steque.h"

#define BUFFER_SIZE 80

pthread_mutex_t req_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  req_cond = PTHREAD_COND_INITIALIZER;
steque_t        request_queue;


typedef struct request_t{
    gfcontext_t *ctx;
    char *path;
    void* arg;
} request_t;



typedef struct thread_info_t {
    int         thread_num;   //ID
    pthread_t   thread_id;    //Value returned from pthread_create()
    char*       name;
    request_t*  request;
}thread_info_t;

void error(const char* _message){ perror(_message); exit(0);}

ssize_t handler_perform(gfcontext_t *ctx, char *path, void* arg);


/* Function call by individual threads.  Threads atomically 
 * dequeue requests or wait till queue is not empty
 * thread_info can be used for debugging */
void thread_main(void* _thread_info){
    pthread_mutex_lock(&req_mutex);
    while(1){
        while(steque_isempty(&request_queue))
            pthread_cond_wait(&req_cond, &req_mutex);
        
        steque_node_t* node = steque_pop(&request_queue);
        request_t* req = (request_t*) node->item;
        pthread_mutex_unlock(&req_mutex);

        handler_perform(req->ctx, req->path, req->arg);
    }
    
    free(_thread_info);
}

void handler_init(int _num_threads){
    char name[BUFFER_SIZE];
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    thread_info_t* thread_info = calloc(_num_threads, sizeof(thread_info_t));
    
        
    for (int i = 0; i < _num_threads; i++) {
        memset(name, 0, BUFFER_SIZE);
        snprintf(name, BUFFER_SIZE,"thread_%d",i);
        thread_info[i].name = strdup(name);
        thread_info[i].thread_num = i;
    
        int success = pthread_create(&thread_info[i].thread_id, &attributes, (void*)&thread_main, &thread_info[i]);
        if (success != 0) error("pthread_create failed");
    
    }
    
    steque_init(&request_queue);
    pthread_attr_destroy(&attributes);

}

ssize_t handler_perform(gfcontext_t *ctx, char *path, void* arg){
    int fildes;
    ssize_t file_len, bytes_transferred;
    ssize_t read_len, write_len;
    char buffer[BUFFER_SIZE];
    
    if( 0 > (fildes = content_get(path)))
        return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
    
    /* Calculating the file size */
    file_len = lseek(fildes, 0, SEEK_END);
    
    gfs_sendheader(ctx, GF_OK, file_len);
    
    /* Sending the file contents chunk by chunk. */
    bytes_transferred = 0;
    while(bytes_transferred < file_len){
        read_len = pread(fildes, buffer, BUFFER_SIZE, bytes_transferred);
        if (read_len <= 0){
            fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
            gfs_abort(ctx);
            return -1;
        }
        write_len = gfs_send(ctx, buffer, read_len);
        if (write_len != read_len){
            fprintf(stderr, "handle_with_file write error");
            gfs_abort(ctx);
            return -1;
        }
        bytes_transferred += write_len;
    }
    
    return bytes_transferred;
}


ssize_t handler_get(gfcontext_t *ctx, char *path, void* arg){
    steque_node_t* node = calloc(1, sizeof(steque_node_t));
    request_t* request = calloc(1, sizeof(request_t));
    
    request->ctx = ctx;
    request->path = strdup(path);
    request->arg = arg;
    node->item = (void*)request;
    
    pthread_mutex_lock(&req_mutex);
    steque_enqueue(&request_queue, node);
    
    pthread_cond_signal(&req_cond);
    pthread_mutex_unlock(&req_mutex);

    return 0;
}

