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
#include <stdbool.h>
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

#define DEBUG 1

/** 
 * Global variable of cmd attributes  
 */
uint32_t	token	= 0;
uint16_t	family	= AF_INET;
uint32_t	saddr	= 0;
uint32_t	daddr	= 0;
uint16_t	dport	= 0;
uint8_t		loc_id	= 0;
uint8_t		rem_id	= 0;

/**
 * Global variable of mptcp event
 */
bool created = false;
bool established = false;
bool announced = false;
bool closed = false;
bool sub_established = false;





/* thread function */
void event_rx_thread(void*);

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
	th_id = pthread_create(&p_thread[0], NULL, (void*)event_rx_thread, NULL);
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





#if 1
	/**
	 * Send CMD to Netlink API
	 * Start
	 */ 
	struct 	nl_sock*	cmd_send_sock 	= NULL;
	struct 	nl_msg*		send_msg	= NULL;
	int 	family_id;
	
	cmd_send_sock = nl_socket_alloc();
	if(!cmd_send_sock){
		perror("[thread] cmd_send_sock = nl_socket_alloc() ");
		return -1;
	}
	
	if(genl_connect(cmd_send_sock)){
		perror("[thread] genl_connect(cmd_send_sock) ");
		return -1;
	}
	
	family_id = genl_ctrl_resolve(cmd_send_sock, MPTCP_GENL_NAME);
	if(family_id < 0){
		perror("[thread] genl_ctrl_resolve() ");
		return -1;
	}

	bool announce_event = false;
	char err_buff[256];
	char* ptr;
	struct nlmsghdr* nlm;
	while(1){
		if(created && established && !announce_event){
			printf("[main] CMD_ANNOUNCE\n");
			saddr = inet_addr("192.168.1.10");
			loc_id = 4;

			send_msg = nlmsg_alloc();

			genlmsg_put(send_msg, getpid(), NL_AUTO_SEQ, family_id, 0,
					NLM_F_REQUEST, MPTCP_CMD_ANNOUNCE, MPTCP_GENL_VER);

			nla_put(send_msg, MPTCP_ATTR_TOKEN, sizeof(token), &token);
			nla_put(send_msg, MPTCP_ATTR_FAMILY, sizeof(family), &family);
			nla_put(send_msg, MPTCP_ATTR_LOC_ID, sizeof(loc_id), &loc_id);
			nla_put(send_msg, MPTCP_ATTR_SADDR4, sizeof(saddr), &saddr);

			nl_send_auto(cmd_send_sock, send_msg);

			// FIXME
			memset(err_buff, 0, sizeof(err_buff));
			ret = recv(nl_socket_get_fd(cmd_send_sock), err_buff, sizeof(err_buff), 0);
			if(ret > 0){
				ptr = (char*)&err_buff[0];
				nlm = (struct nlmsghdr*)&err_buff[0];

				for(int i=0; i<nlm->nlmsg_len; i++){
					printf("%02X ", *(ptr+i));
				}
				printf("\n");

				if(nlm->nlmsg_type == NLMSG_ERROR){
					printf("cmd_announce error\n");
					//return -1;
				}
			}
			// FIXME

			nlmsg_free(send_msg);
			announce_event = true;
		}

		if(announce_event && announced){
			printf("[main] CMD_SUB_CREATE\n");
				
			dport = htons(dport);

			// token
			// family
			// loc_id
			// rem_id
			// saddr4
			// daddr4
			// dport
			
			send_msg = nlmsg_alloc();

			genlmsg_put(send_msg, getpid(), NL_AUTO_SEQ, family_id, 0,
					NLM_F_REQUEST, MPTCP_CMD_SUB_CREATE, MPTCP_GENL_VER);

			nla_put(send_msg, MPTCP_ATTR_TOKEN, sizeof(token), &token);
			nla_put(send_msg, MPTCP_ATTR_FAMILY, sizeof(family), &family);
			nla_put(send_msg, MPTCP_ATTR_LOC_ID, sizeof(loc_id), &loc_id);
			nla_put(send_msg, MPTCP_ATTR_REM_ID, sizeof(rem_id), &rem_id);
			nla_put(send_msg, MPTCP_ATTR_SADDR4, sizeof(saddr), &saddr);
			nla_put(send_msg, MPTCP_ATTR_DADDR4, sizeof(daddr), &daddr);
			nla_put(send_msg, MPTCP_ATTR_DPORT, sizeof(dport), &dport);

			// FIXME
			ptr = (char*)nlmsg_hdr(send_msg);
			int len = (struct nlmsghdr*)(nlmsg_hdr(send_msg))->nlmsg_len;
			for(int i=0; i<len; i++){
				printf("%02X ", *(ptr+i));
			}
			printf("\n\n");

			nl_send_auto(cmd_send_sock, send_msg);

			memset(err_buff, 0, sizeof(err_buff));
			ret = recv(nl_socket_get_fd(cmd_send_sock), err_buff, sizeof(err_buff), 0);
			if(ret > 0){
				ptr = (char*)&err_buff[0];
				nlm = (struct nlmsghdr*)&err_buff[0];

				for(int i=0; i<nlm->nlmsg_len; i++){
					printf("%02X ", *(ptr+i));
				}
				printf("\n");

				if(nlm->nlmsg_type == NLMSG_ERROR){
					printf("cmd_sub_create error\n");
					return -1;
				}
			}
			// FIXME


			nlmsg_free(send_msg);
			break;
		}
	}
	/**
	 * Send CMD to Netlink API
	 * End
	 */
