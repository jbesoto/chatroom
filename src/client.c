/**
 * @file client.c
 *
 * @brief Client-side implementation for a chat room application.
 *
 * @author Juan Diego Becerra
 * @date   Apr 15, 2024
 */

#include <netdb.h>

#include "chatroom.h"

/**
 * @brief Entry point of the client program which connects to a server, sends
 * the client's name, and transmits messages.
 *
 * Handles command-line arguments to determine client's name and server details.
 * Establishes connection using specified or default server and port. Sends the
 * client's name and then enters a loop to send messages typed by the user,
 * until
 * '/exit' is typed. Exits if any step fails.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 *
 * @return Returns EXIT_SUCCESS on normal exit, or EXIT_FAILURE on any error
 *         condition.
 */
int main(int argc, char *argv[]) {
  int sockfd;
  char *name;
  in_port_t port;

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
    port = (in_port_t)strtol(argv[3], NULL, 10);
    if (port <= 0 || port > kMaxPort) {
      PrintError("Invalid port number: %s\n", argv[3]);
      return EXIT_FAILURE;
    }
  }

  sockfd = ConnectServerSocket(port, node);
  if (sockfd < -1) {
    return EXIT_FAILURE;
  }

  // Send name
  if (send(sockfd, name, strlen(name), 0) < 0) {
    PrintError("Failed to send name to server: %s\n", strerror(errno));
    close(sockfd);
    return EXIT_FAILURE;
  }

  while (1) {
    char buf[kMessageCharLimit];

    ssize_t msg_len = read(STDIN_FILENO, buf, kMessageCharLimit);
    if (msg_len < 0) {
      PrintError("Failed to read input: %s\n", strerror(errno));
      close(sockfd);
      return EXIT_FAILURE;
    }

    if (send(sockfd, buf, kMessageCharLimit, 0) < 0) {
      PrintError("Failed to send message: %s\n", strerror(errno));
      close(sockfd);
      return EXIT_FAILURE;
    }

    if (strncmp(buf, kExitCommand, strlen(kExitCommand)) == 0) {
      break;
    }
  }

  close(sockfd);
  return EXIT_SUCCESS;
}

/**
 * @brief Establishes a client connection to a server using a specified port and
 * node.
 *
 * This function resolves the server's hostname or IP address, creates a socket,
 * and establishes a connection. It reports errors for any failure in these
 * steps. It is meant to be used within network client implementations to handle
 * initial server connection setup.
 *
 * @param port The port number to connect on.
 * @param node The hostname or IP address of the server.
 *
 * @return Returns a socket file descriptor on success, or -1 on failure, with
 *         an appropriate error printed.
 */
int ConnectServerSocket(in_port_t port, const char *node) {
  int sockfd, status;
  char port_str[6];
  struct addrinfo *res, hints;

  snprintf(port_str, sizeof(port_str), "%d", port);

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  status = getaddrinfo(node, port_str, &hints, &res);
  if (status != 0) {
    PrintError("Failed to resolve hostname/IP: %s\n", gai_strerror(status));
    return -1;
  }

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    PrintError("Failed to create socket: %s\n", strerror(errno));
    freeaddrinfo(res);
    return -1;
  }

  if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
    PrintError("Failed to connect to socket: %s\n", strerror(errno));
    freeaddrinfo(res);
    close(sockfd);
    return -1;
  }
  freeaddrinfo(res);

  return sockfd;
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