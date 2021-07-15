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

#include <linux/genetlink.h>

#include "../header/mptcp.h"
#include "../header/mptcp_netlink_api.h"
#include "./token.h"


/* generic netlik 매크로 */
#define GENLMSG_DATA(gh) ((void*)(NLMSG_DATA(gh) + GENL_HDRLEN))
#define GENLMSG_PAYLOAD(gh) (NLMSG_PAYLOAD(gh, 0) - GENL_HDRLEN)
#define NLA_DATA(na) ((void*)((char*)(na) + NLA_HDRLEN))

/* netlik socket 구조체 */
struct nl_sock {
	int fd;
	uint32_t pid;

	int protocol;
	uint32_t seq_id;

	int family_id;
	const char *name;
};

/* generic netlik request & response 구조체 */
struct gennl_ins_msg {
	struct nlmsghdr nh;
	struct genlmsghdr gh;
	char payload[1024];
};

/* generic netlik 전역 함수 */
static struct nl_sock instance = { 	
	.fd = -1,
	.name = MPTCP_GENL_NAME 
};


/* generic netlik function */
int create_gennl_sock(struct nl_sock*);
int lookup_gennl_family_id(struct nl_sock*);
// int gennl_interaction(struct nl_sock*);

/* etc function */
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
	printf("| netlink    | 1 |\n");
	printf("------------------\n");
	while(1){
		printf("Input the Manager Number >> ");
		scanf("%d", &manager_num);
	
		if(manager_num < 0 || manager_num > 1) 
			fprintf(stderr, "usage: 0~4\n");
		else
			break;
	}

	/* 선택된 Path Manager 설정 */
	switch(manager_num){
		case 0:
			manager = "default";
			break;
		case 1:
			manager = "netlink";
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
		perror("[client] setsockopt(MPTCP_ENABLED) ");
		return -1;
	}

	/* setsockopt()함수와 MPTCP_PATH_MANAGER(=44)상수를 사용하여 MPTCP의 Path Manager 변경 */
	ret = setsockopt(sock, SOL_TCP, MPTCP_PATH_MANAGER, manager, strlen(manager));
	if(ret < 0){
		perror("[client] setsockopt(MPTCP_PATH_MANAGER) ");
		return -1;
	}

	/* setsockopt()함수와 MPTCP_SCHEDULER(=43)상수를 사용하여 MPTCP의Packet Scheduler 변경 */
	ret = setsockopt(sock, SOL_TCP, MPTCP_SCHEDULER, scheduler, strlen(scheduler));
	if(ret < 0){
		perror("[client] setsockopt(MPTCP_SCHEDULER) ");
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

	/* generic netlik api craete */
	ret = create_gennl_sock(&instance);
	if(ret < 0) return -1;
	instance.family_id = lookup_gennl_family_id(&instance);





	/**
	 * MPTCP Signal 전송하기(using Genlink API)
	 *
	 * n초 마다 Signal을 전송
	 * 단, Count를 둬서 서로 다른 Signal을 전송
	 *
	 * 1 : add_addr
	 * 2 : create_subflow
	 **/
	struct gennl_ins_msg request;
	struct nlattr* nla;

	memset(&request, 0, sizeof(request));





	/* 1 : add_addr code start */

	uint32_t token = get_token(1); // get local token
	uint8_t loc_id = 1;
	uint16_t family = AF_INET;
	uint32_t saddr4 = inet_addr("192.168.1.10");

	request.nh.nlmsg_len = NLMSG_HDRLEN + GENL_HDRLEN;
	request.nh.nlmsg_type = instance.family_id;
	request.nh.nlmsg_flags = NLM_F_REQUEST;
	request.nh.nlmsg_pid = getpid();

	request.gh.cmd = MPTCP_CMD_ANNOUNCE;
	request.gh.version = MPTCP_GENL_VER;

	// Set token to nlattr
	nla = (struct nlattr*)request.payload;
	nla->nla_len = NLA_HDRLEN + sizeof(token);
	nla->nla_type = MPTCP_ATTR_TOKEN;
	memcpy((char*)nla + NLA_HDRLEN, &token, sizeof(token));
	request.nh.nlmsg_len += NLA_HDRLEN + NLA_ALIGN(sizeof(token));

	// Set loc_id to nlattr
	nla = (struct nlattr*)((char*)request.payload 
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(token))));
	nla->nla_len = NLA_HDRLEN + sizeof(loc_id);
	nla->nla_type = MPTCP_ATTR_LOC_ID;
	memcpy((char*)nla + NLA_HDRLEN, &loc_id, sizeof(loc_id));
	request.nh.nlmsg_len += NLA_HDRLEN + NLA_ALIGN(sizeof(loc_id));

	// Set family to nlattr
	nla = (struct nlattr*)((char*)request.payload 
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(token)))
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(loc_id))));
	nla->nla_len = NLA_HDRLEN + sizeof(family);
	nla->nla_type = MPTCP_ATTR_FAMILY;
	memcpy((char*)nla + NLA_HDRLEN, &family, sizeof(family));
	request.nh.nlmsg_len += NLA_HDRLEN + NLA_ALIGN(sizeof(family));

	// Set v4_source_addr(ip) to nlattr
	nla = (struct nlattr*)((char*)request.payload 
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(token)))
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(loc_id)))
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(family))));
	nla->nla_len = NLA_HDRLEN + sizeof(saddr4);
	nla->nla_type = MPTCP_ATTR_SADDR4;
	memcpy((char*)nla + NLA_HDRLEN, &saddr4, sizeof(saddr4));
	request.nh.nlmsg_len += NLA_HDRLEN + NLA_ALIGN(sizeof(saddr4));

	// Send to request
	struct sockaddr_nl nl_addr;
	memset(&nl_addr, 0, sizeof(nl_addr));
	nl_addr.nl_family = AF_NETLINK;

	ret = sendto(instance.fd, (char*)&request, request.nh.nlmsg_len,
			0, (struct sockaddr*)&nl_addr, sizeof(nl_addr));
	if(ret != request.nh.nlmsg_len) {
		perror("sendto(NETLINK_GENERIC) : ");
		fprintf(stderr, "sendto(NETLINK_GENERIC) errno : %d\n", errno);
		return -1;
	}

	/* 1 : add_addr code end */






	/* 2 : create_subflow code start */

	memset(&request, 0, sizeof(request));

	uint32_t token_ = get_token(0); // get local token
	uint16_t family_ = AF_INET;
	uint8_t loc_id_ = 1;
	uint8_t rem_id_ = 0;
	uint32_t daddr4_ = inet_addr(ADDR);
	uint16_t dport_ = ntohs(PORT);

	request.nh.nlmsg_len = NLMSG_HDRLEN + GENL_HDRLEN;
	request.nh.nlmsg_type = instance.family_id;
	request.nh.nlmsg_flags = NLM_F_REQUEST;
	request.nh.nlmsg_pid = getpid();

	request.gh.cmd = MPTCP_CMD_SUB_CREATE;
	request.gh.version = MPTCP_GENL_VER;

	// Set token to nlattr
	nla = (struct nlattr*)request.payload;
	nla->nla_len = NLA_HDRLEN + sizeof(token_);
	nla->nla_type = MPTCP_ATTR_TOKEN;
	memcpy((char*)nla + NLA_HDRLEN, &token_, sizeof(token_));
	request.nh.nlmsg_len += NLA_HDRLEN + NLA_ALIGN(sizeof(token_));

	// Set family to nlattr
	nla = (struct nlattr*)((char*)request.payload 
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(token_))));
	nla->nla_len = NLA_HDRLEN + sizeof(family_);
	nla->nla_type = MPTCP_ATTR_FAMILY;
	memcpy((char*)nla + NLA_HDRLEN, &family_, sizeof(family_));
	request.nh.nlmsg_len += NLA_HDRLEN + NLA_ALIGN(sizeof(family_));

	// Set loc_id to nlattr
	nla = (struct nlattr*)((char*)request.payload 
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(token_)))
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(family_))));
	nla->nla_len = NLA_HDRLEN + sizeof(loc_id_);
	nla->nla_type = MPTCP_ATTR_LOC_ID;
	memcpy((char*)nla + NLA_HDRLEN, &loc_id_, sizeof(loc_id_));
	request.nh.nlmsg_len += NLA_HDRLEN + NLA_ALIGN(sizeof(loc_id_));

	// Set rem_id to nlattr
	nla = (struct nlattr*)((char*)request.payload 
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(token_)))
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(family_)))
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(loc_id_))));
	nla->nla_len = NLA_HDRLEN + sizeof(rem_id_);
	nla->nla_type = MPTCP_ATTR_REM_ID;
	memcpy((char*)nla + NLA_HDRLEN, &rem_id_, sizeof(rem_id_));
	request.nh.nlmsg_len += NLA_HDRLEN + NLA_ALIGN(sizeof(rem_id_));

	// Set dest_address_ipv4 to nlattr
	nla = (struct nlattr*)((char*)request.payload
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(token_)))
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(family_)))
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(loc_id_)))
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(rem_id_))));
	nla->nla_len = NLA_HDRLEN + sizeof(daddr4_);
	nla->nla_type = MPTCP_ATTR_DADDR4;
	memcpy((char*)nla + NLA_HDRLEN, &daddr4_, sizeof(daddr4_));
	request.nh.nlmsg_len += NLA_HDRLEN + NLA_ALIGN(sizeof(daddr4_));

	// Set dest_port to nlattr
	nla = (struct nlattr*)((char*)request.payload
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(token_)))
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(family_)))
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(loc_id_)))
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(rem_id_)))
				+ (NLA_HDRLEN + NLA_ALIGN(sizeof(daddr4_))));
	nla->nla_len = NLA_HDRLEN + sizeof(dport_);
	nla->nla_type = MPTCP_ATTR_DPORT;
	memcpy((char*)nla + NLA_HDRLEN, &dport_, sizeof(dport_));
	request.nh.nlmsg_len += NLA_HDRLEN + NLA_ALIGN(sizeof(dport_));

	printf("\n");
	char *ptr = (char*)&request;
	for(int i=0; i<request.nh.nlmsg_len; i++){
		printf("%02x ", *(ptr+i));
	}
	printf("\n");

	// Send to request
	memset(&nl_addr, 0, sizeof(nl_addr));
	nl_addr.nl_family = AF_NETLINK;

	ret = sendto(instance.fd, (char*)&request, request.nh.nlmsg_len,
			0, (struct sockaddr*)&nl_addr, sizeof(nl_addr));
	if(ret != request.nh.nlmsg_len) {
		perror("sendto(NETLINK_GENERIC) : ");
		fprintf(stderr, "sendto(NETLINK_GENERIC) errno : %d\n", errno);
		return -1;
	}

	/* 2 : create_subflow code end */





	/*

	printf("[client] file sending...(%s) %dB\n", FILE_PATH, fsize);
	while(nsize!=fsize){
		int fpsize = fread(send_buff, 1, 1024, file);
		nsize += fpsize;

		send(sock, send_buff, fpsize, 0);
	}
	*/
	
	fclose(file);
	close(sock);

	return 0;
}

