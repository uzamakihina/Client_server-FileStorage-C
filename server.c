#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<string.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include<unistd.h>
#include<pthread.h>
#include<dirent.h>
#include<sys/stat.h>
#include <openssl/md5.h>
#include<signal.h>
#include"supportfile.h"
#include<semaphore.h>

// All of these need to be used by signal handlers or threads, so must be made global
sem_t mutex;
int fileCount = 0;
int serverSocket= 0;
char directory[MAX_INPUT];
pthread_t threadList[MAX_CLIENTS] = {0};
struct fileStruct {
   char hashname[MD5_DIGEST_LENGTH + 1];
   char ** knownas;
   int numAliases;
};
struct fileStruct * allFiles;

void * socketThread(void * arg);
void signalHandler(int sigNum);
void download(int socket);
void removeRemote(int socket);
void list(int socket);
void upload(int socket);
void quit(int socket);
void writeFile();
void readFile();
FILE * fopenAtDirectory(char * filename, char * mode);

int main(int argc, char * argv[]) {
    // If command line arguments are wrong, exit
    if (argc != 3) exit(0);
    // Set default IP address
    char * ipAddress = "127.0.0.1";
    // Append directory argument with / just in case it is not supplied in command line
    strcpy(directory, argv[1]);
    strcat(directory, "/");
    // Save the port number argument
    int portNumber;
    sscanf(argv[2], "%d", &portNumber);

    // Create signal handler
    struct sigaction sigActStruct;
  	memset (&sigActStruct, '\0', sizeof(sigActStruct));
  	sigActStruct.sa_handler = &signalHandler;
  	sigActStruct.sa_flags = SA_RESTART;
  	sigaction(SIGTERM, &sigActStruct, NULL);
    // Initiate mutex here, as it is destroyed in signal handler
    sem_init(&mutex, 0, 1);

    // Socket creation
    struct sockaddr_in serverAddr;
    struct sockaddr serverStorage;
    socklen_t addr_size = sizeof(serverStorage);

    // TCP connection create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(portNumber); /// port
    serverAddr.sin_addr.s_addr = inet_addr(ipAddress); // ip address local host
    // set all padding field to 0
    memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));

    // binds socket and address
    bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));

    // listen to requests
    listen(serverSocket,50);

    // Allocate space for one file to begin with
    allFiles = malloc(sizeof(struct fileStruct));
    // Read values into allFiles if .dedup exists and is formatted correctly
    readFile();
    //Accepting calls, creating a thread for each client that calls
    for (int i = 0; i < MAX_CLIENTS; i++) {
        int newSocket = accept(serverSocket, &serverStorage, &addr_size);
        if (newSocket >= 0) {
          int rc = pthread_create(&threadList[i], NULL, socketThread, &newSocket);
          if (rc != 0) printf("Failed to create thread\n");
        } else printf("Failed to connect\n");
    }
    // In the case that the arbritarily chosen MAX_CLIENTS is reached
    // Exit as though a SIGTERM got sent, ensuring server terminates gracefully
    signalHandler(SIGTERM);
}

// Appends filename to directory
// so it is reading and writing files in the directory specified for the server
FILE * fopenAtDirectory(char * filename, char * mode) {
  char fileAtDirectory[MAX_INPUT];
  strcpy(fileAtDirectory, directory);
  strcat(fileAtDirectory, filename);
  FILE * file = fopen(fileAtDirectory, mode);
  return file;
}

// Same as fopenAtDirectory but for removing a file in the specified directory
void removeAtDirectory(char * filename) {
  char fileAtDirectory[MAX_INPUT];
  strcpy(fileAtDirectory, directory);
  strcat(fileAtDirectory, filename);
  remove(fileAtDirectory);
}

