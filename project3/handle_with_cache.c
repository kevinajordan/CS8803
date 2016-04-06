
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
#define CONT 0
#define END  1

extern mqd_t ctrl_mq_tx;
extern mqd_t ctrl_mq_rx;
extern pthread_mutex_t  seg_mutex;
extern pthread_cond_t   seg_cond;
extern steque_t         seg_queue;

/* Get the next available segment, function is thread-safe */
segment_item* acquire_segment(steque_t* _segment_q){
    pthread_mutex_lock(&seg_mutex);
    
    while(steque_isempty(_segment_q))
        pthread_cond_wait(&seg_cond, &seg_mutex);
    
    segment_item* seg = steque_pop(_segment_q);
    pthread_mutex_unlock(&seg_mutex);
    

    return seg;
}

/* Release segment by pushing it back to queue */
void release_segment(steque_t* _segment_q, segment_item* _seg){
    pthread_mutex_lock(&seg_mutex);
    steque_push(_segment_q, _seg);
    
    pthread_cond_signal(&seg_cond);
    pthread_mutex_unlock(&seg_mutex);
}


int wait_for_filesize(gfcontext_t* _ctx, thread_packet* _thr_pkt, segment_item* _seg, enum sm *_state, int *ret_value){
    int ret = CONT;
    //clock_gettime(CLOCK_REALTIME, &timeout);
    //timeout.tv_sec += 3;
    rx_mq(_seg->mq_data_rx, (void*)_thr_pkt, sizeof(thread_packet));
    
    if(_thr_pkt->cache_hit){
        gfs_sendheader(_ctx, GF_OK, _thr_pkt->file_size);
        *_state = SM_GET_DATA;
    }
    else{
        *ret_value = gfs_sendheader(_ctx, GF_FILE_NOT_FOUND, 0);
        ret = END;
    }
    
    return ret;
}



int handle_request(gfcontext_t* _ctx, segment_item* _seg, char* _path){
    thread_packet thr_pkt;
    memcpy(thr_pkt.requested_file, _path, MAX_REQUEST_LEN);
    int ret_value, total_bytes_rx = 0;
    enum sm state = SM_GET_FILESIZE;
    thr_pkt.segment_size = 0, thr_pkt.chunk_size = 0;
    thr_pkt.segment_index = _seg->segment_index;

    /* Send request, inform cache of the segment to use */
    tx_mq(ctrl_mq_tx, (void*)&thr_pkt, sizeof(thr_pkt));
    
    
    do{
        if(state == SM_GET_FILESIZE){
            int flow = wait_for_filesize(_ctx, &thr_pkt, _seg, &state, &ret_value);
            if(flow == END) break;
        }
        else if(state == SM_GET_DATA){
            rx_mq(_seg->mq_data_rx, (void*)&thr_pkt, sizeof(thr_pkt));
            
            int gfs_nbytes = gfs_send(_ctx, _seg->segment_ptr, thr_pkt.chunk_size);
            ASSERT(gfs_nbytes == thr_pkt.chunk_size);
            
            total_bytes_rx += thr_pkt.chunk_size;
            ret_value = total_bytes_rx;
            //Send ack
            tx_mq(_seg->mq_data_tx, (void*)&thr_pkt, sizeof(thr_pkt));
        }
    }
    while(total_bytes_rx < thr_pkt.file_size);

    return ret_value;
}




ssize_t handle_with_cache(gfcontext_t* _ctx, char* _path, void* _arg){
    steque_t* segment_q = (steque_t*) _arg;
    int nbytes;

    /* Acquire shared-memory segment */
    segment_item* seg = acquire_segment(segment_q);
    
    /* Process incoming server request */
    nbytes = handle_request(_ctx, seg, _path);
    
    /* Release shared-memory segment */
    release_segment(segment_q, seg);

    return nbytes;
}


