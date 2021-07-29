/**
 * MPTCP Socket API Test App
 * File Sender with Path Manager (Client)
 * 
 * @date	: xxxx-xx-xx(xxx)
 * @author	: Ji-Hun(INSLAB)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>

#include <pthread.h>

#include <netlink/msg.h>
#include <netlink/attr.h>

#include "../../header/mptcp.h"
#include "../../header/mptcp_netlink_api.h"
//#include "../token/token.h"

#define DEBUG 0 

/* mptcp subflow policy proceed thread */
void subflow_policy_thread(void*);

/* etc function */
int get_fsize(FILE*);

/* main function */
int main(int argc, char** argv)
{

	/**
	 * Variable of mptcp socket
	 */
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
	char* manager = "netlink";



	/**
	 * Variable of thread
	 */
	pthread_t p_thread[1];
	int th_id;
	int status;






	/**
	 * Start main function
	 */
	if(argc != 4){
		fprintf(stderr, "usage: %s [host_address] [port_number] [file_path]\n", argv[0]);
		return -1;
	}
	ADDR = argv[1];
	PORT = atoi(argv[2]);
	FILE_PATH = argv[3];

	/* start the thread */
	th_id = pthread_create(&p_thread[0], NULL, (void*)subflow_policy_thread, NULL);
	if(th_id < 0){
		perror("[main] pthread_create() ");
		return -1;;
	}
	printf("[main] start the subflow policy thread\n");

	/* create socket */
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock < 0){
		perror("[main] socket() ");
		return -1;
	}

	/* setsockopt()함수와 MPTCP_ENABLED(=42)상수를 사용하여 MPTCP Socket으로 Setup */
	ret = setsockopt(sock, SOL_TCP, MPTCP_ENABLED, &enable, sizeof(int));
	if(ret < 0){
		perror("[main] setsockopt(MPTCP_ENABLED) ");
		return -1;
	}

	/* setsockopt()함수와 MPTCP_PATH_MANAGER(=44)상수를 사용하여 MPTCP의 Path Manager 변경 */
	ret = setsockopt(sock, SOL_TCP, MPTCP_PATH_MANAGER, manager, strlen(manager));
	if(ret < 0){
		perror("[main] setsockopt(MPTCP_PATH_MANAGER) ");
		return -1;
	}

	/* setsockopt()함수와 MPTCP_SCHEDULER(=43)상수를 사용하여 MPTCP의Packet Scheduler 변경 */
	ret = setsockopt(sock, SOL_TCP, MPTCP_SCHEDULER, scheduler, strlen(scheduler));
	if(ret < 0){
		perror("[main] setsockopt(MPTCP_SCHEDULER) ");
		return -1;
	}



	/**
	 * MPTCP Connection init (MP_CAPABLE)
	 */
	memset(&addr, 0x00, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ADDR);
	addr.sin_port = htons(PORT);

	ret = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
	if(ret < 0) {
		perror("[main] connect() ");
		return -1;
	}
	printf("[main] connected\n");



	/**
	 * Sending data
	 */
	file = fopen(FILE_PATH, "rb");
	if(file == NULL){
		perror("[main] fopen() ");
		return -1;
	}
	fsize = get_fsize(file);
	printf("[main] file sending...(%s) %dB\n", FILE_PATH, fsize);
	while(nsize!=fsize){
		int fpsize = fread(send_buff, 1, 1024, file);
		nsize += fpsize;
		send(sock, send_buff, fpsize, 0);
	}



	/**
	 * close
	 */	
	fclose(file);
	close(sock);

	pthread_join(p_thread[0], (void**)&status);
	return 0;
}





