
#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gfserver.h"
#include <time.h>
#include "shm_channel.h"

extern mqd_t tx_mqd;
extern mqd_t rx_mqd;

ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg)
{
    char buffer[MSG_SIZE];
    struct timespec timeout;

    unsigned long file_size = 5000000;
    int bytes_received = 0, total_bytes_received = 0, gfs_bytes_sent;
    unsigned int priority;
    
    fprintf(stderr,"Sending tx request..\n");
    send_message(tx_mqd, path, strlen(path) + 1, 0);
    
    steque_t* segment_q = (steque_t*) arg;
    
    ASSERT(segment_q != NULL);
    
    segment_item* seg = (segment_item*)steque_front(segment_q);
    
    //sprintf(seg->mem, "%s", "Hi to cache (from proxy)");

    
    ////fprintf(stderr,"Request sent...\n");
    
    /*
    while(steque_isempty(&request_queue))
        pthread_cond_wait(&req_cond, &req_mutex);
    
    steque_node_t* node = steque_pop(&request_queue);
    request_t* req = (request_t*) node->item;
    pthread_mutex_unlock(&req_mutex);
    */
    

    fprintf(stderr,"----------------------\n");

    do{

        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 3;
        bytes_received = mq_timedreceive(rx_mqd, &buffer[0], sizeof(buffer), &priority, &timeout);

        ASSERT(bytes_received != 0);
        //timeout reached or error connecting, loop
        if(bytes_received == ERROR){
            continue;
        }
        
        if(priority == NOT_FOUND_PRIORITY){
            perror("priority == NOT_FOUND_PRIORITY");
            return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
        }
        else if(priority == HEADER_PRIORITY){
            fprintf(stderr,"File size = %s\n", buffer);
            file_size = atoi(buffer);
            gfs_sendheader(ctx, GF_OK, atoi(buffer));
            continue;
        }
        


        if (bytes_received > 0){
            fprintf(stderr,"Recieved data, bytes = %d\n", bytes_received);
            gfs_bytes_sent = gfs_send(ctx, buffer, bytes_received);
            if (gfs_bytes_sent != bytes_received){
                perror("gfs_bytes_sent != bytes_received");
                return EXIT_FAILURE;
            }
            total_bytes_received += bytes_received;
            if(total_bytes_received == file_size){
                fprintf(stderr,"Transfer Complete!\n");
                fprintf(stderr,"----------------------\n");
            }
        }
    }
    while(total_bytes_received < file_size);
    
    //fprintf(stderr,"File received, bytes = %d, file_size = %lu\n", total_bytes_received, file_size);

    fprintf(stderr, "Reading Segment: %s \n", (char*)seg->mem);

    
    return 0;
}
