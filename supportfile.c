#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<math.h>
#include <time.h>
#include<string.h>
#include"supportfile.h"
#include <openssl/md5.h>
#include <sys/socket.h>

// Return hash of contents in hashname
void getHash(char * hashname, char * contents) {
    // Enter the hash of the filecontents into MD5 hash function
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5_CTX mdContext;
    MD5_Init (&mdContext);
    MD5_Update (&mdContext, (unsigned char *) contents, strlen(contents));
    MD5_Final (hash,&mdContext);

    // Format hex characters into char array
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++) {
      sprintf(hashname + i, "%02x", hash[i]);
    }
    hashname[MD5_DIGEST_LENGTH] = '\0';
}

// Send contents of file in multiple sends
void sendfile(FILE * fp, int socket) {

  // Find size of file
  fseek(fp, 0, SEEK_END);
  int size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  // Send size, MSB first
  for (int i = 3; i >= 0; i--) {
    char sizeByte[] = {(size >> 8*i) & 0xFF};
    send(socket, sizeByte, 1, 0);
    rest();
  }
  // If zero-size file, no more to be sent
  if (size == 0) {fclose(fp); return;}

  // Send file MAX_INPUT by MAX_INPUT
  char buffer[MAX_INPUT];
  memset(buffer, 0, MAX_INPUT);
  while (size >= MAX_INPUT) {
    fread(buffer, MAX_INPUT, 1, fp);
    send(socket, buffer, MAX_INPUT, 0);
    rest();
    size -= MAX_INPUT;
  }
  // Send remainder of buffer < MAX_INPUT
  fread(buffer, size, 1, fp);
  send(socket, buffer, size, 0);
  fclose(fp);
}

// Receive contents of file in multiple receives
void recvfile(char * fileContents, int socket) {
  memset(fileContents, 0, MAX_INPUT);
  char buffer[MAX_INPUT];
  // Get size, MSB first
  int N = 0;
  for (int i = 3; i >= 0; i--) {
    recv(socket, buffer, 1, 0);
    N += (buffer[0] & 0xFF) << 8*i;
  }
  // If size == 0, nothing to write to file, return
  if (N == 0) return;

  // Receive contents MAX_INPUT by MAX_INPUT
  do {
    memset(buffer, 0, MAX_INPUT);
    recv(socket, buffer, MAX_INPUT,0);
    strcat(fileContents, buffer);
    N -= MAX_INPUT;
  } while (N > 0);
}

// Accomodate quick back-to-back sends
void rest() {
  const struct timespec ts = {0, 100000000};
  nanosleep(&ts, NULL);
}