int create_gennl_sock(struct nl_sock *nl_sock)
{
	int ret;
	struct sockaddr_nl src, dest;

	nl_sock->fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
	if(nl_sock->fd < 0) {
		perror("[client] socket(NETLINK_GENERIC) : ");
		return -1;
	}

	nl_sock->protocol = NETLINK_GENERIC;
	nl_sock->seq_id = 1;

	/* Connect to kernel (pid 0) */
	memset(&dest, 0, sizeof(dest));
	dest.nl_family = AF_NETLINK;
	dest.nl_pid = 0;

	ret = connect(nl_sock->fd, (struct sockaddr *)&dest, sizeof(dest));
	if(ret < 0) {
		perror("[client] connect(NETLINK_GENERIC) : ");
		return -1;
	}

	return 0;	
}

int lookup_gennl_family_id(struct nl_sock *nl_sock)
{
	struct gennl_ins_msg request, response;
	struct nlattr *nl_na;
	struct sockaddr_nl nl_address;
	int ret;

	/* Step 1. Prepare request message */
	request.nh.nlmsg_type = GENL_ID_CTRL;
	request.nh.nlmsg_flags = NLM_F_REQUEST;
	request.nh.nlmsg_seq = nl_sock->seq_id;
	request.nh.nlmsg_pid = getpid();
	request.nh.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);

	request.gh.cmd = CTRL_CMD_GETFAMILY;
	request.gh.version = MPTCP_GENL_VER;

	nl_na = (struct nlattr*)request.payload;
	nl_na->nla_type = CTRL_ATTR_FAMILY_NAME;
	nl_na->nla_len = strlen(MPTCP_GENL_NAME) + 1 + NLA_HDRLEN;
	strcpy(NLA_DATA(nl_na), MPTCP_GENL_NAME);

	request.nh.nlmsg_len += NLMSG_ALIGN(nl_na->nla_len);

	
	/* Step 2. Send request message */
	memset(&nl_address, 0, sizeof(nl_address));
	nl_address.nl_family = AF_NETLINK;

	ret = sendto(nl_sock->fd, (char*)&request, request.nh.nlmsg_len,
			0, (struct sockaddr*)&nl_address, sizeof(nl_address));
	if(ret != request.nh.nlmsg_len) {
		perror("sendto(NETLINK_GENERIC) : ");
		return -1;
	}


	/* Step 3. Receive response message */
	ret = recv(nl_sock->fd, &response, sizeof(response), 0);
	if(ret < 0) {
		perror("recv(NETLINK_GENERIC) : ");
		return -1;
	}


	/* Step 4. Validate response message */
	if(!NLMSG_OK((&response.nh), ret)) {
		perror("NLMSG_OK() : ");
		return -1;
	}
	if(response.nh.nlmsg_type == NLMSG_ERROR) {
		perror("NLMSG_ERROR : ");
		return -1;
	}


	/* Step 5. Parse response message */
	nl_na = (struct nlattr*) GENLMSG_DATA(&response);
	nl_na = (struct nlattr*) ((char*)nl_na + NLA_ALIGN(nl_na->nla_len));
	if(nl_na->nla_type != CTRL_ATTR_FAMILY_ID) {
		perror("CTRL_ATTR_FAMILY_ID : ");
		return -1;
	}

	return *(__u16*)NLA_DATA(nl_na);
}















int get_fsize(FILE* file)
{
	int fsize;

	fseek(file, 0, SEEK_END);
	fsize=ftell(file);
	fseek(file, 0, SEEK_SET);	

	return fsize;
}
