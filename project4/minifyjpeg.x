/*
 * Complete this file and run rpcgen -MN minifyjpeg.x
 */

const MAX_SIZE = 4096;

struct minify_img_arg {
    opaque src_buf<>;
};

struct dest_buffer {
    opaque dest_buf<>;
};

program IMAGE_PROG {
    version IMAGE_VERS {
        dest_buffer minify_image(minify_img_arg) = 1;
    } = 1;
} = 0x33009900;