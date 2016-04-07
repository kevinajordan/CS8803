#include "shm_channel.h"
#include <sys/mman.h>

char* shm_path = "/segment_sebdel";


/*----------------------- MQ  Helper Functions --------------------*/

mqd_t create_message_queue(char* _name, int _flags, int _msg_sz, int _max_msgs){
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_curmsgs = 0;
    attr.mq_msgsize = _msg_sz;
    attr.mq_maxmsg = 10;
    errno = 0;
    
    // Open a queue with the attribute structure
    mqd_t mqd = mq_open (_name, _flags, 0666, &attr);
    ASSERT(mqd != ((mqd_t)-1));

    return mqd;
}



void tx_mq(mqd_t _mqd, void* _msg, int _msg_len){
    errno = 0;
    int status = mq_send(_mqd, _msg, _msg_len, 0);
    ASSERT(status == 0);
}

void rx_mq(mqd_t _mqd, void* _msg, int _msg_len){
    errno = 0;
    int bytes_received = mq_receive(_mqd, _msg, _msg_len, 0);
    ASSERT(bytes_received >= 0);
}


struct mq_attr get_queue_attr(mqd_t _mqd){
    struct mq_attr attr;
    if(mq_getattr (_mqd, &attr)){
        exit(EXIT_FAILURE);
    }
    return attr;
}



/*----------------- Segments Helper Functions --------------------*/


void shm_create_segments(steque_t* _segment_queue, int _num_segments, int _segment_size, int _proxy){
    steque_init(_segment_queue);
    char* tx_prefix;
    char* rx_prefix;
    
    for (int i = 0; i < _num_segments; i++) {
        segment_item* shm_info_item = malloc(sizeof(segment_item));

        char* segment_id = shm_create_id(shm_path, i);
        void* segment_mem = shm_map_segment(segment_id, _segment_size);

        
        if(_proxy){
            tx_prefix = "/proxy-to-cache";
            rx_prefix = "/cache-to-proxy";
        }
        else{
            rx_prefix = "/proxy-to-cache";
            tx_prefix = "/cache-to-proxy";
        }
        
        char* mq_tx_str = shm_create_id(tx_prefix, i);
        char* mq_rx_str = shm_create_id(rx_prefix, i);
        dbg("mq: %s\n", mq_tx_str);

        shm_info_item->mq_data_tx_str = mq_tx_str;
        shm_info_item->mq_data_rx_str = mq_rx_str;
        shm_info_item->mq_data_tx = create_message_queue(mq_tx_str, O_CREAT | O_RDWR,  sizeof(thread_packet), MAX_MSGS);
        shm_info_item->mq_data_rx = create_message_queue(mq_rx_str, O_CREAT | O_RDWR,  sizeof(thread_packet), MAX_MSGS);
        shm_info_item->segment_ptr = segment_mem;
        shm_info_item->segment_id = segment_id;
        shm_info_item->segment_index = i;
        steque_push(_segment_queue, shm_info_item);
    }
}


char* shm_create_id(char* _prefix, int _index){
    char index_str[MAX_SEG];
    sprintf(index_str, "%d", _index);
    char* id = malloc(strlen(shm_path) +strlen(index_str) + 1);
    strcpy(id, _prefix);
    strcat(id, index_str);
    
    return id;
}



void* shm_map_segment(char* _segment_id, int _segment_size){
    //fprintf(stderr, "opening fd: %s\n", _segment_id);
    int segment_fd = shm_open(_segment_id, O_RDWR | O_CREAT, 0666);
    
    if(segment_fd < 0) {
        perror("shm_open()");
        exit(EXIT_FAILURE);
    }
    
    ftruncate(segment_fd, _segment_size);
    void* segment_mem = mmap(NULL, _segment_size, PROT_WRITE |PROT_READ, MAP_SHARED, segment_fd, 0);
    
    if(segment_mem == MAP_FAILED) {
        perror("mmap()");
        exit(EXIT_FAILURE);
    }
    
    close(segment_fd);
    return segment_mem;
}



void  shm_clean_segments(steque_t* _segment_queue){
    char* tx_prefix;
    char* rx_prefix;
    tx_prefix = "/proxy-to-cache";
    rx_prefix = "/cache-to-proxy";
    
    for (int i = 0; i < steque_size(_segment_queue); i++) {
        char* mq_tx_str = shm_create_id(tx_prefix, i);
        char* mq_rx_str = shm_create_id(rx_prefix, i);
        shm_unlink(mq_tx_str);
        shm_unlink(mq_rx_str);
    }
    
    for(int i = 0; i < steque_size(_segment_queue); i++){
        segment_item* shm_info_item = steque_front(_segment_queue);
        steque_cycle(_segment_queue);
        mq_unlink(shm_info_item->mq_data_tx_str);
        mq_unlink(shm_info_item->mq_data_rx_str);
        mq_close(shm_info_item->mq_data_tx);
        mq_close(shm_info_item->mq_data_rx);
        //Free strings
        free(shm_info_item->mq_data_tx_str);
        free(shm_info_item->mq_data_rx_str);
    }


}