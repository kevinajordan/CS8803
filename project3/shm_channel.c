#include "shm_channel.h"
#include <sys/mman.h>

char* shm_path = "/segment";


mqd_t create_message_queue(char* _name, int _flags, int _msg_sz, int _max_msgs){
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_curmsgs = 0;
    //fprintf(stderr, "queue msg size = %d\n", _msg_sz);
    attr.mq_msgsize = _msg_sz;
    attr.mq_maxmsg = 9;
    
    // Open a queue with the attribute structure
    mqd_t mqd = mq_open (_name, _flags, 0644, &attr);
    
    if(mqd == (mqd_t)-1){
        //fprintf(stderr,"ERROR: mq_open()\n");
        exit(EXIT_FAILURE);
    }

    
    //struct mq_attr attr;

    //if(mq_getattr (_mqd, &attr)){
    ////fprintf(stderr,"Maximum # of messages on queue: %ld\n", attr.mq_maxmsg);
    ////fprintf(stderr,"Maximum message size: %ld\n", attr.mq_msgsize);
    ////fprintf(stderr,"# of messages currently on queue: %ld\n", attr.mq_curmsgs);
    
    return mqd;
}


struct mq_attr get_queue_attr(mqd_t _mqd){
    struct mq_attr attr;
    if(mq_getattr (_mqd, &attr)){
        //fprintf(stderr,"ERROR: mq_getattr()\n");
        exit(EXIT_FAILURE);
    }
    
    return attr;
}


void send_message(mqd_t _mqd, void* _msg, int _msg_len, int priority){
    
    int status = mq_send(_mqd, _msg, _msg_len, priority);
    //ASSERT(status == 0);
    if(status < 0){
        //fprintf(stderr,"ERROR: mq_send()\n");
        exit(EXIT_FAILURE);
    }
    
    ////fprintf(stderr,"Message sent..\nPath: -%s-, Length: %d\n", _msg, _msg_len);
    
}




void receive_message(mqd_t _mqd, char* _buffer){
    //fprintf(stderr,"Waiting to rx message...\n");
    int bytes_received = mq_receive(_mqd, _buffer, MSG_SIZE, 0);
    //ASSERT(status == 0);
    
    if(bytes_received < 0){
        //fprintf(stderr,"ERROR: mq_receive()\n");
        exit(EXIT_FAILURE);
    }
    
    
    //fprintf(stderr,"Message received..\nPath -%s-, Length: %d\n", _buffer, bytes_received);   
}



/*----------------- Segments --------------------*/


void shm_create_segments(steque_t* _segment_queue, int _num_segments, int _segment_size){
    steque_init(_segment_queue);
    
    for (int i = 0; i < _num_segments; i++) {
        char* segment_id = shm_init_id(i);
        void* segment_mem = shm_map_segment(segment_id, _segment_size);

        segment_item* shm_info_item = malloc(sizeof(segment_item));
        shm_info_item->segment_ptr = segment_mem;
        shm_info_item->segment_id = segment_id;
        shm_info_item->segment_index = i;
        steque_push(_segment_queue, shm_info_item);
    }
}


char* shm_init_id(int _index){
    char index_str[MAX_SEG];
    sprintf(index_str, "%d", _index);
    char* segment_id = malloc(strlen(shm_path) +strlen(index_str) + 1);
    strcpy(segment_id, shm_path);
    strcat(segment_id, index_str);
    
    return segment_id;
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














