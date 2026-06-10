#include "../include/net_utils.h"
#include "../include/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

void print_sockaddr_storage(struct sockaddr_storage *storage)
{
	void *in_addr;
	char *ipver;
	const char *dst;
	char ipstr[INET6_ADDRSTRLEN];
	unsigned short int port;

	if (storage->ss_family == AF_INET) { // IPV4
		struct sockaddr_in *ip = (struct sockaddr_in *)storage;
		in_addr = &(ip->sin_addr);
		ipver = "IPv4";
		port = ntohs(ip->sin_port);
	} else { // IPV6
		struct sockaddr_in6 *ip = (struct sockaddr_in6 *)storage;
		in_addr = &(ip->sin6_addr);
		ipver = "IPv6";
		port = ntohs(ip->sin6_port);
	}

	dst = inet_ntop(storage->ss_family, in_addr, ipstr, INET6_ADDRSTRLEN);
	if (dst == NULL) {
		log_error("entop error");
		exit(EXIT_FAILURE);
	}

	printf("%s - %s:%d\n", ipver, ipstr, port);
}

void print_addrinfo(struct addrinfo *addr)
{
	void *in_addr;
	char *ipver;
	const char *dst;
	char ipstr[INET6_ADDRSTRLEN];
	unsigned short int port;

	if (addr->ai_family == AF_INET) { // IPV4
		struct sockaddr_in *ip;
		ip = (struct sockaddr_in *)addr->ai_addr;
		in_addr = &(ip->sin_addr);
		ipver = "IPv4";
		port = ntohs(ip->sin_port);
	} else { // IPV6
		struct sockaddr_in6 *ip;
		ip = (struct sockaddr_in6 *)addr->ai_addr;
		in_addr = &(ip->sin6_addr);
		ipver = "IPv6";
		port = ntohs(ip->sin6_port);
	}

	dst = inet_ntop(addr->ai_family, in_addr, ipstr, INET6_ADDRSTRLEN);
	if (dst == NULL) {
		log_error("ntop error");
		exit(EXIT_FAILURE);
	}

	printf("%s - %s:%d\n", ipver, ipstr, port);
}

struct addrinfo* resolve_tcp_address(const char *node, const char *service, int is_passive)
{
	struct addrinfo hints = {0};
	struct addrinfo *res = NULL;
	int status;

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (is_passive) {
		hints.ai_flags = AI_PASSIVE;
	}

	status = getaddrinfo(node, service, &hints, &res);
	if (status != 0) {
		log_error("[Err] (resolve_tcp_address) gai error: %s\n",
			gai_strerror(status));
		return NULL;
	}
	return res;
}

int bind_to_address(struct addrinfo *res, struct addrinfo **bound_info)
{
	int sockfd = -1;
	struct addrinfo *p;
	int yes = 1;

	for (p = res; p != NULL; p = p->ai_next) {
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd < 0) {
			log_warn("socket error");
			continue;
		}

		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
			if (bound_info != NULL) {
				*bound_info = p;
			}
			return sockfd;
		}
		close(sockfd);
	}

	log_error("socket/bind error");
	return -1;
}

int connect_to_address(struct addrinfo *res, struct addrinfo **connected_info)
{
	int sockfd = -1;
	struct addrinfo *p;

	for (p = res; p != NULL; p = p->ai_next) {
		sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (sockfd < 0) {
			log_warn("socket error");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
			if (connected_info != NULL) {
				*connected_info = p;
			}
			return sockfd;
		}
		close(sockfd);
	}

	log_error("socket/connect error");
	return -1;
}

int start_listening(int sockfd, int backlog)
{
	if (listen(sockfd, backlog) < 0) {
		log_error("listen error");
		return -1;
	}
	return 0;
}

int accept_peer(int listen_fd, struct sockaddr_storage *peer_addr)
{
	socklen_t peer_addr_size = sizeof(struct sockaddr_storage);
	int con_sockfd = accept(listen_fd, (struct sockaddr *)peer_addr, &peer_addr_size);

	if (con_sockfd < 0) {
		log_error("accept error");
		return -1;
	}
	return con_sockfd;
}

int recvall_block(int sock, void *buff, size_t len)
{
	size_t bytes_left = len;
	size_t bytes_recvd;
	uint8_t *ptr = (uint8_t*)buff;

	while (bytes_left > 0) {
		bytes_recvd = recv(sock, buff, bytes_left, 0);

		if (bytes_recvd < 0) {
			if (errno == EINTR)
				continue;

			log_error("%s", strerror(errno));
			exit(EXIT_FAILURE); /* provisório */

			return -1; /* indica para o caller que deu ruim */
		}

		if (bytes_recvd == 0) {
			log_warn("Connection closed by peer");
			/* a conexão foi fechada */
			break;
		}

		bytes_left -= bytes_recvd;
		ptr += bytes_recvd;
	}

	return (len - bytes_recvd);
}
