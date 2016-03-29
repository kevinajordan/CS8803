
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
    
    ////fprintf(stderr,"Request sent...\n");

    

    do{
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 3;
        bytes_received = mq_timedreceive(rx_mqd, &buffer[0], sizeof(buffer), &priority, &timeout);
        printf("Timeout\n");
        continue;
        if(priority == NOT_FOUND_PRIORITY){
            //fprintf(stderr,"File not found, cache miss\n");
            return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
        }
        else if(priority == HEADER_PRIORITY){
            //fprintf(stderr,"File size (string) = %s, bytes_received = %d\n", buffer, bytes_received);
            file_size = atoi(buffer);
            gfs_sendheader(ctx, GF_OK, atoi(buffer));
            continue;
        }
        
        
        if(bytes_received == ERROR){
            //fprintf(stderr,"Waiting for response, sleeping (5)..\n\n");
            sleep(5);
            continue;
        }
        else if (bytes_received > 0){
            fprintf(stderr,"Recieved data, bytes = %d\n", bytes_received);
            gfs_bytes_sent = gfs_send(ctx, buffer, bytes_received);
            if (gfs_bytes_sent != bytes_received){
                //fprintf(stderr,stderr, "handle_with_file write error");
                return EXIT_FAILURE;
            }
            total_bytes_received += bytes_received;
        }
    }
    while(total_bytes_received < file_size);
    
    fprintf(stderr,"File received, bytes = %d, file_size = %lu\n", total_bytes_received, file_size);


    
    return 0;
}