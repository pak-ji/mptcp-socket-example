/**
 * MPTCP Socket API Test App
 * Echo Client
 * 
 * @date	: 2021-04-25(Sun)
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

/**
 * 기존의 TCP Client는 { socket() -> connect() -> recv(), send() -> close() }순서로 흘러간다.
 * 여기서 TCP Socket을 MPTCP Socket으로 설정하기 위해서는 socket()과 connect()사이에 setsockopt()을 사용한다.
 **/
int main(int argc, char** argv)
{
	char* ADDR;
	int PORT;

	int sock;
	struct sockaddr_in addr;
	char buffer[1024];
	const char *msg = "hello";
	int recv_len, ret;

	int enable = 1;

	if(argc != 3){
		fprintf(stderr, "usage: %s [host_address] [port_number]\n", argv[0]);
		return -1;
	}
	ADDR = argv[1];
	PORT = atoi(argv[2]);

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0){
		perror("[client] socket() ");
		return -1;
	}

	/* setsockopt()함수와 MPTCP_ENABLED(=42)상수를 사용하여 MPTCP Socket으로 Setup */
	setsockopt(sock, SOL_TCP, 42 /* MPTCP_ENABLED */, &enable, sizeof(int));
	if(ret < 0){
		perror("[server] setsockopt() ");
		return -1;
	}

	memset(&addr, 0x00, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ADDR);
	addr.sin_port = htons(PORT);

	ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
	if(ret < 0){
		perror("[client] connect() ");
		return -1;
	}

	ret = send(sock, msg, strlen(msg), 0);
	if(ret < 0){
		perror("[client] send() ");
		return -1;
	}

	recv_len = recv(sock, buffer, 1024, 0);
	if(recv_len < 0){
		perror("[client] recv() ");
		return -1;
	}

	buffer[recv_len] = '\0';

	printf("[client] received data : %s\n", buffer);

	close(sock);

	return 0;
}
