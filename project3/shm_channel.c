#include "shm_channel.h"
#include <sys/mman.h>

char* shm_path = "/segment";


mqd_t create_message_queue(char* _name, int _flags, int _msg_sz, int _max_msgs){
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_curmsgs = 0;
    attr.mq_msgsize = _msg_sz;
    attr.mq_maxmsg = 10;
    
    // Open a queue with the attribute structure
    mqd_t mqd = mq_open (_name, _flags, 0644, &attr);
    
    if(mqd == (mqd_t)-1){
        //fprintf(stderr,"ERROR: mq_open()\n");
        exit(EXIT_FAILURE);
    }

    
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


void send_message(mqd_t _mqd, char* _msg, int _msg_len, int priority){
    
    int status = mq_send(_mqd, _msg, _msg_len, priority);
    //ASSERT(status == 0);
    if(status < 0){
        //fprintf(stderr,"ERROR: mq_send()\n");
        exit(EXIT_FAILURE);
    }
    
    ////fprintf(stderr,"Message sent..\nPath: -%s-, Length: %d\n", _msg, _msg_len);
    
}

void send_file(mqd_t _mqd, int _fd){
    char buffer[MSG_SIZE];
    memset(buffer, 0, sizeof(buffer));
    int bytes_read = 0, total_bytes_read = 0;
    unsigned file_size = lseek(_fd, 0, SEEK_END);
    ASSERT(file_size > 0);
    
    lseek(_fd, 0, SEEK_SET);
    //fprintf(stderr,"File size = %d\n", file_size);
    //send_message(_mqd, "boy", 3);
    
    //fprintf(stderr,"Sending cached file...\n");
    while (total_bytes_read < file_size){
        int offset = file_size - total_bytes_read;
        int request_size = (offset < MSG_SIZE) ? offset : MSG_SIZE;
        
        //fprintf(stderr,"request_size: %d\n", request_size);
        bytes_read = read(_fd, &buffer[0], request_size);
        //fprintf(stderr,"bytes_read: %d\n", bytes_read);

        if(bytes_read < 0){
            //fprintf(stderr,"ERROR: read()\n");
            exit(EXIT_FAILURE);
        }
        
        total_bytes_read += bytes_read;
        send_message(_mqd, &buffer[0], bytes_read, 0);
        //fprintf(stderr,".");
    }
    //fprintf(stderr,"----> File transfer completed\n");
    
   /* if (error(_fd)) {
        //fprintf(stderr,"ERROR: send_file()\n");
        exit(EXIT_FAILURE);
    }
    */
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


void shm_init_segments(steque_t* _segment_queue, int _num_segments, int _segment_size){
    steque_t* segment_ids = malloc(sizeof(steque_t));
    steque_t* segment_mems = malloc(sizeof(steque_t));

    shm_init_ids(segment_ids, _num_segments);
    shm_map_segments(segment_mems, segment_ids, _segment_size);
    
    shm_create_segment_info(_segment_queue, segment_ids, segment_mems);
    
    free(segment_ids);
    free(segment_mems);
}

void shm_init_segments2(steque_t* _segment_queue, int _num_segments, int _segment_size){
    steque_t* segment_ids = malloc(sizeof(steque_t));
    steque_t* segment_mems = malloc(sizeof(steque_t));
    
    shm_init_ids(segment_ids, _num_segments);
    shm_map_segments2(segment_mems, segment_ids, _segment_size);
    
    shm_create_segment_info2(_segment_queue, segment_ids, segment_mems);
    
    free(segment_ids);
    free(segment_mems);
}


void shm_init_ids(steque_t* _segment_ids, int _num_segments){
    steque_init(_segment_ids);
    
    for (int i = 0; i < _num_segments; i++) {
        char index[MAX_SEG];
        sprintf(index, "%d", i);
        
        char* segment_id = malloc(strlen(shm_path) +strlen(index) + 1);
        strcpy(segment_id, shm_path);
        strcat(segment_id, index);
        
        steque_push(_segment_ids, segment_id);

        shm_unlink(segment_id);
    }
}



void shm_map_segments(steque_t* _segment_mems, steque_t* _segment_ids, int _segment_size){
    int num_segments = steque_size(_segment_ids);
    fprintf(stderr, "segment size = %d\n", _segment_size);

    for (int i = 0; i < num_segments; i++) {
        
        char* segment_id = (char*) steque_front(_segment_ids);
        steque_cycle(_segment_ids);
        fprintf(stderr, "opening fd: %s\n", segment_id);
        int segment_fd = shm_open(segment_id, O_RDWR | O_CREAT, 0666);
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
        //close(segment_fd);
        
        steque_push(_segment_mems, segment_mem);
    }
}


void shm_map_segments2(steque_t* _segment_mems, steque_t* _segment_ids, int _segment_size){
    int num_segments = steque_size(_segment_ids);
    fprintf(stderr, "segment size = %d\n", _segment_size);
    
    for (int i = 0; i < num_segments; i++) {
        
        char* segment_id = (char*) steque_front(_segment_ids);
        steque_cycle(_segment_ids);
        fprintf(stderr, "opening fd: %s\n", segment_id);
        int segment_fd = shm_open(segment_id, O_RDWR | O_CREAT, 0666);
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
        //close(segment_fd);
        
        steque_push(_segment_mems, segment_mem);
    }
}


void shm_create_segment_info(steque_t* _segment_queue, steque_t* segment_ids, steque_t* _segment_mems)
{
    for (int i = 0; i < steque_size(segment_ids); i++) {
        char* mem = (char*)steque_pop(_segment_mems);
        char* segment_id = (char*)steque_pop(segment_ids);
        segment_item* shm_info_item = malloc(sizeof(segment_item));
        shm_info_item->mem = mem;
        
        char* ptr = (char*) mem;
        shm_info_item->segment_id = segment_id;
        steque_push(_segment_queue, shm_info_item);
    }
}


void shm_create_segment_info2(steque_t* _segment_queue, steque_t* segment_ids, steque_t* _segment_mems)
{
    for (int i = 0; i < steque_size(segment_ids); i++) {
        char* mem = (char*)steque_pop(_segment_mems);
        char* segment_id = (char*)steque_pop(segment_ids);
        segment_item* shm_info_item = malloc(sizeof(segment_item));
        shm_info_item->mem = mem;
        
        char* ptr = (char*) mem;

        sprintf(ptr, "%s\n", "Hello W!");
    

        shm_info_item->segment_id = segment_id;
        steque_push(_segment_queue, shm_info_item);
    }
}


















