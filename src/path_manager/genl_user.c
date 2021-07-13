#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <poll.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>

#include <linux/genetlink.h>

#include "genl_header.h"

/* Generic macros for dealing with netlink sockets. Might be duplicated
 * elsewhere. It is recommended that commercial grade applications use
 * libnl or libnetlink and use the interfaces provided by the library
 */
#define GENLMSG_DATA(gh) ((void *)(NLMSG_DATA(gh) + GENL_HDRLEN))
#define GENLMSG_PAYLOAD(gh) (NLMSG_PAYLOAD(gh, 0) - GENL_HDRLEN)
#define NLA_DATA(na) ((void *)((char*)(na) + NLA_HDRLEN))

#define MESSAGE_TO_KERNEL "Hello World!"

struct nl_sock
{
    int fd;
    uint32_t pid;
    
    int protocol;
    uint32_t seq_id;

    int family_id;
    const char *name;
};

/* memory for netlink request and response messages
 * - headers are included
 */
struct gennl_ins_msg {
    struct nlmsghdr nh;
    struct genlmsghdr gh;
    char user[256];
};

static struct nl_sock instance = { .fd = -1,
                                   .name = GENL_INSTANCE_NAME };

int create_nl_sock(struct nl_sock *nl_sock, int protocol);
int lookup_gennl_family_id(struct nl_sock *nl_sock);
int gennl_interaction(struct nl_sock *nl_sock);

int main(char **argv, int argc)
{
    do {
        if (create_nl_sock(&instance, NETLINK_GENERIC) < 0) {
            break;
        }
        printf("create_nl_sock()\n");

        if (gennl_interaction(&instance)) {
            break;
        }
        printf("gennl_interaction()\n");
    } while (0);

    if (instance.fd > 0) {
        close(instance.fd);
        printf("close()\n");
        instance.fd = -1;
    }

    return 0;
}

int create_nl_sock(struct nl_sock *nl_sock, int protocol)
{
    int retval, rcvbuf = 1024;
    struct sockaddr_nl local, remote;

    nl_sock->fd = socket(AF_NETLINK, SOCK_RAW, protocol);
    if (nl_sock->fd < 0) {
        fprintf(stderr, "create socket errno: %d\n", errno);
        return errno;
    }
    
    nl_sock->protocol = protocol;
    nl_sock->seq_id = 1;
    
    if (setsockopt(nl_sock->fd, SOL_SOCKET, SO_RCVBUFFORCE,
                   &rcvbuf, sizeof(rcvbuf))) {
        printf("setting %d-bytes socket receive buffer failed, errno %d\n",
               rcvbuf, errno);
    }

    /* Connect to kernel (pid 0) as remote address. */
    memset(&remote, 0, sizeof(remote));
    remote.nl_family = AF_NETLINK;
    remote.nl_pid = 0;

    if (connect(nl_sock->fd, (struct sockaddr *) &remote,
                sizeof(remote)) < 0) {
        retval = errno;
        fprintf(stderr, "connect sock fail, errno %d\n", errno);

        goto error;
    }

    if (lookup_gennl_family_id(nl_sock)) {
        retval = -1;
        goto error;
    }

    return 0;

error:
    if (nl_sock->fd >= 0) {
        close(nl_sock->fd);
        nl_sock->fd = -1;
    }

    return retval;
}

