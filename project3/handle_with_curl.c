#include <stdlib.h>
#include <fcntl.h>
#include <curl/curl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "gfserver.h"

//Replace with an implementation of handle_with_curl and any other
//functions you may need.

/*
ssize_t handle_with_file(gfcontext_t *ctx, char *path, void* arg){
	int fildes;
	size_t file_len, bytes_transferred;
	ssize_t read_len, write_len;
	char buffer[4096];
	char *data_dir = arg;

	strcpy(buffer,data_dir);
	strcat(buffer,path);

	if( 0 > (fildes = open(buffer, O_RDONLY))){
		if (errno == ENOENT)
			return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
		else
			return EXIT_FAILURE;
	}

	file_len = lseek(fildes, 0, SEEK_END);
	lseek(fildes, 0, SEEK_SET);

	gfs_sendheader(ctx, GF_OK, file_len);

	bytes_transferred = 0;
	while(bytes_transferred < file_len){
		read_len = read(fildes, buffer, 4096);
		if (read_len <= 0){
			fprintf(stderr, "handle_with_file read error, %zd, %zu, %zu", read_len, bytes_transferred, file_len );
			return EXIT_FAILURE;
		}
		write_len = gfs_send(ctx, buffer, read_len);
		if (write_len != read_len){
			fprintf(stderr, "handle_with_file write error");
			return EXIT_FAILURE;
		}
		bytes_transferred += write_len;
	}

	return bytes_transferred;
}
*/


struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
    
    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if(mem->memory == NULL) {
        /* out of memory! */
        fprintf(stderr, "not enough memory (realloc returned NULL)\n");
        return 0;
    }
    
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    
    return realsize;
}

ssize_t handle_with_file(gfcontext_t *ctx, char *path, void* arg)
{
    CURL *curl_handle;
    CURLcode res;
    long http_resp_code = 0;
    
    struct MemoryStruct chunk;
    double filesize;
    
    char* server = (char*) arg;
    //fprintf(stderr, "Server: %s\n", server);

    
    int server_len = strlen(server);
    int url_len = strlen(path);
    
    char* url = malloc(server_len + url_len + 1);
    memcpy(url, server, server_len);
    memcpy(url + server_len, path, strlen(path) + 1);
    url[server_len + url_len] = '\0';
    
    
    
    //fprintf(stderr, "URL: %s\n", url);
    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */
    
    curl_global_init(CURL_GLOBAL_ALL);
    
    /* init the curl session */
    curl_handle = curl_easy_init();
    
    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    //curl_easy_setopt(curl_handle, CURLOPT_URL, "http://s3.amazonaws.com/content.udacity-data.com/courses/ud923/filecorpus/1kb-sample-file-1.html");

    curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 1L);
    
    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    
    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    
    /* some servers don't like requests that are made without a user-agent
     field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    

    
    res = curl_easy_perform(curl_handle);
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD,
                            &filesize);
    

    if(res != CURLE_OK) {
        fprintf(stderr, "1) curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
        return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
    }
    else {
        
        curl_easy_getinfo (curl_handle, CURLINFO_RESPONSE_CODE, &http_resp_code);
        if(http_resp_code == 404 || http_resp_code == 403){
            fprintf(stderr, "HEREEEEE\n");
            return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, http_resp_code);
        }
        else if(http_resp_code != 200){
            fprintf(stderr, "ERROR CODE: %lu\n", http_resp_code);
            return EXIT_FAILURE;
        }
        
        
        //printf("Filesize: %0.0f \n", filesize);
        gfs_sendheader(ctx, GF_OK, filesize);
    }
    
    
    
    
    
    
    
    
    
    
    curl_easy_setopt(curl_handle, CURLOPT_NOBODY, 0);
    
    int bytes_transferred = 0;
    while(bytes_transferred < filesize){
        res = curl_easy_perform(curl_handle);
        if(res != CURLE_OK) {
            fprintf(stderr, "2) curl_easy_perform() failed: %s\n",
                    curl_easy_strerror(res));
            return EXIT_FAILURE;
        }
        
        //TODO: This section not needed in Udacity check, why?
        curl_easy_getinfo (curl_handle, CURLINFO_RESPONSE_CODE, &http_resp_code);
        if(http_resp_code == 404 || http_resp_code == 403){
            fprintf(stderr, "HEREEEEE\n");
            return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, http_resp_code);
        }
        else if(http_resp_code != 200){
            fprintf(stderr, "ERROR CODE: %lu\n", http_resp_code);
            return EXIT_FAILURE;
        }

        
        int write_len = gfs_send(ctx, chunk.memory, chunk.size);
        //fprintf(stderr, "%s", chunk.memory);
        
        //fprintf(stderr, "Sent to gfserver: %d\n", write_len);
        if (write_len != chunk.size){
            fprintf(stderr, "handle_with_file write error");
            return EXIT_FAILURE;
        }
        bytes_transferred += write_len;
    }
    
    
    
    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);
    
    free(chunk.memory);
    
    /* we're done with libcurl, so clean it up */
    curl_global_cleanup();
    
    return bytes_transferred;
}
