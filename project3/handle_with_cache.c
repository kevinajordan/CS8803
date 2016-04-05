
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

extern mqd_t ctrl_mq_tx;
extern mqd_t ctrl_mq_rx;
extern pthread_mutex_t  seg_mutex;
extern pthread_cond_t   seg_cond;
extern steque_t         seg_queue;

   
ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg)
{
  
    thread_packet thr_pkt;
    memcpy(thr_pkt.requested_file, path, strlen(path));
    thr_pkt.segment_size = 0, thr_pkt.chunk_size = 0;
    steque_t* segment_q = (steque_t*) arg;
    int gfs_bytes_sent;
    //num_segments = steque_size(segment_q);

    
    int n, total_bytes_rx = 0;
    enum sm state = SM_GET_FILESIZE;
    
    
    /* Get the next available segment, thread-safe */
    pthread_mutex_lock(&seg_mutex);

    while(steque_isempty(segment_q))
        pthread_cond_wait(&seg_cond, &seg_mutex);

    segment_item* seg = steque_pop(segment_q);
    pthread_mutex_unlock(&seg_mutex);
    
    
    dbg("Thread processing.. thr_id=%x path: %s\n", (int)pthread_self(), thr_pkt.requested_file);
    

    
    //Send request, inform cache of the segment to use
    thr_pkt.segment_index = seg->segment_index;

    send_message(ctrl_mq_tx, (void*)&thr_pkt, sizeof(thr_pkt), 0);
    dbg("Sending tx request.. seg idx=%d, thr_id=%x path: %s\n", thr_pkt.segment_index, (int)pthread_self(), thr_pkt.requested_file);

    
    do{
        if(state == SM_GET_FILESIZE){
            //clock_gettime(CLOCK_REALTIME, &timeout);
            //timeout.tv_sec += 3;
            //dbg( "Wainting for response (cache hit/miss)...\n");
            n = mq_receive(seg->mq_data_rx, (void*)&thr_pkt, sizeof(thr_pkt), 0); //&timeout);

            ASSERT(n != ERROR);

            dbg("Received response: hit/miss: %d, file size = %d, seg: %d\n", thr_pkt.cache_hit, thr_pkt.file_size, thr_pkt.segment_index);
            
            if(thr_pkt.cache_hit){
                gfs_sendheader(ctx, GF_OK, thr_pkt.file_size);
                state = SM_GET_DATA;
            }
            else{
                pthread_mutex_lock(&seg_mutex);
                steque_push(segment_q, seg);
                
                pthread_cond_signal(&seg_cond);
                pthread_mutex_unlock(&seg_mutex);
                return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
            }
        }
        else if(state == SM_GET_DATA){
            n = mq_receive(seg->mq_data_rx, (void*)&thr_pkt, sizeof(thr_pkt), 0); //&timeout);
            
            dbg("seg:%d, mq_rx=%d, thread id: %x, T: %d, c=%d, fs: %d\n", seg->segment_index, seg->mq_data_rx, (int)pthread_self(), total_bytes_rx, thr_pkt.chunk_size, thr_pkt.file_size);


            ASSERT(n != -1);
            ASSERT(n == sizeof(thr_pkt));
    
            

            gfs_bytes_sent = gfs_send(ctx, seg->segment_ptr, thr_pkt.chunk_size);
            if (gfs_bytes_sent != thr_pkt.chunk_size){
                dbg("seg=%d, gfs_bytes_sent != bytes_received\n", thr_pkt.segment_index);
                return EXIT_FAILURE;
            }
            
            fprintf(stderr, "seg:%d, thr %x, ctx->file_len: %zu\n", seg->segment_index, (int)pthread_self(), ctx->file_len);
            fprintf(stderr, "seg:%d, thread id: %xctx->bytes_transferred: %zu\n", seg->segment_index,(int)pthread_self(), ctx->bytes_transferred);
            
            total_bytes_rx += thr_pkt.chunk_size;
            
            ASSERT(total_bytes_rx <= thr_pkt.file_size);
            if(total_bytes_rx == thr_pkt.file_size){
                dbg("seg=%d, mq_tx=%d, Transfer Complete...\n", seg->segment_index, seg->mq_data_tx);
            }
            
            //Send ack
            send_message(seg->mq_data_tx, (void*)&thr_pkt, sizeof(thr_pkt), 0);
        }
    }
    while(total_bytes_rx < thr_pkt.file_size);

    
    fprintf(stderr, "%s%zu\n", "ctx->file_len: ", ctx->file_len);
    fprintf(stderr, "%s%zu\n", "ctx->bytes_transferred: ", ctx->bytes_transferred);
    
    //ASSERT(thr_pkt.file_size == total_bytes_rx);
    pthread_mutex_lock(&seg_mutex);
    steque_push(segment_q, seg);
    
    pthread_cond_signal(&seg_cond);
    pthread_mutex_unlock(&seg_mutex);
    
    return total_bytes_rx;
}