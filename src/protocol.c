/*
 * File: src/protocol.c
 * Autor: Vitor s Passamani (vitor.spassamani@gmail.com)
 * Implementação das funções utilitárias declaradas em protocol.h
 */

#include "../include/protocol.h"
#include "../include/log.h"
#include <stdlib.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h> /* strerror */

int parse_hdr(msg_hdr_t *hdr, uint8_t *buff)
{
	hdr->type = buff[0];
	buff += 1;

	memcpy(&(hdr->len), buff, sizeof(uint32_t));
	hdr->len = ntohl(hdr->len);

	log_info("Decoded header. type = %hu; len = %u", hdr->type, hdr->len);

	/* Função retorna int para fazer possíveis checagens de erro no futuro */
	return 0;
}
int parse_payload(void *payload, uint8_t *buff, uint8_t type, uint32_t len)
{
	if (type == MSG_ANNOUNCE) {
		announce_payload_t *announce = payload;
		announce->port = ntohs(*(uint16_t *)buff);
		log_info("Decoded announce ayload. port = %hu", announce->port);
		return 0;
	}

	if (type == MSG_HEARTBEAT) {
		return 0;
	}

	log_fatal("Message type not supported");
	exit(EXIT_FAILURE);
}
