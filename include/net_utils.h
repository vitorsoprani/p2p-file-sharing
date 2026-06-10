#ifndef _NET_UTILS_H
#define _NET_UTILS_H

#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stddef.h>

void print_sockaddr_storage(struct sockaddr_storage *storage);
void print_addrinfo(struct addrinfo *addr);

struct addrinfo* resolve_tcp_address(const char *node, const char *service, int is_passive);
int bind_to_address(struct addrinfo *res, struct addrinfo **bound_info);
int connect_to_address(struct addrinfo *res, struct addrinfo **connected_info);
int start_listening(int sockfd, int backlog);
int accept_peer(int listen_fd, struct sockaddr_storage *peer_addr);
ssize_t recvall_block(int sock, void *buff, size_t len);

#endif /* _NET_UTILS_H */

