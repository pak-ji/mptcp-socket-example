/**
 * MPTCP Socket API Test App
 * File Sender with Subflows Info (Client)
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
#include <time.h>

#include "../header/mptcp.h"

#define NIC_NUMBER 3 // 사용하는 NIC 개수 지정 (ex. 만약 사용중인 NIC가 eth0, wlan0, wlan1 일 시 -> 3)

void print_subflow_info(struct mptcp_info);
int get_fsize(FILE*);

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
	int val = MPTCP_INFO_FLAG_SAVE_MASTER;

	char *scheduler = "roundrobin";

	struct mptcp_info minfo;
	struct mptcp_meta_info meta_info;
	struct tcp_info initial;
	struct tcp_info others[NIC_NUMBER];
	struct mptcp_sub_info others_info[NIC_NUMBER];
	socklen_t len;

	clock_t start_t, end_t;

	/* mptcp subflow의 정보를 반환받기 위한 초기화 작업 */
	minfo.tcp_info_len = sizeof(struct tcp_info);
	minfo.sub_len = sizeof(others);
	minfo.meta_len = sizeof(struct mptcp_meta_info);
	minfo.meta_info = &meta_info;
	minfo.initial = &initial;
	minfo.subflows = others;
	minfo.sub_info_len = sizeof(struct mptcp_sub_info);
	minfo.total_sub_info_len = sizeof(others_info);
	minfo.subflow_info = others_info;
	len = sizeof(minfo);

	if(argc != 4){
		fprintf(stderr, "usage: %s [host_address] [port_number] [file_path]\n", argv[0]);
		return -1;
	}
	ADDR = argv[1];
	PORT = atoi(argv[2]);
	FILE_PATH = argv[3];

	/* create socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0){
		perror("[client] socket() ");
		return -1;
	}

	/* setsockopt()함수와 MPTCP_ENABLED(=42)상수를 사용하여 MPTCP Socket으로 Setup */
	ret = setsockopt(sock, SOL_TCP, MPTCP_ENABLED, &enable, sizeof(int));
	if(ret < 0){
		perror("[client] setsockopt(MPTCP_ENABLED) ");
		return -1;
	}

	/* setsockopt()함수와 MPTCP_SCHEDULER(=43)상수를 사용하여 패킷스케줄러를 라운드로빈으로 Setup */
	ret = setsockopt(sock, SOL_TCP, MPTCP_SCHEDULER, scheduler, strlen(scheduler));
	if(ret < 0){
		perror("[client] setsockopt(MPTCP_SCHEDULER) ");
		return -1;
	}
	
	/* setsockopt()함수와 MPTCP_INFO(=45)상수와 MPTCP_INFO_FLAG_SAVE_MASTER 옵션 사용으로 초기 Subflow info 유지시키기 */
	ret = setsockopt(sock, IPPROTO_TCP, MPTCP_INFO, &val, sizeof(val));
	if(ret < 0){
		perror("[client] setsockopt(MPTCP_INFO) ");
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

	start_t = clock();
	printf("[client] sending file...(%s)\n", FILE_PATH); 
	while(nsize!=fsize){
		int fpsize = fread(send_buff, 1, 1024, file);
		nsize += fpsize;
		send(sock, send_buff, fpsize, 0);

		/* 3초간격 getsockopt(MPTCP_INFO) */
		end_t = clock();
		if( 3.0 < (float)(end_t - start_t)/CLOCKS_PER_SEC){
			start_t = clock();
			getsockopt(sock, IPPROTO_TCP, MPTCP_INFO, &minfo, &len);
			print_subflow_info(minfo); // Subflow 정보 출력
		}
	}
	
	fclose(file);
	close(sock);

	return 0;
}



void print_subflow_info(struct mptcp_info minfo)
{
	int subflows_info_len = minfo.total_sub_info_len;
	int subflows_number = subflows_info_len / sizeof(struct mptcp_sub_info);
	int i;

	printf("-----------------------\n");
	printf("[subflows number] %d\n", subflows_number);
	for(i=0; i<subflows_number; i++){
		printf("[subflow %d] %s:%d ", i, inet_ntoa(minfo.subflow_info[i].src_v4.sin_addr), ntohs(minfo.subflow_info[i].src_v4.sin_port));
		printf("-> %s:%d", inet_ntoa(minfo.subflow_info[i].dst_v4.sin_addr), ntohs(minfo.subflow_info[i].dst_v4.sin_port));

	        printf("  { rto %d, ato %d , rtt %d, snd_mss %d, rcv_mss %d, total_retrans %d }\n", 
				minfo.subflows[i].tcpi_rto,
				minfo.subflows[i].tcpi_ato, 
				minfo.subflows[i].tcpi_rtt, 
				minfo.subflows[i].tcpi_snd_mss, 
				minfo.subflows[i].tcpi_rcv_mss, 
				minfo.subflows[i].tcpi_total_retrans ); 
	}
}

int get_fsize(FILE* file)
{
	int fsize;

	fseek(file, 0, SEEK_END);
	fsize=ftell(file);
	fseek(file, 0, SEEK_SET);	

	return fsize;
}
