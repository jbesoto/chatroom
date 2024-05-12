/**
 * @file server.c
 *
 * @brief Server-side implementation for a chat room application.
 *
 * @author Juan Diego Becerra
 * @date   May 1, 2024
 */

#include "chatroom.h"

client_pool_t pool = {.clients = {NULL},
                      .len = 0, 
                      .mutex = PTHREAD_MUTEX_INITIALIZER};

/**
 * @brief Entry point for the server program.
 *
 * Initializes the server on a specified or default port, manages client
 * connections, and spawns threads for handling client communications. The
 * server listens indefinitely until terminated manually. It handles incoming
 * connections and manages a client pool, including thread creation for
 * each client.
 *
 * @param argc Number of command line arguments.
 * @param argv Array of command line argument strings. Can optionally include
 *             the port number as the first argument.
 *
 * @return Returns EXIT_SUCCESS on orderly shutdown, or EXIT_FAILURE on error
 *         or invalid input parameters.
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

  while (1) {
    client_t *client = AcceptConnection(sockfd);
    if (!client) {
      continue;
    }

    // TOOD: Add error checking
    pthread_create(&tid, NULL, &HandleClient, (void *)client);
  }

  close(sockfd);
  return EXIT_SUCCESS;
}

/**
 * @brief Sets up a server socket bound to the specified port.
 *
 * Creates a socket, binds it to the given port on the server's IP, and prepares
 * it to listen for incoming connections. If any step fails, it returns -1 and
 * closes the socket.
 *
 * @param port     The port number on which the server will listen.
 * @param servaddr Pointer to a sockaddr_in structure where server address
 *                 information will be stored.
 *
 * @return Returns the file descriptor of the created socket on success, or -1
 *         if any step of setting up the socket fails.
 */
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

/**
 * @brief Attempts to accept a new client connection using the given socket.
 *
 * This function repeatedly tries to accept a new connection up to a maximum
 * number of attempts. On success, it allocates a new client, assigns a unique
 * ID, and attempts to add the client to the pool. If the pool is full or if
 * any other error occurs during setup, it handles cleanup and reports the
 * error appropriately.
 *
 * @param sockfd Server socket file descriptor used to accept new connections.
 *
 * @return Returns a pointer to the allocated client on successful connection
 *         and client addition, otherwise return NULL.
 */
client_t *AcceptConnection(int sockfd) {
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
          return NULL;
      }
    }
  }
  if (attempts >= kMaxConnectionAttempts) {
    PrintError("Network error. Max retry attempts reached\n");
    return NULL;
  }

  client_t *client = malloc(sizeof(client_t));
  if (!client) {
    PrintError("Failed to allocate memory for client\n");
    close(connfd);
    return NULL;
  }
  client->connfd = connfd;
  client->uid = uid++;

  if (AddClient(client) < 0) {
    free(client);
    close(connfd);
    PrintError("Chatroom capacity reached. Connection rejected\n");
    return NULL;
  }

  return client;
}

/**
 * @brief Adds a new client to the client pool.
 *
 * Locks the pool mutex, checks for capacity, and adds the client to the pool
 * if space is available. If the pool is full, it returns an error without
 * adding the client.
 *
 * @param cli  Pointer to the client to be added to the pool.
 *
 * @return Returns 0 on successful addition, or -1 if the pool is full.
 */
int AddClient(client_t *cli) {
  pthread_mutex_lock(&(pool.mutex));

  if (pool.len + 1 >= kMaxClients) {
    pthread_mutex_unlock(&(pool.mutex));
    return -1;
  }
  pool.clients[pool.len] = cli;
  pool.len++;

  pthread_mutex_unlock(&(pool.mutex));

  return 0;
}

/**
 * @brief Handles client communications in a dedicated thread.
 *
 * Receives a client's name, broadcasts their joining message, then enters a
 * loop to handle all incoming messages from this client. It broadcasts messages
 * to all other clients and handles commands like "/exit". Cleans up and removes
 * the client on disconnection.
 *
 * @param arg Pointer to a client_context_t struct containing the client and
 *            pool data.
 *
 * @return Returns NULL after handling client disconnection and cleaning up.
 */
