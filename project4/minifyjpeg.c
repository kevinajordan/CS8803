#include "minifyjpeg.h"
#include "magickminify.h"
#include <stdio.h>

/* Implement the needed server-side functions here */


//bool_t minify_image_1_svc(minify_img_arg arg, dest_buffer * dst, struct svc_req *cl){
//dest_buffer * minify_image_1_svc(minify_img_arg arg, struct svc_req *cl){
bool_t minify_image_1_svc(minify_img_arg arg, dest_buffer* res, struct svc_req *cl){
    
    magickminify_init();

    //printf("Server:  %.*s \n", arg.src_buf.src_buf_len, arg.src_buf.src_buf_val);

    //res->dest_buf.dest_buf_len = 7;
    //res->dest_buf.dest_buf_val = malloc(res->dest_buf.dest_buf_len * sizeof(char));
    //strcpy(res->dest_buf.dest_buf_val, "perfect");
    
    //void* new_img = magickminify(arg.src_buf.src_buf_val, arg.src_buf.src_buf_len, (ssize_t*)&(res->dest_buf.dest_buf_len));
    
    res->dest_buf.dest_buf_val = magickminify(arg.src_buf.src_buf_val, arg.src_buf.src_buf_len, (ssize_t*)&(res->dest_buf.dest_buf_len));
    
    //memcpy((void*)res->dest_buf.dest_buf_val, new_img, res->dest_buf.dest_buf_len);

    magickminify_cleanup();
    
    return 1;
    //memset(dst->dest_buf.dest_buf_val, 1, 32);
}


int image_prog_1_freeresult (SVCXPRT * prt, xdrproc_t proc, caddr_t addr){
    //TODOOOO
    //xdr_free(proc, addr);
    return 1;
}