// Interprets the .dedup file to read values into allFiles
// Returns without changing allFiles if badly formatted or .dedup does not exist
void readFile() {
  char line[MAX_INPUT];
  FILE * dedup = fopenAtDirectory(".dedup", "r");
  if (dedup == NULL) return;
  if (fgets(line, MAX_INPUT, dedup) == NULL) return; // xml version
  if (fgets(line, MAX_INPUT, dedup) == NULL) return; // <repo> opener
  if (fgets(line, MAX_INPUT, dedup) == NULL) return; // <file> or repo closer
  if (strcmp(line, "</repository>\n") == 0) return;  // if repo closer, nothing to read into allFiles
  if (strcmp(line, "\t<file>\n") != 0) return;       // if != <file>, badly formatted

  fgets(line, MAX_INPUT, dedup);                     // Gets first <hashname> line
  // Continue until no more files to read
  while (strcmp(line, "</repository>\n")) {
    int i = 0;
    int j = 0;
    struct fileStruct * file;

    while (line[i++] != '>'); // closes a tag
    if (line[i - 3] == 'm') { // if the tag is hashname, allocate space for a new file
      file = malloc(sizeof(struct fileStruct));
      allFiles = realloc(allFiles, sizeof(struct fileStruct) * ++fileCount);

      // copy hashname into new file
      memset(file->hashname, '\0', MD5_DIGEST_LENGTH + 1);
      while (line[i++] != '<') file->hashname[j++] = line[i - 1];
      file->hashname[i] = '\0';
      file->numAliases = 0;
      file->knownas = malloc( 0 );
    }
    else if (line[i - 2] == 's') { // if the tag is an knownas, allocate space for a new alias
      file->knownas = realloc(file->knownas, sizeof(char *) * (file->numAliases + 1));
      // copy knownas into new alias
      file->knownas[file->numAliases] = malloc( MAX_INPUT);
      while (line[i++] != '<') file->knownas[file->numAliases][j++] = line[i - 1];
      file->knownas[file->numAliases][j] = '\0';
      file->numAliases += 1;
      allFiles[fileCount - 1] = * file;
    }
    // get next line to parse
    fgets(line, MAX_INPUT, dedup);
    // if </file>, get next line
    if (line[3] == 'f') fgets(line, MAX_INPUT, dedup);
    // if <file>, get next line
    if (line[2] == 'f') fgets(line, MAX_INPUT, dedup);
  }
}

// Writes the contents of allFiles into an xml formatted file
void writeFile() {
  FILE * output = fopenAtDirectory(".dedup", "w");

  fprintf(output, "<?xml version=\"1.0\"?>\n");
  fprintf(output, "<repository>\n");
  for (int i = 0; i < fileCount; i++) {
    fprintf(output, "\t<file>\n");
    fprintf(output, "\t\t<hashname>%s</hashname>\n", allFiles[i].hashname);
    for (int j = 0; j < allFiles[i].numAliases; j++) {
      fprintf(output, "\t\t<knownas>%s</knownas>\n", allFiles[i].knownas[j]);
    }
    fprintf(output, "\t</file>\n");
  }
  fprintf(output, "</repository>\n");
}

void * socketThread(void * arg) {
  int newSocket = *((int *)arg);

  // Main loop for thread, implements every client command
  while (1) {
    // Time buffer
    rest();
    char code = '\0';
    // Checks if a command has arrived
    if (recv(newSocket, &code, 1, 0) == 0) continue;
    if      (code == LIST_SERVER) list(newSocket);
    else if (code == QUIT_SERVER) quit(newSocket);
    else if (code == UPLOAD_SERVER) upload(newSocket);
    else if (code == DELETE_SERVER) removeRemote(newSocket);
    else if (code == DOWNLOAD_SERVER)  download(newSocket);
    // Gives up lock on mutex after accessing allFiles
    sem_post(&mutex);
    // Allows thread to be cancelled again
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  }
}

void signalHandler(int sigNum) {
    if (sigNum == SIGTERM){
        // Request every thread to cancel
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (threadList[i]) pthread_cancel(threadList[i]);
        }
        // Every thread except AT MOST ONE, should have cancelled, then the one
        // with mutex will finish its current task and then join
        for (int i = 0; i < MAX_CLIENTS; i++) {
          if (threadList[i]) pthread_join(threadList[i], NULL);
        }
        // Close socket, destroy mutex, and then with no threads, it is safe to write
        // .dedup as nothing will be accessing allFiles concurrently
        close(serverSocket);
        sem_destroy(&mutex);
        writeFile();
        exit(0);
    }
}

// Download request i.e. send client a file
void download(int socket) {
  char alias[MAX_INPUT] = {'\0'};
  recv(socket, alias, MAX_INPUT, 0);
  // Get lock on mutex and set thread not cancelable
  sem_wait(&mutex);
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
  for (int i = 0; i < fileCount; i++) {
    for (int j = 0; j < allFiles[i].numAliases; j++) {
      // if a file with a matching name is found
      if (strcmp(allFiles[i].knownas[j], alias) == 0) {
        // send success response
        send(socket, DOWNLOAD_RESPONSE_SERVER, 1, 0);
        // Open file at the directory, and then use sendfile to send size and contents
        FILE * fp = fopenAtDirectory(allFiles[i].hashname, "r");
        sendfile(fp, socket);
        i = fileCount;
        return;
      }
    }
  }
  // If no files with a matching name are found
  send(socket, ERROR_SERVER, 1, 0);
  send(socket, "SERROR file not found", MAX_INPUT, 0);
}

