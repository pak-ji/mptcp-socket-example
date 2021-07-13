/**
 * MPTCP Option Header File
 *
 * @date	: 2021-05-25(Thu)
 * @author	: Ji-Hun(INSALB)
 */


#ifndef _UAPI_LINUX_TCP_H
#define _UAPI_LINUX_TCP_H

#ifndef __KERNEL__
#include <sys/socket.h>
#endif

#include <asm/byteorder.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/socket.h>
#include <linux/types.h>

#define MPTCP_ENABLED		42
#define MPTCP_SCHEDULER		43
#define MPTCP_PATH_MANAGER	44
#define MPTCP_INFO		45

#define MPTCP_INFO_FLAG_SAVE_MASTER	0x01

struct mptcp_meta_info {
	__u8	mptcpi_state;
	__u8	mptcpi_retransmits;
	__u8	mptcpi_probes;
	__u8	mptcpi_backoff;

	__u32	mptcpi_rto;
	__u32	mptcpi_unacked;

	/* Times. */
	__u32	mptcpi_last_data_sent;
	__u32	mptcpi_last_data_recv;
	__u32	mptcpi_last_ack_recv;

	__u32	mptcpi_total_retrans;

	__u64	mptcpi_bytes_acked;    /* RFC4898 tcpEStatsAppHCThruOctetsAcked */
	__u64	mptcpi_bytes_received; /* RFC4898 tcpEStatsAppHCThruOctetsReceived */
};

struct mptcp_sub_info {
	union {
		struct sockaddr src;
		struct sockaddr_in src_v4;
		struct sockaddr_in6 src_v6;
	};

	union {
		struct sockaddr dst;
		struct sockaddr_in dst_v4;
		struct sockaddr_in6 dst_v6;
	};
};

struct mptcp_info {
	__u32	tcp_info_len;	/* Length of each struct tcp_info in subflows pointer */
	__u32	sub_len;	/* Total length of memory pointed to by subflows pointer */
	__u32	meta_len;	/* Length of memory pointed to by meta_info */
	__u32	sub_info_len;	/* Length of each struct mptcp_sub_info in subflow_info pointer */
	__u32	total_sub_info_len;	/* Total length of memory pointed to by subflow_info */

	struct mptcp_meta_info	*meta_info;
	struct tcp_info		*initial;
	struct tcp_info		*subflows;	/* Pointer to array of tcp_info structs */
	struct mptcp_sub_info	*subflow_info;
};

#endif /* _UAPI_LINUX_TCP_H */
