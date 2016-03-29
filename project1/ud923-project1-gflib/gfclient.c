#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>

#include "gfclient.h"
#include <assert.h>

#define MISC         16
#define SCHEME_SZ    7
#define MAX_METHOD   5
#define MAX_SERVER   128
#define MAX_PATH     255
#define BUFSIZE      (MAX_PATH + MAX_METHOD + SCHEME_SZ + MISC)
#define RDBUFSIZE    8192
#define ENDSEQ       4
#define OK           3 // + space
#define ERROR_SZ     5
#define NOT_FOUND_SZ 14

char* status_str[] = { "GF_OK", "GF_FILE_NOT_FOUND", "GF_ERROR", "GF_INVALID"};
typedef enum{
    SM_SCHEME = 0,
    SM_METHOD,
    SM_LENGTH,
    SM_PATH,
    SM_CONTENT,
    SM_ERROR,
    SM_NOT_FOUND,
    SM_DONE
} gfstatemachine_t;

const int response_types = 3;

int     connect_to_server(char* _hostname, int _port);
void    send_request(gfcrequest_t* _gfr);
int     process_response(gfcrequest_t* _gfr);
void    set_timeouts(int);
int     parse_scheme(gfcrequest_t* _gfr);
void    parse_method(gfcrequest_t* _gfr);
void    parse_length(gfcrequest_t* _gfr);
int     parse_path(gfcrequest_t* _gfr);
void    parse_content(gfcrequest_t* _gfr);


void error(const char* _message){
    perror(_message); exit(0);
}

typedef struct gfcrequest_t {
    char*  server;
    char*  path;
    unsigned short port;
    char*  scheme;
    char*  method;
    char*  end_sequence;
    gfstatus_t status;
    unsigned file_length;
    int file_length_valid;
    unsigned rx_bytes;
    unsigned total_rx_bytes;
    unsigned bytes_written;
    void* header_arg;
    void* write_arg;
    int   socket_fd;
    char buffer[RDBUFSIZE];
    int header_pt;
    unsigned req_length;
    int length_start_idx;
    int content_pt;
    gfstatemachine_t sm;   //State Machine
    void (*write_callback)(void*, size_t, void*);
    void (*header_callback)(void*, size_t, void *);
} gfcrequest_t;


/*
 * Helper function used to connect to server
 */
int connect_to_server(char* _hostname, int _port){
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) error("ERROR:  Socket() fail");
    
    struct sockaddr_in server_addr;
    bzero((char *) &server_addr, sizeof(server_addr));
    
    struct hostent* server = gethostbyname(_hostname);
    if (!server)
        fprintf(stderr,"ERROR: no server hostname found\n");
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(_port);
    bcopy((char*)server->h_addr, (char*) &server_addr.sin_addr.s_addr, server->h_length);
    
    
    int success = connect(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if(success < 0) error("ERROR: Connect() fail");
    
    return socket_fd;
}

/*
 * Send request to server, use populated struct gfcrequest_t variables to construct request
 */
void send_request(gfcrequest_t* _gfr){
    char buffer[BUFSIZE];
    bzero(buffer, BUFSIZE);
    sprintf(buffer, "%s %s %s %s", _gfr->scheme, _gfr->method, _gfr->path, _gfr->end_sequence);
    _gfr->req_length = strlen(buffer);
    
    int n = write(_gfr->socket_fd, buffer, _gfr->req_length);
    if(n < 0) fprintf(stderr,"ERROR: client could not send data over socket\n");
}
/*
 * Follow a state machine scheme to parse response by 'stages'
 */