void *HandleClient(void *arg) {
  char name[kNameCharLimit];
  char msg[kMessageCharLimit];
  char buf[strlen(kPromptString) + kNameCharLimit + kMessageCharLimit];
  memset(buf, 0, sizeof(buf));

  client_t *cli = (client_t *)arg;

  // Set name
  ssize_t name_len = recv(cli->connfd, name, kNameCharLimit - 1, 0); 
  if (name_len <= 0) {
    PrintError("Failed to receive client name: %s\n", strerror(errno));
    goto close_connection;
  }
  memset(cli->name, 0, kNameCharLimit);
  strncpy(cli->name, name, name_len);
  cli->name[name_len - 1] = '\0';
  
  // Handle \r\n sent by telnet connections
  if (cli->name[name_len - 2] == '\r') {
    cli->name[name_len - 2] = '\0';
  }

  // Broadcast welcome message
  printf("Client joined the chat: %s\n", cli->name);
  snprintf(buf, sizeof(buf), "\n=== %s has joined the chat ===\n", cli->name);
  if (BroadcastMessage(buf, cli->uid) < 0) {
    PrintError("Failed to broadcast message: %s\n", strerror(errno));
    goto close_connection;
  }

  while (1) {
    ssize_t msg_len;

    msg_len = recv(cli->connfd, msg, kMessageCharLimit, 0);
    if (msg_len < 0) {
      PrintError("Failed to receive message: %s\n", strerror(errno));
      break;
    }
    if (msg_len == 0 || strncmp(msg, kExitCommand, strlen(kExitCommand)) == 0) {
      printf("Client left the chat: %s\n", cli->name);
      snprintf(buf, sizeof(buf), "\n=== %s has left the chat ===\n", cli->name);
      if (BroadcastMessage(buf, cli->uid) < 0) {
        PrintError("Failed to broadcast message: %s\n", strerror(errno));
      }
      break;
    }
    
    msg[msg_len - 1] = '\0';
    printf("%s sent a message: %s\n", cli->name, msg);
    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf), "%s%s%s\n", cli->name, kPromptString, msg);
    if (BroadcastMessage(buf, cli->uid) < 0) {
      PrintError("Failed to broadcast error: %s\n", strerror(errno));
      break;
    }
  }

close_connection:
  RemoveClient(cli->uid);
  pthread_detach(pthread_self());

  return NULL;
}

/**
 * @brief Broadcasts a message to all clients in the pool except the sender.
 *
 * Locks the pool mutex and iterates over all clients in the pool, sending
 * the message to each client except the one identified by uid.
 *
 * @param msg  The message to broadcast.
 * @param uid  User ID of the sender.
 *
 * @return Returns 0 if the message was successfully sent to all other clients,
 *         or -1 if an error occurred during sending.
 */
int BroadcastMessage(char *msg, int uid) {
  pthread_mutex_lock(&(pool.mutex));

  for (size_t i = 0; i < pool.len; i++) {
    client_t *client = pool.clients[i];

    if (client && client->uid != uid) {
      if (send(client->connfd, msg, kMessageCharLimit, 0) < 0) {
        pthread_mutex_unlock(&(pool.mutex));
        return -1;
      }
    }
  }

  pthread_mutex_unlock(&(pool.mutex));

  return 0;
}

/**
 * @brief Removes a client from the client pool based on their unique ID.
 *
 * Locks the pool mutex, searches for the client by uid, and removes them.
 * Compacts the client array to fill the gap left by the removed client.
 *
 * @param uid  User ID of the client to be removed.
 */
void RemoveClient(int uid) {
  pthread_mutex_lock(&(pool.mutex));

  for (size_t i = 0; i < pool.len; i++) {
    client_t *client = pool.clients[i];

    if (client && client->uid == uid) {
      close(client->connfd);
      free(client);

      // Eliminate gaps in client array
      for (size_t j = i; j < pool.len - 1; j++) {
        pool.clients[j] = pool.clients[j + 1];
      }

      pool.clients[pool.len - 1] = NULL;
      pool.len--;
      break;
    }
  }

  pthread_mutex_unlock(&(pool.mutex));
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