void subflow_policy_thread(void *arg)
{

	/**
	 * Variable of generic netlink 
	 */
	struct 	nl_sock*	event_recv_sock = NULL;
	struct 	nl_sock*	cmd_send_sock 	= NULL;
	struct 	nl_msg*		send_msg	= NULL;
	char 	recv_buff[1024]			= { '\0', };
	int	family_id;
	int	group_id;
	int	check;
	int 	ret;

	/**
	 * Variable of receive event
	 */
	struct	nlmsghdr*	nlh		= NULL;
	struct 	genlmsghdr*	genlh		= NULL;
	struct	nlattr*		nla		= NULL;
	struct 	nlattr*		nla_tmp		= NULL;
	int	nlmsg_len;
	int	nla_totlen;

	/** 
	 * Variable of cmd attributes  
	 */
	uint32_t	token	= 0;
	uint16_t	family	= AF_INET;
	uint32_t	saddr	= 0;
	uint32_t	daddr	= 0;
	uint16_t	dport	= 0;
	uint8_t		loc_id	= 0;
	uint8_t		rem_id	= 0;
	
	

	/**
	 * Socket init (sender, receiver)
	 */
	event_recv_sock = nl_socket_alloc();
	if(!event_recv_sock){
		perror("[thread] event_recv_sock = nl_socket_alloc() ");
		return;
	}

	cmd_send_sock = nl_socket_alloc();
	if(!cmd_send_sock){
		perror("[thread] cmd_send_sock = nl_socket_alloc() ");
		return;
	}

	if(genl_connect(event_recv_sock)){
		perror("[thread] genl_connect(event_recv_sock) ");
		return;
	}

	if(genl_connect(cmd_send_sock)){
		perror("[thread] genl_connect(cmd_send_sock) ");
		return;
	}



	/**
	 * lookup (family_id, group_id)
	 */
	family_id = genl_ctrl_resolve(event_recv_sock, MPTCP_GENL_NAME);
	if(family_id < 0){
		perror("[thread] genl_ctrl_resolve() ");
		return;
	}

	group_id = genl_ctrl_resolve_grp(event_recv_sock, MPTCP_GENL_NAME, MPTCP_GENL_EV_GRP_NAME);
	if(group_id < 0){
		perror("[thread] genl_ctrl_resolve_grp() ");
		return;
	}



	/**
	 * join group
	 */
	ret = nl_socket_add_membership(event_recv_sock, group_id);
	if(ret < 0){
		perror("[thread] nl_socket_add_membership() ");
		return;
	}



	/**
	 * while(1) 
	 * 	recv buff set zero bit
	 * 	recv event
	 * 	switch(event)
	 * 		case MPTCP_ESTABLISHED:
	 * 			execute CMD_ANNOUNCE
	 * 		case MPTCP_ANNOUNCED:
	 * 			execute CMD_SUB_CREATE
	 */
	while(1){
		memset(recv_buff, 0, sizeof(recv_buff));
		ret = recv(nl_socket_get_fd(event_recv_sock), recv_buff, sizeof(recv_buff), 0);
		if(ret < 0){
			perror("[thread] recv() ");
			return;
		}

		nlh = (struct nlmsghdr*)recv_buff;
		genlh = (struct genlmsghdr*)genlmsg_hdr(nlh);
		nla = (struct nlattr*)genlmsg_data(genlh);

		nlmsg_len = nlh->nlmsg_len;
		nla_totlen = nlmsg_len - (16 + 4); // 16 = NLMSG_HDRLEN / 4 = GENLMSG_HDRLEN

		printf("[thread] received event : ");
		switch(genlh->cmd){
			case MPTCP_EVENT_CREATED:
				printf("MPTCP_EVENT_CREATED\n");
				break;

			case MPTCP_EVENT_ESTABLISHED:
				printf("MPTCP_EVENT_ESTABLISHED\n");

				nla_tmp = nla_find(nla, nla_totlen, MPTCP_ATTR_TOKEN);
				token = *(uint32_t*)nla_data(nla_tmp);

				nla_tmp = nla_find(nla, nla_totlen, MPTCP_ATTR_DPORT);
				dport = *(uint16_t*)nla_data(nla_tmp);

				nla_tmp = nla_find(nla, nla_totlen, MPTCP_ATTR_DADDR4);
				daddr = *(uint32_t*)nla_data(nla_tmp);

				/**
				 * request the MPTCP_CMD_ANNOUNCE to netlink api
				 */
				saddr = inet_addr("192.168.1.10");
				loc_id = 4;

				send_msg = nlmsg_alloc();
				if(!send_msg){
					perror("nlmsg_alloc() ");
					return;
				}

				if(!genlmsg_put(send_msg, getpid(), NL_AUTO_SEQ, family_id, 0,
							NLM_F_REQUEST, MPTCP_CMD_ANNOUNCE, MPTCP_GENL_VER)){
					perror("genlmsg_put() ");
					return;
				}

				check += nla_put(send_msg, MPTCP_ATTR_TOKEN, sizeof(token), &token);
				check += nla_put(send_msg, MPTCP_ATTR_LOC_ID, sizeof(loc_id), &loc_id);
				check += nla_put(send_msg, MPTCP_ATTR_FAMILY, sizeof(family), &family);
				check += nla_put(send_msg, MPTCP_ATTR_SADDR4, sizeof(saddr), &saddr);

				if(check < 0){
					perror("nla_put() ");
					return;
				}
				check = 0;

				ret = nl_send_auto(cmd_send_sock, send_msg);
				if(ret < 0){
					perror("nl_send_auto() ");
					return;
				}
				nlmsg_free(send_msg);

				break;

			case MPTCP_EVENT_CLOSED:
				printf("MPTCP_EVENT_CLOSED\n");
				break;

			case MPTCP_EVENT_ANNOUNCED:
				printf("MPTCP_EVENT_ANNOUNCED\n");

				nla_tmp = nla_find(nla, nla_totlen, MPTCP_ATTR_REM_ID);
				rem_id = *(uint8_t*)nla_data(nla_tmp);

				printf("rem_id : %d\n", rem_id);

				break;

			case MPTCP_EVENT_SUB_CLOSED:
				printf("MPTCP_EVENT_SUB_CLOSED\n");
				break;

			default:
				printf("??\n");

				char* ptr = (char*)nlh;
				for(int i=0; i<nlmsg_len; i++){
					printf("%02X ", *(ptr+i));
				}
				printf("\n\n");


		}
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