int process_response(gfcrequest_t* _gfr){
    int r = 0;
    if(!_gfr->write_callback) return 0;
    set_timeouts(_gfr->socket_fd);
    
    while(1){
        int request_size = RDBUFSIZE - _gfr->content_pt;
        //Reduce request_size if request exceeds file_length (only applicable if we have parse the file length)
        if(_gfr->sm > SM_PATH && (request_size + _gfr->total_rx_bytes) > _gfr->file_length)
            request_size = _gfr->file_length - _gfr->total_rx_bytes;
        
        int n = read(_gfr->socket_fd, _gfr->buffer + _gfr->content_pt, request_size);
        if (n <= 0) return -1;

        _gfr->rx_bytes += n;
        _gfr->total_rx_bytes += n;
        
        if(_gfr->sm == SM_SCHEME)       r = parse_scheme(_gfr);
        if(_gfr->sm == SM_METHOD)       parse_method(_gfr);
        if(_gfr->sm == SM_LENGTH)       parse_length(_gfr);
        if(_gfr->sm == SM_PATH)         r = parse_path(_gfr);
        if(_gfr->sm == SM_CONTENT)      parse_content(_gfr);
        if(_gfr->sm == SM_DONE)         r = 1;
        if(_gfr->sm == SM_ERROR){       _gfr->status = GF_ERROR; r = 1; }
        if(_gfr->sm == SM_NOT_FOUND){   _gfr->status = GF_FILE_NOT_FOUND; r = 1; }
        
        if(r == 1) return 0;    //if r == 1, request suceeded
        if(r < 0) return r;     //if r < 0, request failed
    }
}


int parse_scheme(gfcrequest_t* _gfr){
    if(_gfr->rx_bytes >= SCHEME_SZ){
        char* tok = strtok (_gfr->buffer, " ");;
        if(!tok || strncmp(_gfr->buffer, "GETFILE", 7))
            return -1;
        else{
            _gfr->header_pt = (int)strlen(tok) + 1;
            _gfr->sm = SM_METHOD;
        }
    }
    return 0;
}

void parse_method(gfcrequest_t* _gfr){
    if(_gfr->rx_bytes - _gfr->header_pt > OK){
        char* tok = strtok (NULL, " ");
        if(tok && !strcmp(tok, "OK")){
            _gfr->header_pt += (int)strlen(tok) + 1;  //covers spaces, points to first digit
            _gfr->sm = SM_LENGTH;
            _gfr->length_start_idx = _gfr->header_pt;
            _gfr->status = GF_OK;
        }
    }
    
    if(_gfr->sm == SM_METHOD){
        int method_bytes = _gfr->rx_bytes - _gfr->header_pt;
        //Check we have received enough bytes
        if(method_bytes > ERROR_SZ){
            if(!strncmp(&_gfr->buffer[_gfr->header_pt], "ERROR", ERROR_SZ)){
                _gfr->sm = SM_ERROR;
            }
        }
    
        if(method_bytes > NOT_FOUND_SZ){
            if(!strncmp(&_gfr->buffer[_gfr->header_pt], "FILE_NOT_FOUND", NOT_FOUND_SZ))
                _gfr->sm = SM_NOT_FOUND;
            else
                _gfr->sm = SM_ERROR;
                
        }
    }
}

void parse_length(gfcrequest_t* _gfr){
    //Increment buffer pointer until we the current character is not a digit
    while((_gfr->rx_bytes >= _gfr->header_pt) && (isdigit(_gfr->buffer[_gfr->header_pt]))){
        _gfr->header_pt++;
    }

    //Client has not received enougth bytes, retry later
    if (_gfr->rx_bytes < _gfr->header_pt)
        return;
    
    //Length field has been processed, go to next STATE
    _gfr->sm = SM_PATH;
}


int parse_path(gfcrequest_t* _gfr){
    int length_size = _gfr->header_pt - _gfr->length_start_idx;
    
    //Length in string format
    char* length_pt = strndup(&_gfr->buffer[_gfr->length_start_idx], length_size);
    _gfr->file_length = atoi(length_pt);  //Convert to numerical value
    _gfr->file_length_valid = 1;
    
    //Ignore spaces
    while(0 == strncmp(&_gfr->buffer[_gfr->header_pt], " ", 1)) _gfr->header_pt++;
    
    //Check we have received enough bytes to check validty of end marker
    if(_gfr->rx_bytes - _gfr->header_pt >= ENDSEQ){
        if(!strncmp(&_gfr->buffer[_gfr->header_pt], "\r\n\r\n", ENDSEQ)){
            _gfr->header_pt += ENDSEQ;
            
            if(_gfr->header_callback)
                _gfr->header_callback(&_gfr->buffer[0], _gfr->header_pt, _gfr->header_arg);
            
            _gfr->sm = SM_CONTENT;
            _gfr->total_rx_bytes -= _gfr->header_pt;  //Ignore header size for total byte count
            _gfr->content_pt = _gfr->header_pt;
        }
        else{
            _gfr->status = GF_ERROR;
            return -1;
        }
    }
    return 0;
}


