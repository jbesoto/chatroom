/**
 * @file client.c
 *
 * @brief Client-side implementation for a chat application.
 *
 * This file contains the main program logic for the client side of a chat
 * application. It handles the initialization of network communication,
 * manages user input and server responses, and maintains the connection
 * to the chat server. The client uses TCP/IP for communication, handles
 * error reporting, and processes command-line arguments for configuration.
 *
 * @author Juan Diego Becerra
 * @date   Apr 15, 2024
 */

#include <netdb.h>

#include "chatroom.h"

/**
 * @brief Initializes and runs the client for a chat application.
 *
 * Sets up a socket connection using provided command-line arguments for 
 * hostname and port. The client then enters a loop to handle input from stdin
 * and from the server using poll. It manages connection setup, input/output
 * operations, and error handling throughout the runtime of the application.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments: expected to contain the user's
 *             name, optional hostname/IP, and optional port number.
 *
 * @return Returns EXIT_SUCCESS upon successful operation and EXIT_FAILURE on
 *         error.
 */
int main(int argc, char *argv[]) {
  int sockfd, status;
  char *name;
  in_port_t port;
  struct pollfd fds[kChatCapacity];
  struct addrinfo *res, hints;
  char port_str[6];

  if (argc < 2 || argc > 4) {
    PrintUsage();
    return EXIT_FAILURE;
  }

  if (strlen(argv[1]) > kNameCharLimit) {
    PrintError("Name character limit (%s) exceeded\n", kNameCharLimit);
    return EXIT_FAILURE;
  }
  name = argv[1];

  const char *node = (argc == 3) ? argv[2] : kDefaultHostname;

  port = kDefaultPort;
  if (argc == 4) {
    port = (in_port_t) strtol(argv[3], NULL, 10);
    if (port <= 0 || port > kMaxPort) {
      PrintError("Invalid port number: %s\n", argv[3]);
      return EXIT_FAILURE;
    }
  }

  // String is required by getaddrinfo
  snprintf(port_str, sizeof(port_str), "%d", port);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  status = getaddrinfo(node, port_str, &hints, &res);
  if (status != 0) {
    PrintError("Failed to resolve hostname/IP: %s\n", gai_strerror(status));
    return EXIT_FAILURE;
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    PrintError("Failed to create socket: %s\n", strerror(errno));
    freeaddrinfo(res);
    return EXIT_FAILURE;
  }

  if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
    PrintError("Failed to connect to socket: %s\n", strerror(errno));
    freeaddrinfo(res);
    goto close_socket;
  }
  freeaddrinfo(res);

  fds[kClient].fd = STDIN_FILENO;
  fds[kClient].events = POLLIN;
  fds[kClient].revents = 0;

  fds[kServer].fd = sockfd;
  fds[kServer].events = POLLIN;
  fds[kServer].revents = 0;

  const size_t kPromptSize = kNameCharLimit + strlen(kPromptString);
  const size_t kBufferSize = kCharLimit + kPromptSize + 1;

  while (1) {
    char buf[kBufferSize];
    ssize_t msg_len;
    int events;

    events = poll(fds, kChatCapacity, kTimeout);
    if (events < 0) {
      PrintError("Failed to poll: %s\n", strerror(errno));
      goto close_socket;
    }
    else if (!events) {
      puts("Connection timeout. Exiting...");
      break;
    }

    if (fds[kServer].revents & POLLIN) {
      msg_len = recv(fds[kServer].fd, buf, sizeof(buf), 0);
      if (msg_len < 0) {
        PrintError("Failed to receive message: %s\n", strerror(errno));
        goto close_socket;
      } 
      if (!msg_len) {
        puts("Server exited. Exiting...");
        break;
      }
      printf("%s\n", buf);
    } 
    else if (fds[kClient].revents & POLLIN) {
      int prompt_len;

      prompt_len = snprintf(buf, kPromptSize, "%s%s", name, kPromptString);
      if (prompt_len < 0) {
        PrintError("Failed to write prompt: %s\n", strerror(errno));
        goto close_socket;
      }

      msg_len = read(fds[kClient].fd, buf + prompt_len, sizeof(buf));
      if (msg_len < 0) {
        PrintError("Failed to read input: %s\n", strerror(errno));
        goto close_socket;
      }
      msg_len += prompt_len;
      buf[msg_len - 1] = '\0';
      
      if (send(fds[kServer].fd, buf, kBufferSize, 0) < 0) {
        PrintError("Failed to send message: %s\n", strerror(errno));
        goto close_socket;
      }
    }
  }

  close(sockfd);
  return EXIT_SUCCESS;

close_socket:
  close(sockfd);
  return EXIT_FAILURE;
}

/**
 * @brief Displays usage information for the client-side program.
*/
void PrintUsage(void) {
  fprintf(stderr, "Usage: client <NAME> [HOSTNAME|IP] [PORT]\n\n");
  fprintf(stderr, "Arguments:\n");
  fprintf(stderr, "  %-12s%s\n", "NAME",
          "Name to be displayed with each message");
  fprintf(stderr, "  %-12s%s\n", "HOSTNAME",
          "Hostname of the server to connect to");
  fprintf(stderr, "  %-12s%s\n", "IP",
          "IP address of the server to connect to");
  fprintf(stderr, "  %-12s%s\n", "PORT",
          "Port number that the server will be listening to");
}

/**
 * @brief Prints a formatted error message to stderr.
 *
 * @param format The format string for the error message, followed by any
 *               arguments needed for formatting, similar to `printf`.
 */
void PrintError(const char *format, ...) {
  va_list args;
  va_start(args, format);

  fprintf(stderr, "client: ");
  vfprintf(stderr, format, args);

  va_end(args);
}