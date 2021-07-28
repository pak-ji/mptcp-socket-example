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

#include <pthread.h>

#include <netlink/msg.h>
#include <netlink/attr.h>

#include "../../header/mptcp.h"
#include "../../header/mptcp_netlink_api.h"
//#include "../token/token.h"

#define DEBUG 0

/* mptcp subflow policy proceed thread */
void subflow_policy_thread(void*);

/* main function */
int main(int argc, char** argv)
{
	/**
	 * Variable of mptcp socket
	 */
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

	/**
	 * Variable of thread
	 */
	pthread_t p_thread[1];
	int th_id;
	int status;




	/**
	 * start main function
	 */
	if(argc != 2){
		fprintf(stderr, "usage: %s [port_number]\n", argv[0]);
		return -1;
	}
	PORT = atoi(argv[1]);

	/* start the thread */
	th_id = pthread_create(&p_thread[0], NULL, (void*)subflow_policy_thread, NULL);
	if(th_id < 0){
		perror("[main] pthread_create() ");
		return -1;
	}
	printf("[main] start the subflow policy thread\n");

	/* create socket */
	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(server_sock < 0){
		perror("[main] socket() ");
		return -1;
	}

	/* setsockopt()함수와 MPTCP_ENABLED(=42)상수를 사용하여 MPTCP Socket으로 Setup */
	ret = setsockopt(server_sock, SOL_TCP, MPTCP_ENABLED, &enable, sizeof(int));
	if(ret < 0){
		perror("[main] setsockopt(MPTCP_ENABLED) ");
		return -1;
	}

	/* setsockopt()함수와 MPTCP_PATH_MANAGER(=44)상수를 사용하여 MPTCP의 Path Manager 변경 */
	ret = setsockopt(server_sock, SOL_TCP, MPTCP_PATH_MANAGER, manager, strlen(manager));
	if(ret < 0){
		perror("[main] setsockopt(MPTCP_PATH_MANAGER) ");
		return -1;
	}

	/**
	 * bind mptcp socket
	 */
	memset(&server_addr, 0x00, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(PORT);

	ret = bind(server_sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
	if(ret < 0){
		perror("[main] bind() ");
		return -1;
	}	

	ret = listen(server_sock, 5);
	if(ret < 0){
		perror("[main] listen() ");
		return -1;
	}

	/**
	 * MPTCP Connection init (MP_CAPABLE)
	 */
	addr_len = sizeof(struct sockaddr_in);
	client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_len);	
	if(client_sock < 0){
		perror("[main] accept() ");
		return -1;
	}
	printf("[main] connected client\n");

	/**
	 * Receving data
	 */
	file = fopen(FILE_NAME, "wb");
	if(file == NULL){
		perror("[main] fopen() ");
		return -1;
	}

	do{
		nsize = recv(client_sock, buffer, 1024, 0);
		fwrite(buffer, sizeof(char), nsize, file);
	}while(nsize!=0);
	printf("[main] received file\n");
	
	fclose(file);
	close(client_sock);
	close(server_sock);

	pthread_join(p_thread[0], (void**)&status);
	return 0;
}



void subflow_policy_thread(void* arg)
{

	/**
	 * Variable of generic netlink
	 */
	struct 	nl_sock*	event_recv_sock	= NULL;
	struct 	nl_sock*	cmd_send_sock	= NULL;
	struct 	nl_msg*		send_msg	= NULL;
	char	recv_buff[1024]			= { '\0', };
	int	family_id;
	int	group_id;
	int 	check;
	int 	ret;

	/**
	 * Variable of receive event
	 */
	struct 	nlmsghdr*	nlh		= NULL;
	struct 	genlmsghdr*	genlh		= NULL;
	struct 	nlattr*		nla		= NULL;
	struct 	nlattr*		nla_tmp		= NULL;
	int 	nlmsg_len;
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
		perror("[thread] genl_connect(event_recv_sock)");
		return;
	}

	if(genl_connect(cmd_send_sock)){
		perror("[thread] genl_connect(cmd_send_sock");
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
		perror("[thread] nl_socket_add_membersihp() ");
		return;
	}


	/**
	 * recv event
	 */
	while(1){
		memset(recv_buff, 0, sizeof(recv_buff));
		ret = recv(nl_socket_get_fd(event_recv_sock), recv_buff, sizeof(recv_buff), 0);
		if(ret < 0){
			perror("[thread] recv() ");
			return;
		}

		nlh = (struct nlmsghdr*)recv_buff;
		genlh = (struct genlmsghdr*)((char*)nlh+16);
		nla = (struct nlattr*)genlmsg_data(genlh);

		nlmsg_len = nlh->nlmsg_len;
		nla_totlen = nlmsg_len - (16 + 4); // 16 - NLMSG_HDRLEN / 4 - GENLMSG_HDRLEN

		printf("[thread] received events : ");
		switch(genlh->cmd){
			case MPTCP_EVENT_CREATED:
				printf("MPTCP_EVENT_CREATED\n");
				break;

			case MPTCP_EVENT_ESTABLISHED:
				printf("MPTCP_EVENT_ESTABLISHED\n");
				
				nla_tmp = nla_find(nla, nla_totlen, MPTCP_ATTR_TOKEN);
				token = *(uint32_t*)nla_data(nla_tmp);

				printf("token : %X\n", token);

				break;

			case MPTCP_EVENT_CLOSED:
				printf("MPTCP_EVENT_CLOSED\n");
				break;

			case MPTCP_EVENT_ANNOUNCED:
				printf("MPTCP_EVNET_ANNOUNCED\n");
				break;

			default:
				printf("??\n");
		}
	}
}
