#ifndef CHATROOM_H_
#define CHATROOM_H_

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define kNameCharLimit 64
#define kMaxClients 10

const size_t kMessageCharLimit = 4096;
const in_port_t kDefaultPort = 13000;
const int kTimeout = 60000;  // milliseconds
const size_t kMaxConnectionAttempts = 5;
const char *kPromptString = "> ";
const in_port_t kMaxPort = 65535;
const char *kDefaultHostname = "localhost";

typedef struct {
  int connfd;
  int uid;
  char name[kNameCharLimit];
} client_t;

typedef struct {
  client_t *clients[kMaxClients];
  size_t len;
  pthread_mutex_t mutex;
} client_pool_t;

void PrintUsage(void);
void PrintError(const char *format, ...);

// Server
int AcceptConnection(client_pool_t *pool, int sockfd);
int SetupServerSocket(in_port_t port, struct sockaddr_in *servaddr);
int BroadcastMessage(client_pool_t *pool, char *msg, int uid);
void RemoveClient(client_pool_t *pool, int uid);
int AddClient(client_pool_t *pool, client_t *cli);

enum { kServer, kClient };

enum { CHATROOM_SUCCESS = 0, CHATROOM_CAPACITY_REACHED };

#endif  // CHATROOM_H_