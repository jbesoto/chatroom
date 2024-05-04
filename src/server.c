/**
 * @file server.c
 *
 * @brief Server-side implementation for a chat room application.
 *
 * @author Juan Diego Becerra
 * @date   May 1, 2024
 */

#include "chatroom.h"

/**
 * @brief Initializes and runs the server for a chat application.
 *
 * Configures a server socket and binds it to a specified port. The server
 * listens for incoming connections and handles incoming messages in a loop
 * using poll. Manages connections, receives and sends messages, and performs
 * cleanup on errors or when terminated.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments: expected to contain the server
 *             name and optional port number.
 *
 * @return Returns EXIT_SUCCESS on normal termination and EXIT_FAILURE when
 *         an error occurs.
 */
int main(int argc, char *argv[]) {
  int sockfd;
  in_port_t port;
  struct sockaddr_in servaddr;
  pthread_t tid;

  if (argc > 2) {
    PrintUsage();
    return EXIT_FAILURE;
  }

  port = kDefaultPort;
  if (argc == 3) {
    port = (in_port_t)strtol(argv[1], NULL, 10);
    if (port <= 0 || port > kMaxPort) {
      PrintError("Invalid port number: %s\n", argv[1]);
      return EXIT_FAILURE;
    }
  }

  sockfd = SetupServerSocket(port, &servaddr);
  if (sockfd < 0) {
    PrintError("Failed to setup socket: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  client_pool_t pool = {.clients = {NULL}, 
                        .len = 0, 
                        .mutex = PTHREAD_MUTEX_INITIALIZER};
  
  while (1) {
    int status;

    status = AcceptConnection(&pool, sockfd);
    if (status < 0) {
      PrintError("Failed to accept connection: %s\n", strerror(errno));
      continue;
    }
    else if (status == CHATROOM_CAPACITY_REACHED) {
      puts("Chatroom capacity reached. Rejecting connection...");
      continue;
    }
  }

  // Cleanup
  close(sockfd);
  return EXIT_SUCCESS;

  // fds[kServer].fd = STDIN_FILENO;
  // fds[kServer].events = POLLIN;
  // fds[kServer].revents = 0;

  // const size_t kPromptSize = kNameCharLimit + strlen(kPromptString);
  // const size_t kBufferSize = kMessageCharLimit + kPromptSize + 1;

  // for (int exit_flag = 0;;) {
  //   if (exit_flag) {
  //     break;
  //   }

  //   connfd = AcceptConnection(sockfd);
  //   if (connfd < 0) {
  //     PrintError("Failed to accept connection: %s\n", strerror(errno));
  //     goto close_socket;
  //   }

  //   fds[kClient].fd = connfd;
  //   fds[kClient].events = POLLIN;
  //   fds[kClient].revents = 0;

  //   while (1) {
  //     char buf[kBufferSize];
  //     ssize_t msg_len;
  //     int events;

  //     events = poll(fds, kMaxClients, kTimeout);
  //     if (events < 0) {
  //       PrintError("Failed to poll: %s\n", strerror(errno));
  //       goto close_all;
  //     }

  //     if (fds[kClient].revents & POLLIN) {
  //       msg_len = recv(fds[kClient].fd, buf, sizeof(buf), 0);
  //       if (msg_len < 0) {
  //         PrintError("Failed to receive message: %s\n", strerror(errno));
  //         goto close_all;
  //       }
  //       if (!msg_len) {
  //         break;
  //       }
  //       printf("%s\n", buf);
  //     }
  //     else if (fds[kServer].revents & POLLIN) {
  //       int prompt_len;

  //       prompt_len = snprintf(buf, kPromptSize, "%s", kPromptString);
  //       if (prompt_len < 0) {
  //         PrintError("Failed to write prompt: %s\n", strerror(errno));
  //         goto close_all;
  //       }

  //       msg_len = read(fds[kServer].fd, buf + prompt_len, sizeof(buf));
  //       if (msg_len < 0) {
  //         PrintError("Failed to read input: %s\n", strerror(errno));
  //         goto close_all;
  //       }
  //       if (!msg_len) {
  //         exit_flag = 1;
  //         break;
  //       }
  //       msg_len += prompt_len;
  //       buf[msg_len - 1] = '\0';

  //       if (send(fds[kClient].fd, buf, kBufferSize, 0) < 0) {
  //         PrintError("Failed to send message: %s\n", strerror(errno));
  //         goto close_all;
  //       }
  //     }
  //   }
  // }
}

// Returns the file descriptor of the created socket and updates the servaddr
// structure with the information of the server...
int SetupServerSocket(in_port_t port, struct sockaddr_in *servaddr) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    return -1;
  }

  servaddr->sin_family = AF_INET;
  servaddr->sin_port = htons(port);
  servaddr->sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sockfd, (struct sockaddr *)servaddr, sizeof(*servaddr)) < 0) {
    close(sockfd);
    return -1;
  }

  if (listen(sockfd, kMaxClients) < 0) {
    close(sockfd);
    return -1;
  }

  return sockfd;
}

