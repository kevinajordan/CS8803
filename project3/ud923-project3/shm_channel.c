#include "shm_channel.h"


mqd_t create_message_queue(char* _name, int _flags, int _msg_sz, int _max_msgs){
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_curmsgs = 0;
    attr.mq_msgsize = MSG_SIZE;
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



int receive_message(mqd_t _mqd, char* _buffer){
    //fprintf(stderr,"Waiting to rx message...\n");
    int bytes_received = mq_receive(_mqd, _buffer, MSG_SIZE, 0);
    //ASSERT(status == 0);
    
    if(bytes_received < 0){
        //fprintf(stderr,"ERROR: mq_receive()\n");
        exit(EXIT_FAILURE);
    }
    
    
    //fprintf(stderr,"Message received..\nPath -%s-, Length: %d\n", _buffer, bytes_received);
    return 0;
    
}