int lookup_gennl_family_id(struct nl_sock *nl_sock)
{
    struct gennl_ins_msg request, reply;
    struct nlattr *nl_na;
    struct sockaddr_nl nl_address;
    int length;

    /* Step 1. prepare request msg */
    /* netlink header */
    request.nh.nlmsg_type = GENL_ID_CTRL;
    request.nh.nlmsg_flags = NLM_F_REQUEST;
    request.nh.nlmsg_seq = nl_sock->seq_id;
    request.nh.nlmsg_pid = getpid();
    request.nh.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);

    /* generic netlink header */
    request.gh.cmd = CTRL_CMD_GETFAMILY;
    request.gh.version = 0x1;

    /* assemble attr */
    nl_na = (struct nlattr *) GENLMSG_DATA(&request);
    nl_na->nla_type = CTRL_ATTR_FAMILY_NAME;
    nl_na->nla_len = strlen(GENL_INSTANCE_NAME) + 1 + NLA_HDRLEN;
    strcpy(NLA_DATA(nl_na), GENL_INSTANCE_NAME);

    request.nh.nlmsg_len += NLMSG_ALIGN(nl_na->nla_len);

    /* Step 2. send request msg */
    memset(&nl_address, 0, sizeof(nl_address));
    nl_address.nl_family = AF_NETLINK;

    length = sendto(nl_sock->fd, (char *) &request, request.nh.nlmsg_len,
                    0, (struct sockaddr *) &nl_address, sizeof(nl_address));
    if (length != request.nh.nlmsg_len) {
        fprintf(stderr, "%s sendto fail, %d\n", __func__, length);
        return -1;
    }

    /* Step 3. receive reply msg */
    length = recv(nl_sock->fd, &reply, sizeof(reply), 0);
    if (length < 0) {
        fprintf(stderr, "%s recv fail, %d\n", __func__, length);
        return -1;
    }

    /* Step 4. validate&parse reply msg */
    if (!NLMSG_OK((&reply.nh), length)) {
        fprintf(stderr, "family ID request: invalid message\n");
        return -1;
    }
    if (reply.nh.nlmsg_type == NLMSG_ERROR) {
        fprintf(stderr, "family ID request: receive error\n");
        return -1;
    }

    nl_na = (struct nlattr *) GENLMSG_DATA(&reply);
    nl_na = (struct nlattr *) ((char *) nl_na + NLA_ALIGN(nl_na->nla_len));
    if (nl_na->nla_type != CTRL_ATTR_FAMILY_ID) {
        fprintf(stderr, "family ID request: receive nla type(%d) not match %d\n",
                nl_na->nla_type, CTRL_ATTR_FAMILY_ID);
        return -1;    
    }

    nl_sock->family_id = *(__u16 *) NLA_DATA(nl_na);
    printf("%s genric netlink id %d\n",
           GENL_INSTANCE_NAME, nl_sock->family_id);

    return 0;
}

int gennl_interaction(struct nl_sock *nl_sock)
{
    struct gennl_ins_msg request, reply;
    struct nlattr *nl_na;
    struct sockaddr_nl nl_address;
    int length;

    memset(&request, 0, sizeof(request));
    memset(&reply, 0, sizeof(reply));


    request.nh.nlmsg_len = NLMSG_LENGTH(GENL_HDRLEN);
    request.nh.nlmsg_type = nl_sock->family_id;
    request.nh.nlmsg_flags = NLM_F_REQUEST;
    request.nh.nlmsg_seq = 60;
    request.nh.nlmsg_pid = getpid();

    request.gh.cmd = INSTANCE_C_ECHO;
    nl_na = (struct nlattr *) GENLMSG_DATA(&request);
    nl_na->nla_type = INSTANCE_A_MSG;
    nl_na->nla_len = sizeof(MESSAGE_TO_KERNEL) + NLA_HDRLEN;
    memcpy(NLA_DATA(nl_na), MESSAGE_TO_KERNEL, sizeof(MESSAGE_TO_KERNEL));

    request.nh.nlmsg_len += NLMSG_ALIGN(nl_na->nla_len);

    memset(&nl_address, 0, sizeof(nl_address));
    nl_address.nl_family = AF_NETLINK;

    length = sendto(nl_sock->fd, (char *) &request, request.nh.nlmsg_len,
                    0, (struct sockaddr *) &nl_address, sizeof(nl_address));
    if (length != request.nh.nlmsg_len) {
        fprintf(stderr, "%s sento return %d, expect %d\n", __func__,
                length, request.nh.nlmsg_len);
        return -1;
    }

    length = recv(nl_sock->fd, &reply, sizeof(reply), 0);
    if (length < 0) {
        printf("%s recv error %d\n", __func__, length);
        return -1;
    }

    if (!NLMSG_OK((&reply.nh), length)) {
        fprintf(stderr, "%s recv invalid nlmsg\n", __func__);
        return -1;
    }

    length = GENLMSG_PAYLOAD(&reply.nh);
    nl_na = (struct nlattr *) GENLMSG_DATA(&reply);
    printf("kernel replied: %s\n",(char *)NLA_DATA(nl_na));

    return 0;
}