void parse_content(gfcrequest_t* _gfr){
    if(_gfr->rx_bytes > _gfr->content_pt){
        int packet_length = _gfr->rx_bytes - _gfr->content_pt;
        _gfr->write_callback(&_gfr->buffer[_gfr->content_pt], packet_length, _gfr->write_arg);
        _gfr->bytes_written += packet_length;
        _gfr->content_pt += packet_length;
        
        //Reset pointer if they have reached maximum buffer size
        if(_gfr->content_pt == RDBUFSIZE){
            _gfr->content_pt = 0;
            _gfr->rx_bytes = 0;
        }
        // Check if we are done
        if(_gfr->bytes_written >= _gfr->file_length)
            _gfr->sm = SM_DONE;
    }
}




gfcrequest_t *gfc_create(){
    gfcrequest_t* gfc = malloc(sizeof(gfcrequest_t));
    gfc->rx_bytes = 0;
    gfc->total_rx_bytes = 0;
    gfc->file_length = 0;
    gfc->file_length_valid = 0;
    gfc->status = GF_INVALID;
    gfc->port = 80;
    gfc->path = NULL;
    gfc->write_arg = NULL;
    gfc->header_callback = NULL;
    gfc->write_callback = NULL;
    gfc->server = strdup("localhost");
    gfc->scheme = strdup("GETFILE");
    gfc->method = strdup("GET");
    gfc->end_sequence = strdup("\r\n\r\n");
    gfc->header_pt = 0;
    gfc->length_start_idx = 0;
    gfc->sm = SM_SCHEME;
    gfc->content_pt = 0;
    gfc->bytes_written = 0;
    memset(gfc->buffer,0, RDBUFSIZE);
    return gfc;
}

void gfc_set_server(gfcrequest_t *gfr, char* server){
    if (gfr->server) free(gfr->server);
    gfr->server = server;
}

void gfc_set_path(gfcrequest_t *gfr, char* path){
    if (gfr->path) free(gfr->path);
    gfr->path = path;
}

void gfc_set_port(gfcrequest_t *gfr, unsigned short port){
    gfr->port = port;
}

void gfc_set_headerfunc(gfcrequest_t *gfr, void (*headerfunc)(void*, size_t, void *)){
    gfr->header_callback = headerfunc;
}

void gfc_set_headerarg(gfcrequest_t *gfr, void *headerarg){
    gfr->header_arg = headerarg;
}

void gfc_set_writefunc(gfcrequest_t *gfr, void (*writefunc)(void*, size_t, void *)){
    gfr->write_callback = writefunc;
}

void gfc_set_writearg(gfcrequest_t *gfr, void *writearg){
    gfr->write_arg = writearg;
}

int gfc_perform(gfcrequest_t* _gfr){
    _gfr->socket_fd = connect_to_server(_gfr->server, _gfr->port);
    send_request(_gfr);
    return process_response(_gfr);
}

gfstatus_t gfc_get_status(gfcrequest_t *gfr){
    return gfr->status;
}

char* gfc_strstatus(gfstatus_t status){
    char* ret;
    switch(status){
        case   GF_OK:
            ret = status_str[0];
            break;
        case GF_FILE_NOT_FOUND:
            ret = status_str[1];
            break;
        case GF_ERROR:
            ret = status_str[2];
            break;
        default:
            ret = status_str[3];
    }
    fprintf(stderr,"GFC STATUS END\n");
    
    return ret;
}

size_t gfc_get_filelen(gfcrequest_t *gfr){
    return gfr->file_length;
}

size_t gfc_get_bytesreceived(gfcrequest_t *gfr){
    return gfr->total_rx_bytes;
}

void gfc_cleanup(gfcrequest_t *gfr){
    free(gfr->path);
    free(gfr->server);
    free(gfr->end_sequence);
    close(gfr->socket_fd);
    free(gfr);
}

void gfc_global_init(){}
void gfc_global_cleanup(){}

void set_timeouts(int socket_fd){
    struct timeval timeout = {1, 0};  //Timeout = 1s
    
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&timeout, sizeof(struct timeval));
    setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, (struct timeval *)&timeout, sizeof(struct timeval));
}