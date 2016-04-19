/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#ifndef _MINIFYJPEG_H_RPCGEN
#define _MINIFYJPEG_H_RPCGEN

#include <rpc/rpc.h>

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SIZE 4096

struct minify_img_arg {
	struct {
		u_int src_buf_len;
		char *src_buf_val;
	} src_buf;
};
typedef struct minify_img_arg minify_img_arg;

struct dest_buffer {
	struct {
		u_int dest_buf_len;
		char *dest_buf_val;
	} dest_buf;
};
typedef struct dest_buffer dest_buffer;

#define IMAGE_PROG 0x33009900
#define IMAGE_VERS 1

#if defined(__STDC__) || defined(__cplusplus)
#define minify_image 1
extern  enum clnt_stat minify_image_1(minify_img_arg , dest_buffer *, CLIENT *);
extern  bool_t minify_image_1_svc(minify_img_arg , dest_buffer *, struct svc_req *);
extern int image_prog_1_freeresult (SVCXPRT *, xdrproc_t, caddr_t);

#else /* K&R C */
#define minify_image 1
extern  enum clnt_stat minify_image_1();
extern  bool_t minify_image_1_svc();
extern int image_prog_1_freeresult ();
#endif /* K&R C */

/* the xdr functions */

#if defined(__STDC__) || defined(__cplusplus)
extern  bool_t xdr_minify_img_arg (XDR *, minify_img_arg*);
extern  bool_t xdr_dest_buffer (XDR *, dest_buffer*);

#else /* K&R C */
extern bool_t xdr_minify_img_arg ();
extern bool_t xdr_dest_buffer ();

#endif /* K&R C */

#ifdef __cplusplus
}
#endif

#endif /* !_MINIFYJPEG_H_RPCGEN */
