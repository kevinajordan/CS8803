
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

//extern mqd_t ctrl_mq_tx;
//extern mqd_t ctrl_mq_rx;
extern pthread_mutex_t  seg_mutex;
extern pthread_cond_t   seg_cond;
extern steque_t         seg_queue;

   
ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg)
{
  
    thread_packet thr_pkt;
    memcpy(thr_pkt.requested_file, path, strlen(path));
    thr_pkt.segment_size = 0, thr_pkt.chunk_size = 0;
    int gfs_bytes_sent num_segments = steque_size(segment_q);
;
    steque_t* segment_q = (steque_t*) arg;
    
    int n;
    int total_bytes_rx = 0;
    enum sm state = SM_GET_FILESIZE;
    
    
    /* Get the next available segment, thread-safe */
    pthread_mutex_lock(&seg_mutex);

    while(steque_isempty(segment_q))
        pthread_cond_wait(&seg_cond, &seg_mutex);

    seg = steque_pop(segment_q);
    pthread_mutex_unlock(&seg_mutex);
    
    
    fprintf(stderr,"Thread procesisng.. thr_id=%x path: %s\n", (int)pthread_self(), thr_pkt.requested_file);
    

    
    //Send request, inform cache of the segment to use
    thr_pkt.segment_index = seg->segment_index;
    send_message(seg->ctrl_mq_tx, (void*)&thr_pkt, sizeof(thr_pkt), ctrl_priority);

    fprintf(stderr,"Sending tx request.. seg idx=%d, thr_id=%x path: %s\n", thr_pkt.segment_index, (int)pthread_self(), thr_pkt.requested_file);
    
    do{
        if(state == SM_GET_FILESIZE){
            //clock_gettime(CLOCK_REALTIME, &timeout);
            //timeout.tv_sec += 3;
            //fprintf(stderr, "Wainting for response (cache hit/miss)...\n");
            n = mq_receive(seg->ctrl_mq_rx, (void*)&thr_pkt, sizeof(thr_pkt), &thr_pkt.segment_index); //&timeout);

            ASSERT(n != ERROR);

            //fprintf(stderr,"Received response: hit/miss: %d, file size = %d, segment_size = %d, chunk_size = %d\n", thr_pkt.cache_hit, thr_pkt.file_size, thr_pkt.segment_size, thr_pkt.chunk_size);
            gfs_sendheader(ctx, GF_OK, thr_pkt.file_size);
            state = SM_GET_DATA;
        }
        else if(state == SM_GET_DATA){
            n = mq_receive(seg->ctrl_mq_rx, (void*)&thr_pkt, sizeof(thr_pkt), &thr_pkt.segment_index); //&timeout);
            
            //fprintf(stderr,"Rx id: %x, c: %d, fs: %d\n", (int)pthread_self(),total_bytes_rx, thr_pkt.file_size );


            ASSERT(n != -1);
            ASSERT(n == sizeof(thr_pkt));
    
            

            gfs_bytes_sent = gfs_send(ctx, seg->segment_ptr, thr_pkt.chunk_size);
            if (gfs_bytes_sent != thr_pkt.chunk_size){
                perror("gfs_bytes_sent != bytes_received");
                return EXIT_FAILURE;
            }
            
            total_bytes_rx += thr_pkt.chunk_size;
            
            ASSERT(total_bytes_rx <= thr_pkt.file_size);
            if(total_bytes_rx == thr_pkt.file_size){
                fprintf(stderr,"Transfer Complete!\n");
                fprintf(stderr,"----------------------\n");
            }
            
            //Send ack
            send_message(seg->ctrl_mq_tx, (void*)&thr_pkt, sizeof(thr_pkt), thr_pkt.segment_index);
        }
    }
    while(total_bytes_rx < thr_pkt.file_size);


    pthread_mutex_lock(&seg_mutex);
    steque_push(segment_q, seg);
    
    pthread_cond_signal(&seg_cond);
    pthread_mutex_unlock(&seg_mutex);
    
    return 0;  //TODO zero or bytes_rx?
}


//memset(seg->segment_ptr, 0, thr_pkt.segment_size);
//       fprintf(stderr, "Reading Segment: %s \n", (char*)seg->segment_ptr);


/*
 while(1){
 seg = (segment_item*)steque_front(segment_q);
 steque_cycle(segment_q);
 if(seg->segment_index == 9)
 break;
 }*/

/*
 while(1){
 seg = (segment_item*)steque_front(segment_q);
 steque_cycle(segment_q);
 if(seg->segment_index == 9)
 break;
 }
 */
//sprintf(seg->mem, "%s", "Hi to cache (from proxy)");



//fprintf(stderr,"File received, bytes = %d, file_size = %lu\n", total_bytes_received, file_size);

/*
 while(steque_isempty(&request_queue))
 pthread_cond_wait(&req_cond, &req_mutex);
 
 steque_node_t* node = steque_pop(&request_queue);
 request_t* req = (request_t*) node->item;
 pthread_mutex_unlock(&req_mutex);
 */
//struct timespec timeout;

//fprintf(stderr, "Segment id = %s\n", seg->segment_id);




//fprintf(stderr,"Maximum # of messages on queue: %ld\n", attr.mq_maxmsg);
//fprintf(stderr,"Maximum message size: %ld\n", attr.mq_msgsize);
//fprintf(stderr,"# of messages currently on queue: %ld\n", attr.mq_curmsgs);