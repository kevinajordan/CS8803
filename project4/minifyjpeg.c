#include "minifyjpeg.h"
#include "magickminify.h"
#include <stdio.h>

/* Implement the needed server-side functions here */

/* Process request */
bool_t minify_image_1_svc(minify_img_arg arg, dest_buffer* res, struct svc_req *cl){
    
    magickminify_init();
    
    res->dest_buf.dest_buf_val = magickminify(arg.src_buf.src_buf_val, arg.src_buf.src_buf_len, (ssize_t*)&(res->dest_buf.dest_buf_len));

    return 1;
}


/* RPC clean up function */
int image_prog_1_freeresult (SVCXPRT * prt, xdrproc_t proc, caddr_t addr){
    magickminify_cleanup();

    xdr_free(proc, addr);
    return 1;
}
