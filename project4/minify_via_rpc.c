#include <stdlib.h>
#include <stdio.h>
#include "minifyjpeg_xdr.c"
#include "minifyjpeg_clnt.c"

CLIENT* get_minify_client(char* _server){
    CLIENT *cl;
    
    cl = clnt_create(_server, IMAGE_PROG, IMAGE_VERS, "tcp");
 
    /* check if connection was established */
    if(cl == NULL){
        clnt_pcreateerror(_server);
        exit(2);
    }
    
    return cl;
}


void* minify_via_rpc(CLIENT *cl, void* src_val, size_t src_len, size_t *dst_len){
    //enum clnt_stat result;
    
    //struct minify_img_arg arg;
    //arg.src_buf.src_buf_len = src_len;
    //arg.src_buf.src_buf_val = src_val;
    //struct dest_buffer dst_struct;
    //dst_struct.dest_buf.dest_buf_val = malloc(MAX_SIZE * sizeof(char));
    
    //minify_img_arg arg;
    //arg.src_buf.src_buf_val = malloc(src_len * sizeof(char));
    //arg.src_buf.src_buf_len = 6;
    //strcpy(arg.src_buf.src_buf_val, "culero");
    
    minify_img_arg arg;
    arg.src_buf.src_buf_val = src_val;
    arg.src_buf.src_buf_len = src_len;

    struct dest_buffer* dest = malloc(sizeof(struct dest_buffer));
    //dest->dest_buf.dest_buf_val; // = malloc(90000000);

    int bool_res;
    
    if ((bool_res = minify_image_1(arg, dest, cl)) != RPC_SUCCESS) {
        printf("ERROR in client\n");
        clnt_perror(cl, NULL);
        exit(3);
    }
    
    
    //if ((dest = minify_image_1(arg, cl)) == NULL) {
    //    clnt_perror(cl, NULL);
    //    exit(3);
    //}
    
    printf("dest_len:  %d \n", dest->dest_buf.dest_buf_len);
    
    
    *dst_len = dest->dest_buf.dest_buf_len;

    //printf("result = %d\n", dest->result);
    
    //if ((result = minify_image_1(arg, &dst_struct, cl)) != RPC_SUCCESS) {
    //    clnt_perror(cl, NULL);
    //    exit(3);
    //}
    
    return dest->dest_buf.dest_buf_val;
}