#endif





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





void event_rx_thread(void *arg)
{

	/**
	 * Variable of generic netlink 
	 */
	struct 	nl_sock*	event_recv_sock = NULL;
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
	 * Socket init (sender, receiver)
	 */
	event_recv_sock = nl_socket_alloc();
	if(!event_recv_sock){
		perror("[thread] event_recv_sock = nl_socket_alloc() ");
		return;
	}

	if(genl_connect(event_recv_sock)){
		perror("[thread] genl_connect(event_recv_sock) ");
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
		genlh = (struct genlmsghdr*)genlmsg_hdr(nlh);
		nla = (struct nlattr*)genlmsg_data(genlh);

		nlmsg_len = nlh->nlmsg_len;
		nla_totlen = nlmsg_len - (16 + 4); // 16 = NLMSG_HDRLEN / 4 = GENLMSG_HDRLEN

#if DEBUG
		unsigned char* ptr = NULL;
#endif
		printf("[thread] received event : ");
		switch(genlh->cmd){
			case MPTCP_EVENT_CREATED:
				printf("MPTCP_EVENT_CREATED\n");

				nla_tmp = nla_find(nla, nla_totlen, MPTCP_ATTR_TOKEN);
				token = *(uint32_t*)nla_data(nla_tmp);

				nla_tmp = nla_find(nla, nla_totlen, MPTCP_ATTR_DPORT);
				dport = *(uint16_t*)nla_data(nla_tmp);

				nla_tmp = nla_find(nla, nla_totlen, MPTCP_ATTR_DADDR4);
				daddr = *(uint32_t*)nla_data(nla_tmp);

#if DEBUG
				ptr = (unsigned char*)&daddr;
				printf("[thread] token : %X\n", token);
				printf("[thread] dport : %d(%X)\n", dport, dport);
				printf("[thread] daddr : %u.%u.%u.%u\n", *(ptr+0), *(ptr+1), *(ptr+2), *(ptr+3));
#endif

				created = true;
				break;

			case MPTCP_EVENT_ESTABLISHED:
				printf("MPTCP_EVENT_ESTABLISHED\n");
				established = true;
				break;

			case MPTCP_EVENT_CLOSED:
				printf("MPTCP_EVENT_CLOSED\n");
				closed = true;
				return;
				break;

			case MPTCP_EVENT_ANNOUNCED:
				printf("MPTCP_EVENT_ANNOUNCED\n");

				nla_tmp = nla_find(nla, nla_totlen, MPTCP_ATTR_REM_ID);
				rem_id = *(uint8_t*)nla_data(nla_tmp);

				nla_tmp = nla_find(nla, nla_totlen, MPTCP_ATTR_DADDR4);
				daddr = *(uint32_t*)nla_data(nla_tmp);


#if DEBUG
				ptr = (unsigned char*)&daddr;
				printf("rem_id : %d\n", rem_id);
				printf("[thread] daddr : %u.%u.%u.%u\n", *(ptr+0), *(ptr+1), *(ptr+2), *(ptr+3));
#endif
				announced = true;
				break;

			case MPTCP_EVENT_SUB_CLOSED:
				printf("MPTCP_EVENT_SUB_CLOSED\n");
				break;

			default:
				printf("??\n");
#if DEBUG
				ptr = (char*)nlh;
				for(int i=0; i<nlmsg_len; i++){
					printf("%02X ", *(ptr+i));
				}
				printf("\n\n");
#endif


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
