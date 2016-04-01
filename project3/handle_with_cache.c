
#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gfserver.h"
#include <time.h>
#include "shm_channel.h"

#define MAX_THRD 50

extern mqd_t tx_mqd;
extern mqd_t rx_mqd;

   
ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg)
{
  
    thread_packet thr_pkt;
    memcpy(thr_pkt.requested_file, path, strlen(path));
    thr_pkt.segment_size = 0;
    thr_pkt.chunk_size = 0;
    int gfs_bytes_sent;
    

    
    steque_t* segment_q = (steque_t*) arg;
    
    
    ASSERT(segment_q != NULL);
    
    segment_item* seg = NULL;
    
    while(1){
        seg = (segment_item*)steque_front(segment_q);
        steque_cycle(segment_q);
        if(seg->segment_index == 9)
            break;
    }
    
    //sprintf(seg->mem, "%s", "Hi to cache (from proxy)");

    
    ////fprintf(stderr,"Request sent...\n");
    
    /*
    while(steque_isempty(&request_queue))
        pthread_cond_wait(&req_cond, &req_mutex);
    
    steque_node_t* node = steque_pop(&request_queue);
    request_t* req = (request_t*) node->item;
    pthread_mutex_unlock(&req_mutex);
    */
    //struct timespec timeout;

    //fprintf(stderr, "Segment id = %s\n", seg->segment_id);
    

    
    int n;
    int total_bytes_rx = 0;
    enum sm state = SM_GET_FILESIZE;
    
    
    //fprintf(stderr,"Maximum # of messages on queue: %ld\n", attr.mq_maxmsg);
    //fprintf(stderr,"Maximum message size: %ld\n", attr.mq_msgsize);
    //fprintf(stderr,"# of messages currently on queue: %ld\n", attr.mq_curmsgs);
    

    fprintf(stderr,"----------------------\n");
    fprintf(stderr,"Sending tx request.. path: %s\n", thr_pkt.requested_file);
    send_message(tx_mqd, (void*)&thr_pkt, sizeof(thr_pkt), 0);

    do{
        if(state == SM_GET_FILESIZE){
            //clock_gettime(CLOCK_REALTIME, &timeout);
            //timeout.tv_sec += 3;
            fprintf(stderr, "Wainting for response (cache hit/miss)...\n");
            n = mq_receive(rx_mqd, (void*)&thr_pkt, sizeof(thr_pkt), 0); //&timeout);
            
            //memset(seg->segment_ptr, 0, thr_pkt.segment_size);
            //       fprintf(stderr, "Reading Segment: %s \n", (char*)seg->segment_ptr);
            
            if(n == ERROR){
                perror("filesize_mq wrong");
                return EXIT_FAILURE;
            }
          
            fprintf(stderr,"Received response: hit/miss: %d, file size = %d, segment_size = %d, chunk_size = %d\n", thr_pkt.cache_hit, thr_pkt.file_size, thr_pkt.segment_size, thr_pkt.chunk_size);
            gfs_sendheader(ctx, GF_OK, thr_pkt.file_size);
            state = SM_GET_DATA;
        }
        else if(state == SM_GET_DATA){
            fprintf(stderr,"Waiting on data response..\n");

            n = mq_receive(rx_mqd, (void*)&thr_pkt, sizeof(thr_pkt), 0); //&timeout);
            
            fprintf(stderr,"Received response..\n");


            ASSERT(n != 0);
            if(n == ERROR){
                continue;
            }
            
    
            

            gfs_bytes_sent = gfs_send(ctx, seg->segment_ptr, thr_pkt.chunk_size);
            if (gfs_bytes_sent != thr_pkt.chunk_size){
                perror("gfs_bytes_sent != bytes_received");
                return EXIT_FAILURE;
            }
            
            total_bytes_rx += thr_pkt.chunk_size;
            if(total_bytes_rx == thr_pkt.file_size){
                fprintf(stderr,"Transfer Complete!\n");
                fprintf(stderr,"----------------------\n");
            }
            
            send_message(tx_mqd, (void*)&thr_pkt, sizeof(thr_pkt), 0);
        }
    }
    while(total_bytes_rx < thr_pkt.file_size);
    
    //fprintf(stderr,"File received, bytes = %d, file_size = %lu\n", total_bytes_received, file_size);


    
    return 0;
}

/*
 if(state priority == NOT_FOUND_PRIORITY){
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
 */
