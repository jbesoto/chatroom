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
  int sockfd, connfd;
  in_port_t port;
  char *name;
  struct sockaddr_in servaddr;
  struct pollfd fds[kMaxClients];

  if (argc > 2) {
    PrintUsage();
    return EXIT_FAILURE;
  }

  port = kDefaultPort;
  if (argc == 3) {
    port = (in_port_t) strtol(argv[1], NULL, 10);
    if (port <= 0 || port > kMaxPort) {
      PrintError("Invalid port number: %s\n", argv[1]);
      return EXIT_FAILURE;
    }
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    PrintError("Failed to create socket: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(port);
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    PrintError("Failed to bind socket: %s\n", strerror(errno));
    goto close_socket;
  }

  if (listen(sockfd, kMaxClients) < 0) {
    PrintError("Failed to listen on socket: %s\n", strerror(errno));
    goto close_socket;
  }

  fds[kServer].fd = STDIN_FILENO;
  fds[kServer].events = POLLIN;
  fds[kServer].revents = 0;

  const size_t kPromptSize = kNameCharLimit + strlen(kPromptString);
  const size_t kBufferSize = kMessageCharLimit + kPromptSize + 1;

  for (int exit_flag = 0; ; ) {
    if (exit_flag) {
      break;
    }

    connfd = AcceptConnection(sockfd);
    if (connfd < 0) {
      PrintError("Failed to accept connection: %s\n", strerror(errno));
      goto close_socket;
    }

    fds[kClient].fd = connfd;
    fds[kClient].events = POLLIN;
    fds[kClient].revents = 0;

    while (1) {
      char buf[kBufferSize];
      ssize_t msg_len;
      int events;

      events = poll(fds, kMaxClients, kTimeout);
      if (events < 0) {
        PrintError("Failed to poll: %s\n", strerror(errno));
        goto close_all;
      }

      if (fds[kClient].revents & POLLIN) {
        msg_len = recv(fds[kClient].fd, buf, sizeof(buf), 0);
        if (msg_len < 0) {
          PrintError("Failed to receive message: %s\n", strerror(errno));
          goto close_all;
        }
        if (!msg_len) {
          break;
        }
        printf("%s\n", buf);
      } 
      else if (fds[kServer].revents & POLLIN) {
        int prompt_len;

        prompt_len = snprintf(buf, kPromptSize, "%s%s", name, kPromptString);
        if (prompt_len < 0) {
          PrintError("Failed to write prompt: %s\n", strerror(errno));
          goto close_all;
        }

        msg_len = read(fds[kServer].fd, buf + prompt_len, sizeof(buf));
        if (msg_len < 0) {
          PrintError("Failed to read input: %s\n", strerror(errno));
          goto close_all;
        }
        if (!msg_len) {
          exit_flag = 1;
          break;
        }
        msg_len += prompt_len;
        buf[msg_len - 1] = '\0';

        if (send(fds[kClient].fd, buf, kBufferSize, 0) < 0) {
          PrintError("Failed to send message: %s\n", strerror(errno));
          goto close_all;
        }
      }
    }
  }

  close(connfd);
  close(sockfd);
  return EXIT_SUCCESS;

close_all:
  close(connfd);
close_socket:
  close(sockfd);
  return EXIT_FAILURE;
}

/**
 * @brief Attempts to accept a new connection on the specified socket.
 *
 * This function tries to accept a new connection, handling potential network
 * errors and retrying a limited number of times before failing. It provides
 * detailed output in case of network issues that might temporarily prevent
 * connection acceptance.
 *
 * @param sockfd The socket file descriptor on which to accept the connection.
 *
 * @return Returns a new socket file descriptor for the accepted connection,
 *         or -1 if the connection attempt fails after retrying or due to
 *         non-recoverable errors.
 */
int AcceptConnection(int sockfd) {
  size_t attempts;
  int connfd;

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

  return connfd;
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