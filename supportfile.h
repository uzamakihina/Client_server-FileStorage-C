#ifndef charbinary
#define charbinary
void getHash(char * hashname, char * contents);
void sendfile(FILE * fp, int socket);
void recvfile(char * fileContents, int socket);
void rest();
#define MAX_SIZE 65536
#define MAX_INPUT 4096
#define MAX_CLIENTS 10000

// Define all the codes used by client and responses sent by server

#define LIST_CLIENT "\x00"
#define LIST_SERVER '\x00'
#define LIST_RESPONSE_SERVER "\x01"
#define LIST_RESPONSE_CLIENT '\x01'

#define UPLOAD_CLIENT "\x02"
#define UPLOAD_SERVER '\x02'
#define UPLOAD_RESPONSE_SERVER "\x03"
#define UPLOAD_RESPONSE_CLIENT '\x03'

#define DELETE_CLIENT "\x04"
#define DELETE_SERVER '\x04'
#define DELETE_RESPONSE_SERVER "\x05"
#define DELETE_RESPONSE_CLIENT '\x05'

#define DOWNLOAD_CLIENT "\x06"
#define DOWNLOAD_SERVER '\x06'
#define DOWNLOAD_RESPONSE_SERVER "\x07"
#define DOWNLOAD_RESPONSE_CLIENT '\x07'

#define QUIT_CLIENT "\x08"
#define QUIT_SERVER '\x08'
#define QUIT_RESPONSE_SERVER "\x09"
#define QUIT_RESPONSE_CLIENT '\x09'

#define ERROR_SERVER "\xFF"
#define ERROR_CLIENT '\xFF'

#endif
