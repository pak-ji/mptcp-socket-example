/**
 * MPTCP Socket API Test App
 * Echo Server
 * 
 * @date	: 2021-04-25(Sun)
 * @author	: Ji-Hun(INSLAB)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>


/**
 * 기존의 TCP Server는 { socket() -> bind() -> listen() -> accept() -> recv(), send() -> close() }순서로 흘러간다.
 * 여기서 TCP Socket을 MPTCP Socket으로 설정하기 위해서는 socket()과 bind()사이에 setsockopt()을 사용한다.
 **/
int main(int argc, char** argv)
{
	int PORT;

	int server_sock, client_sock;
	struct sockaddr_in server_addr, client_addr;
	char buffer[1024];
	int len, addr_len, recv_len, ret;

	int enable = 1;

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
	ret = setsockopt(server_sock, SOL_TCP, 42 /*MPTCP_ENABLED*/, &enable, sizeof(int));
	if(ret < 0){
		perror("[server] setsockopt() ");
		return -1;
	}

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

	addr_len = sizeof(struct sockaddr_in);
	
	while(1){
		client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
		if(client_sock < 0){
			perror("[server] accept() ");
			break;
		}

		printf("[server] accept client ip : %s\n", inet_ntoa(client_addr.sin_addr));
		
		recv_len = recv(client_sock, buffer, sizeof(buffer), 0);
		if(recv_len < 0){
			perror("recv() ");
			break;
		}
		buffer[recv_len] = '\0';

		printf("[server] received data : %s\n", buffer);
		send(client_sock, buffer, strlen(buffer), 0);

		close(client_sock);
	}
	close(server_sock);

	return -1;
}
