/**
 * MPTCP Socket API Test App
 * File Recevier with Path Manager (Server)
 * 
 * @date	: xxxx-xx-xx(xxx)
 * @author	: Ji-Hun(INSLAB)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <netlink/msg.h>
#include <netlink/attr.h>

#include "../../header/mptcp.h"
#include "../../header/mptcp_netlink_api.h"
#include "../token/token.h"

/**
 * 기존의 TCP Server는 { socket() -> bind() -> listen() -> accept() -> recv(), send() -> close() }순서로 흘러간다.
 * 여기서 TCP Socket을 MPTCP Socket으로 설정하기 위해서는 socket()과 bind()사이에 setsockopt()을 사용한다.
 **/
int main(int argc, char** argv)
{
	int PORT;
	const char* FILE_NAME = "recv_file";

	int server_sock, client_sock;
	struct sockaddr_in server_addr, client_addr;
	int len, addr_len, recv_len, ret;

	FILE *file;
	int fsize = 0, nsize = 0;
	char buffer[1024];

	int enable = 1;

	char* manager = "netlink";

	if(argc != 2){
		fprintf(stderr, "usage: %s [port_number]\n", argv[0]);
		return -1;
	}
	PORT = atoi(argv[1]);

	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(server_sock < 0){
		perror("[server] socket() ");
		return -1;
	}

	/* setsockopt()함수와 MPTCP_ENABLED(=42)상수를 사용하여 MPTCP Socket으로 Setup */
	ret = setsockopt(server_sock, SOL_TCP, MPTCP_ENABLED, &enable, sizeof(int));
	if(ret < 0){
		perror("[server] setsockopt(MPTCP_ENABLED) ");
		return -1;
	}

	/* setsockopt()함수와 MPTCP_PATH_MANAGER(=44)상수를 사용하여 MPTCP의 Path Manager 변경 */
	ret = setsockopt(server_sock, SOL_TCP, MPTCP_PATH_MANAGER, manager, strlen(manager));
	if(ret < 0){
		perror("[server] setsockopt(MPTCP_PATH_MANAGER) ");
		return -1;
	}


	/**
	 * libnl generic netlink api init
	 */
	struct nl_sock *nlsock = NULL;
	struct nl_msg* msg = NULL;

	int family_id;
	int check = 0;

	nlsock = nl_socket_alloc();
	if(!nlsock) {
		perror("nl_socket_alloc() ");
		return -1;
	}

	if(genl_connect(nlsock)) {
		perror("genl_connect() ");
		nl_socket_free(nlsock);
		return -1;
	}

	/**
	 * lookup mptcp genl family id
	 */
	family_id = genl_ctrl_resolve(nlsock, MPTCP_GENL_NAME);
	if(family_id < 0) {
		perror("genl_ctrl_resolve() ");
		nl_socket_free(nlsock);
		return -1;
	}


	/**
	 * Open mptcp socket
	 */
	memset(&server_addr, 0x00, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(PORT);

	ret = bind(server_sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
	if(ret < 0){
		perror("[server] bind() ");
		return -1;
	}	

	ret = listen(server_sock, 5);
	if(ret < 0){
		perror("[server] listen() ");
		return -1;
	}

	/**
	 * MPTCP Connection init (MP_CAPABLE)
	 */
	addr_len = sizeof(struct sockaddr_in);
	client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_len);	
	if(client_sock < 0){
		perror("[server] accept() ");
		return -1;
	}
	printf("[server] connected client\n");


	/**
	 * netlink api send message to kernel
	 * 
	 * @cmd : MPTCP_CMD_ANNOUNCE
	 *
	 * @attr : token	local token
	 * @attr : loc_id	local address id
	 * @attr : family	family(AF_INET or AF_INET6)
	 * @attr : saddr	local ip
	 */
	uint32_t token = get_token(1);
	uint8_t loc_id = 10;
	uint16_t family = AF_INET;
	uint32_t saddr = inet_addr("192.168.1.11");

	msg = nlmsg_alloc();
	if(!msg) {
		perror("nlmsg_alloc() ");
		nl_socket_free(nlsock);
		return -1;
	}

	if(!genlmsg_put(msg, getpid(), NL_AUTO_SEQ, family_id, 0,
				NLM_F_REQUEST, MPTCP_CMD_ANNOUNCE, MPTCP_GENL_VER)) {
		perror("genlmsg_putr() ");
		nl_socket_free(nlsock);
		return -1;
	}

	check += nla_put(msg, MPTCP_ATTR_TOKEN, sizeof(token), &token);
	check += nla_put(msg, MPTCP_ATTR_LOC_ID, sizeof(loc_id), &loc_id);
	check += nla_put(msg, MPTCP_ATTR_FAMILY, sizeof(family), &family);
	check += nla_put(msg, MPTCP_ATTR_SADDR4, sizeof(saddr), &saddr);

	if(check < 0) {
		perror("nla_putr() ");
		nlmsg_free(msg);
		nl_socket_free(nlsock);
		return -1;
	}
	check = 0;

	ret = nl_send_auto(nlsock, msg);
	if(ret < 0) {
		perror("nl_send_auto() ");
		nlmsg_free(msg);
		nl_socket_free(nlsock);
		return -1;
	}





	/**
	 * Receving data
	 */
	file = fopen(FILE_NAME, "wb");
	if(file == NULL){
		perror("[server] fopen() ");
		return -1;
	}

	do{
		nsize = recv(client_sock, buffer, 1024, 0);
		fwrite(buffer, sizeof(char), nsize, file);
	}while(nsize!=0);
	printf("[server] received file\n");
	
	fclose(file);
	close(client_sock);
	close(server_sock);

	return 0;
}
