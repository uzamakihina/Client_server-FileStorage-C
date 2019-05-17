#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <openssl/md5.h>
#include"supportfile.h"

void upload      (char * filename, int socket);
void removeRemote(char * filename, int socket);
void download    (char * filename, int socket);
void list                         (int socket);
void quit                         (int socket);

int main(int argc, char * argv[]) {
    // If command line arguments are wrong, exit
    if (argc != 3) exit(0);
    // Get IP address from command line
    char * ipAddress = argv[1];
    // Get port number from command line
    int portNumber;
    sscanf(argv[2], "%d", &portNumber);

    // initialize variables
    int sock_desc;
    struct sockaddr_in serv_addr;

    // make the socklet
    if((sock_desc = socket(AF_INET, SOCK_STREAM,0)) <0)
                printf("Failed making socket\n");
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ipAddress);
    serv_addr.sin_port = htons(portNumber);

    // // connect to the server
    while (connect(sock_desc, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1) {
      printf("Failed to connect to server, trying again.\n");
      sleep(1);
    }

    // Continuously get commands from client, optionally getting filename for commands
    char command;
    char filename[MAX_INPUT];
    for ( scanf("%s", &command) ;; scanf("%s", &command) ) {
        if (command == 'u' || command == 'd' || command == 'r') {
          scanf("%s", filename);
          if      (command == 'u') upload(filename, sock_desc);
          else if (command == 'd') download(filename, sock_desc);
          else if (command == 'r') removeRemote(filename, sock_desc);
        } else if (command == 'l') list(sock_desc);
          else if (command == 'q') quit(sock_desc);
    }
    return 0;
}

// Upload a file
void upload(char * filename, int socket) {
  // Verify file exists
  if ( access(filename, F_OK) == -1 ) {
    printf("CERROR file not found in local directory\n");
    return;
  }
  // Send code
  send(socket, UPLOAD_CLIENT, 1, 0);
  rest();
  // Send filename
  send(socket, filename, strlen(filename), 0);
  rest();
  // Send size and contents
  FILE * fp = fopen(filename, "r");
  sendfile(fp, socket);
  // Check for success
  char response;
  recv(socket, &response, 1, 0);
  if      (response == UPLOAD_RESPONSE_CLIENT) printf("OK\n");
  else if (response == ERROR_CLIENT) {
    char error[MAX_INPUT];
    recv(socket, error, MAX_INPUT, 0);
    puts(error);
  }
}

// request server to remove filename from server
void removeRemote(char * filename, int socket) {
  // send code
  send(socket, DELETE_CLIENT, 1, 0);
  rest();
  // send filename
  send(socket, filename, strlen(filename), 0);
  // check for success
  char response;
  recv(socket, &response, 1, 0);
  if      (response == DELETE_RESPONSE_CLIENT) printf("OK\n");
  else if (response == ERROR_CLIENT) {
    char error[MAX_INPUT];
    recv(socket, error, MAX_INPUT, 0);
    puts(error);
  }
}

// request server to download filename from server
void download(char * filename, int socket) {
  // send code
  send(socket, DOWNLOAD_CLIENT, 1, 0);
  rest();
  // send filename
  send(socket, filename, strlen(filename), 0);
  char fileContents[MAX_SIZE];
  char response;
  memset(fileContents, '\0', MAX_SIZE);
  // check if succesful
  recv(socket, &response, 1, 0);
  if (response == DOWNLOAD_RESPONSE_CLIENT) {
    // receive size and contents
    recvfile(fileContents, socket);
    // write to file
    FILE * downloadedFile = fopen(filename, "w");
    fputs(fileContents, downloadedFile);
    fclose(downloadedFile);
    printf("OK\n");
  } else if (response == ERROR_CLIENT) {
    char error[MAX_INPUT];
    recv(socket, error, MAX_INPUT, 0);
    puts(error);
  }
}

// request server to list files in directory
void list(int socket) {
  // send code
  send(socket, LIST_CLIENT, 1, 0);
  char server_message[MAX_INPUT];
  // check if successful
  recv(socket, server_message, 1, 0);
  if (server_message[0] != LIST_RESPONSE_CLIENT) return;
  rest();
  // get quantity of filenames
  recv(socket, server_message, 2, 0);
  int N = ((server_message[0] & 0xFF) << 8) + (server_message[1] & 0xFF);
  for (int i = 0; i < N; i++) {
    // get each filename
    recv(socket, server_message, MAX_INPUT,0);
    printf("OK+ %s\n", server_message);
  }
  printf("OK\n");
}

// tell server you are quitting
void quit (int socket) {
  // send code
  send(socket, QUIT_CLIENT, 1, 0);
  char response;
  // see if exited nicely, exit anyway
  recv(socket, &response, 1, 0);
  if (response == QUIT_RESPONSE_CLIENT) printf("OK\n");
  close(socket);
  exit(0);
}