// Delete request
void removeRemote(int socket) {
  char alias[MAX_INPUT] = {'\0'};
  recv(socket, alias, MAX_INPUT, 0);
  // Get lock on mutex and set thread not cancelable
  sem_wait(&mutex);
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
  for (int i = 0; i < fileCount; i++) {
    for (int j = 0; j < allFiles[i].numAliases; j++) {
      // if a file with a matching name is found
      if (strcmp(allFiles[i].knownas[j], alias) == 0) {
        // free the memory for the removed alias
        allFiles[i].numAliases -= 1;
        strcpy(allFiles[i].knownas[j], allFiles[i].knownas[allFiles[i].numAliases]);
        free(allFiles[i].knownas[allFiles[i].numAliases]);
        // if no aliases left, delete the file from the directory
        if (allFiles[i].numAliases == 0) {
          removeAtDirectory(allFiles[i].hashname);
          allFiles[i] = allFiles[--fileCount];
        }
        // send succes response
        send(socket, DELETE_RESPONSE_SERVER, 1, 0);
        return;
      }
    }
  }
  // If no files with a matching name are found
  send(socket, ERROR_SERVER, 1, 0);
  send(socket, "SERROR file not found", MAX_INPUT, 0);
}

// List request
void list(int socket) {
  send(socket, LIST_RESPONSE_SERVER, 1, 0);
  int N = 0;
  // Get lock on mutex and set thread not cancelable
  sem_wait(&mutex);
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
  for (int i = 0; i < fileCount; i++) {
    N += allFiles[i].numAliases;
  }
  // Send number of filenames available
  char toSend[] = {(N >> 8) & 0xFF, N & 0xFF};
  send(socket, toSend, 2, 0);

  // send all aliases
  for (int i = 0; i < fileCount; i++) {
    for (int j = 0; j < allFiles[i].numAliases; j++) {
      rest();
      send(socket, allFiles[i].knownas[j], MAX_INPUT, 0);
    }
  }
}

void upload(int socket) {
  // Get alias from client
  char alias[MAX_INPUT];
  memset(alias, '\0', MAX_INPUT);
  recv(socket, alias, MAX_INPUT, 0);

  // Get contents from client
  char fileContents[MAX_SIZE];
  recvfile(fileContents, socket);

  // Hash the contents with MD5
  char hashname[MD5_DIGEST_LENGTH + 1];
  memset(hashname, '\0', MD5_DIGEST_LENGTH + 1);
  getHash(hashname, fileContents);

  // Default is that the file contents are not already in the server
  int fileIndex = -1;

  // Get lock on mutex and set thread not cancelable
  sem_wait(&mutex);
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
  for (int i = 0; i < fileCount; i++) {
    if (strcmp(allFiles[i].hashname, hashname) == 0) fileIndex = i;

    // If any matching names, do not upload
    for (int j = 0; j < allFiles[i].numAliases; j++) {
      if (strcmp(allFiles[i].knownas[j], alias) == 0) {
        send(socket, ERROR_SERVER, 1, 0);
        send(socket, "SERROR file already exists", MAX_INPUT, 0);
        return;
      }
    }
  }

  // If found matching file contents, add an alias
  if (fileIndex >= 0) {
    allFiles[fileIndex].knownas = realloc(allFiles[fileIndex].knownas, sizeof(char *) * (allFiles[fileIndex].numAliases + 1));
    allFiles[fileIndex].knownas[allFiles[fileIndex].numAliases] = malloc(MAX_INPUT);
    strcpy(allFiles[fileIndex].knownas[allFiles[fileIndex].numAliases], alias);
    allFiles[fileIndex].numAliases += 1;
  } else { // Otherwise, add a new file
    allFiles = realloc(allFiles, sizeof(struct fileStruct) * (fileCount + 1));
    allFiles[fileCount].numAliases = 1;
    // Allocate space for aliases
    allFiles[fileCount].knownas = malloc(sizeof(char *));
    allFiles[fileCount].knownas[0] = malloc( MAX_INPUT);
    strcpy(allFiles[fileCount].hashname, hashname);
    strcpy(allFiles[fileCount].knownas[0], alias);

    ++fileCount;
    // Save the file contents
    FILE * newFile = fopenAtDirectory(hashname, "w");
    fputs(fileContents, newFile);
    fclose(newFile);
  }
  send(socket, UPLOAD_RESPONSE_SERVER, 1, 0);
}

// Quit request
void quit(int socket) {
  send(socket, QUIT_RESPONSE_SERVER, 1, 0);
  close(socket);
  pthread_exit(NULL);
}
