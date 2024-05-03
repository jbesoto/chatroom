#ifndef CHAT2_H_
#define CHAT2_H_

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

const size_t kMessageCharLimit = 4096;
const size_t kNameCharLimit = 64;
const in_port_t kDefaultPort = 13000;
const int kTimeout = 60000;  // milliseconds
const size_t kMaxClients = 10;
const size_t kMaxConnectionAttempts = 5;
const char *kPromptString = "> ";
const in_port_t kMaxPort = 65535;
const char *kDefaultHostname = "localhost";

void PrintUsage(void);
void PrintError(const char *format, ...);

// Server
int AcceptConnection(int sockfd);
int SetupServerSocket(in_port_t port, struct sockaddr_in *servaddr);

enum { kServer, kClient };

#endif  // CHAT2_H_