int AcceptConnection(client_pool_t *pool, int sockfd) {
  size_t attempts;
  int connfd;
  static int uid = 0;

  for (attempts = 0; attempts < kMaxConnectionAttempts; attempts++) {
    connfd = accept(sockfd, NULL, NULL);
    if (connfd >= 0) {
      break;
    } else {
      if (errno == EAGAIN) {
        continue;
      }
      // Treate network errors as EAGAIN for improved reliability
      switch (errno) {
        case ENETDOWN:
        case EPROTO:
        case ENOPROTOOPT:
        case EHOSTDOWN:
        case ENONET:
        case EHOSTUNREACH:
        case EOPNOTSUPP:
        case ENETUNREACH:
          printf("Network error: %s. Retrying... (%ld/%ld)\n", strerror(errno),
                 attempts, kMaxConnectionAttempts);
          continue;
        default:
          return -1;
      }
    }
  }
  if (attempts >= kMaxConnectionAttempts) {
    return -1;
  }

  client_t *client = malloc(sizeof(client_t));
  if (!client) {
    close(connfd);
    return -1;
  }
  client->connfd = connfd;
  client->uid = uid++;

  if (AddClient(pool, client) < 0) {
    free(client);
    close(connfd);
    return CHATROOM_CAPACITY_REACHED;
  }

  return 0;
}

int AddClient(client_pool_t *pool, client_t *cli) {
  pthread_mutex_lock(&(pool->mutex));

  if (pool->len + 1 >= kMaxClients) {
    pthread_mutex_unlock(&(pool->mutex));
    return -1;
  }
  pool->clients[pool->len] = cli;
  pool->len++;

  pthread_mutex_unlock(&(pool->mutex));

  return 0;
}

void RemoveClient(client_pool_t *pool, int uid) {
  pthread_mutex_lock(&(pool->mutex));

  for (size_t i = 0; i < pool->len; i++) {
    client_t *client = pool->clients[i];

    if (client && client->uid == uid) {
      free(client);

      // Eliminate gaps in client array
      for (size_t j = i; j < pool->len - 1; j++) {
        pool->clients[j] = pool->clients[j + 1];
      }

      pool->clients[pool->len - 1] = NULL;
      pool->len--;
      break;
    }
  }

  pthread_mutex_unlock(&(pool->mutex));
}

int BroadcastMessage(client_pool_t *pool, char *msg, int uid) {
  pthread_mutex_lock(&(pool->mutex));

  for (size_t i = 0; i < pool->len; i++) {
    client_t *client = pool->clients[i];

    if (client && client->uid != uid) {
      if (send(client->connfd, msg, strlen(msg), 0) < 0) {
        pthread_mutex_unlock(&(pool->mutex));
        return -1;
      }
    }
  }

  pthread_mutex_unlock(&(pool->mutex));

  return 0;
}

/**
 * @brief Displays usage information for the client-side program.
 */
void PrintUsage(void) {
  fprintf(stderr, "Usage: server [PORT]\n\n");
  fprintf(stderr, "Arguments:\n");
  fprintf(stderr, "  %-12s%s\n", "PORT",
          "Port number that the server will be listening to");
}

/**
 * @brief Prints a formatted error message to stderr.
 *
 * @param format The format string for the error message, followed by any
 *               arguments needed for formatting, similar to printf.
 */
void PrintError(const char *format, ...) {
  va_list args;
  va_start(args, format);

  fprintf(stderr, "server: ");
  vfprintf(stderr, format, args);

  va_end(args);
}