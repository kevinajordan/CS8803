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
    enum clnt_stat result;
    
    minify_img_arg arg;
    arg.src_buf.src_buf_val = src_val;
    arg.src_buf.src_buf_len = src_len;

    /* Allocate memory for image */
    struct dest_buffer dest;
    dest.dest_buf.dest_buf_val = malloc(MAX_JPEG_SIZE);
    
    /* RPC Call */
    if ((result = minify_image_1(arg, &dest, cl)) != RPC_SUCCESS) {
        printf("ERROR in client\n");
        clnt_perror(cl, NULL);
        exit(3);
    }
    
    /* set destination pointer */
    *dst_len = dest.dest_buf.dest_buf_len;

    return dest.dest_buf.dest_buf_val;
}