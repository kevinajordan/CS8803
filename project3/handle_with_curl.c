#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gfserver.h"

#define ASSERT(x) \
do { \
if (!(x)) { \
fprintf(stderr, "%s:%d: ", __func__, __LINE__); \
perror(#x); \
exit(-1); \
} \
} while (0) \


typedef struct mem_data_t {
    char *memory;
    size_t size;
}mem_data_t;

static size_t write_callback(void * _data, size_t size, size_t _num_lumps, void* _container){

    size_t page_size = size * _num_lumps;
    mem_data_t* mem = (mem_data_t *)_container;
    
    /* Reallocate memory to accomodate for the webpage data */
    mem->memory = realloc(mem->memory, mem->size + page_size + 1);
    ASSERT(mem->memory != NULL);
    
    memcpy(&(mem->memory[mem->size]), _data, page_size);
    mem->size += page_size;
    
    mem->memory[mem->size] = 0;
    return page_size;
}


CURL* init_curl(mem_data_t* _lump, char* server, char* _path){
    int server_len = strlen(server);
    int url_len = strlen(_path);
    char* url = malloc(server_len + url_len + 1);
    memcpy(url, server, server_len);
    memcpy(url + server_len, _path, url_len + 1);
    url[server_len + url_len] = '\0';
    
    CURL *curl_handle;
    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)_lump);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    
    
    return curl_handle;
}



ssize_t handle_with_file(gfcontext_t* _ctx, char* _path, void* _arg)
{
    CURL* curl_handle;
    CURLcode res;
    long http_resp_code = 0;
    
    mem_data_t lump;
    lump.memory = malloc(1);  //size=1 is temporary; reallocate as needed

    lump.size = 0;
    double filesize;
    
    curl_handle = init_curl(&lump, (char*) _arg, _path);
    res = curl_easy_perform(curl_handle);
    
    /* ------- Check size only, dont transfer data yet -------- */
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD,
                            &filesize);
    
    /* Do the proper file validity
     * Even if CURLE_OK is return, we need to check the HTTP error code
     * If HTTP error codes 403 or 404 than file was not found
     * else if HTTP error code is not 200 (OK), than just send back error to the gfs server
     */
    /* This error checking can be done here as opposed to when doing the actual data transfer, per libcurl documentation
     */
    if(res != CURLE_OK) return gfs_sendheader(_ctx, GF_FILE_NOT_FOUND, 0);
    else{
        curl_easy_getinfo (curl_handle, CURLINFO_RESPONSE_CODE, &http_resp_code);
        if(http_resp_code == 404 || http_resp_code == 403)
            return gfs_sendheader(_ctx, GF_FILE_NOT_FOUND, http_resp_code);
        else if(http_resp_code != 200)
            return EXIT_FAILURE;
    
        gfs_sendheader(_ctx, GF_OK, filesize);
    }

    // Now that we node the size of the file begin file transfer
    
    curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 0);
    
    int bytes_transferred = 0;
    while(bytes_transferred < filesize){
        res = curl_easy_perform(curl_handle);
        if(res != CURLE_OK) {
            fprintf(stderr, "2) curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
            return EXIT_FAILURE;
        }
        
        int write_len = gfs_send(_ctx, lump.memory, lump.size);
        ASSERT(write_len == lump.size);
        bytes_transferred += write_len;
    }
    
    /* cleanup */
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
    free(lump.memory);
    
    return bytes_transferred;
}
