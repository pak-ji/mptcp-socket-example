/**
 * MPTCP Socket API Test App
 * File Sender with Path Manager (Client)
 * 
 * @date	: 2021-05-25(Thu)
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

#include "../header/mptcp.h"

int get_fsize(FILE* file);

/**
 * 기존의 TCP Client는 { socket() -> connect() -> recv(), send() -> close() }순서로 흘러간다.
 * 여기서 TCP Socket을 MPTCP Socket으로 설정하기 위해서는 socket()과 connect()사이에 setsockopt()을 사용한다.
 **/
int main(int argc, char** argv)
{
	char* ADDR;
	int PORT;
	char* FILE_PATH;

	int sock;
	struct sockaddr_in addr;
	int ret;

	FILE* file;
	char send_buff[1024] = { '\0', };
	int fsize = 0, nsize = 0;

	int enable = 1;

	char* scheduler = "roundrobin";

	int manager_num;
	char* manager;

	if(argc != 4){
		fprintf(stderr, "usage: %s [host_address] [port_number] [file_path]\n", argv[0]);
		return -1;
	}
	ADDR = argv[1];
	PORT = atoi(argv[2]);
	FILE_PATH = argv[3];

	/* 테스트할 Path Manager 선택 */
	printf("------------------\n");
	printf("| default    | 0 |\n");
	printf("| fullmesh   | 1 |\n");
	printf("| ndiffports | 2 |\n");
	printf("| binder     | 3 |\n");
	printf("------------------\n");
	while(1){
		printf("Input the Manager Number >> ");
		scanf("%d", &manager_num);
	
		if(manager_num < 0 || manager_num > 3) 
			fprintf(stderr, "usage: 0~3\n");
		else
			break;
	}

	/* 선택된 Path Manager 설정 */
	switch(manager_num){
		case 0:
			manager = "default";
			break;
		case 1:
			manager = "fullmesh";
			break;
		case 2:
			manager = "ndiffports";
			break;
		case 3:
			manager = "binder";
			break;
	}

	/* create socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0){
		perror("[client] socket() ");
		return -1;
	}

	/* setsockopt()함수와 MPTCP_ENABLED(=42)상수를 사용하여 MPTCP Socket으로 Setup */
	ret = setsockopt(sock, SOL_TCP, MPTCP_ENABLED, &enable, sizeof(int));
	if(ret < 0){
		perror("[server] setsockopt(MPTCP_ENABLED) ");
		return -1;
	}

	/* setsockopt()함수와 MPTCP_SCHEDULER(=44)상수를 사용하여 MPTCP의 Path Manager 변경 */
	ret = setsockopt(sock, SOL_TCP, MPTCP_PATH_MANAGER, manager, strlen(manager));
	if(ret < 0){
		perror("[server] setsockopt(MPTCP_PATH_MANAGER) ");
		return -1;
	}

	ret = setsockopt(sock, SOL_TCP, MPTCP_SCHEDULER, scheduler, strlen(scheduler));
	if(ret < 0){
		perror("[server] setsockopt(MPTCP_SCHEDULER) ");
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
	printf("[client] connected\n");

	file = fopen(FILE_PATH, "rb");
	if(file == NULL){
		perror("[client] fopen() ");
		return -1;
	}

	fsize = get_fsize(file);

	printf("[client] sending file...(%s)\n", FILE_PATH); 
	while(nsize!=fsize){
		int fpsize = fread(send_buff, 1, 1024, file);
		nsize += fpsize;
		printf("[client] file size %dB | send to %dB\n", fsize, nsize);
		send(sock, send_buff, fpsize, 0);
	}
	
	fclose(file);
	close(sock);

	return 0;
}

int get_fsize(FILE* file)
{
	int fsize;

	fseek(file, 0, SEEK_END);
	fsize=ftell(file);
	fseek(file, 0, SEEK_SET);	

	return fsize;
}
