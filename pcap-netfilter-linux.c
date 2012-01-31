/*
 * Copyright (c) 2011 Jakub Zawadzki
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote 
 * products derived from this software without specific prior written 
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "pcap-int.h"

#ifdef NEED_STRERROR_H
#include "strerror.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <time.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <linux/types.h>

#include <linux/netlink.h>
#include <linux/netfilter/nfnetlink.h>
#include <linux/netfilter/nfnetlink_log.h>

#include "pcap-netfilter-linux.h"

#define HDR_LENGTH (NLMSG_LENGTH(NLMSG_ALIGN(sizeof(struct nfgenmsg))))

#define NFLOG_IFACE "nflog"

static int
nflog_read_linux(pcap_t *handle, int max_packets, pcap_handler callback, u_char *user)
{
	const unsigned char *buf;
	int count = 0;
	int len;

	/* ignore interrupt system call error */
	do {
		len = recv(handle->fd, handle->buffer, handle->bufsize, 0);
		if (handle->break_loop) {
			handle->break_loop = 0;
			return -2;
		}
	} while ((len == -1) && (errno == EINTR));

	if (len < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "Can't receive packet %d:%s", errno, pcap_strerror(errno));
		return -1;
	}

	buf = handle->buffer;
	while (len >= NLMSG_SPACE(0)) {
		const struct nlmsghdr *nlh = (const struct nlmsghdr *) buf;
		u_int32_t msg_len;

		if (nlh->nlmsg_len < sizeof(struct nlmsghdr) || len < nlh->nlmsg_len) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "Message truncated: (got: %d) (nlmsg_len: %u)", len, nlh->nlmsg_len);
			return -1;
		}

		if (NFNL_SUBSYS_ID(nlh->nlmsg_type) == NFNL_SUBSYS_ULOG && 
			NFNL_MSG_TYPE(nlh->nlmsg_type) == NFULNL_MSG_PACKET) 
		{
			const unsigned char *payload = NULL;
			struct pcap_pkthdr pkth;

			if (handle->linktype != DLT_NFLOG) {
				const struct nfattr *payload_attr = NULL;

				if (nlh->nlmsg_len < HDR_LENGTH) {
					snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "Malformed message: (nlmsg_len: %u)", nlh->nlmsg_len);
					return -1;
				}

				if (nlh->nlmsg_len > HDR_LENGTH) {
					struct nfattr *attr = NFM_NFA(NLMSG_DATA(nlh));
					int attr_len = nlh->nlmsg_len - NLMSG_ALIGN(HDR_LENGTH);

					while (NFA_OK(attr, attr_len)) {
						switch (NFA_TYPE(attr)) {
							case NFULA_PAYLOAD:
								payload_attr = attr;
								break;
						}
						attr = NFA_NEXT(attr, attr_len);
					}
				}

				if (payload_attr) {
					payload = NFA_DATA(payload_attr);
					pkth.len = pkth.caplen = NFA_PAYLOAD(payload_attr);
				}

			} else {
				payload = NLMSG_DATA(nlh);
				pkth.caplen = pkth.len = nlh->nlmsg_len-NLMSG_ALIGN(sizeof(struct nlmsghdr));
			}

			if (payload) {
				/* pkth.caplen = min (payload_len, handle->snapshot); */

				gettimeofday(&pkth.ts, NULL);
				if (handle->fcode.bf_insns == NULL ||
						bpf_filter(handle->fcode.bf_insns, payload, pkth.len, pkth.caplen)) 
				{
					handle->md.packets_read++;
					callback(user, &pkth, payload);
					count++;
				}
			}
		}

		msg_len = NLMSG_ALIGN(nlh->nlmsg_len);
		if (msg_len > len)
			msg_len = len;

		len -= msg_len;
		buf += msg_len;
	}
	return count;
}

static int
netfilter_set_datalink(pcap_t *handle, int dlt)
{
	handle->linktype = dlt;
	return 0;
}

static int
netfilter_stats_linux(pcap_t *handle, struct pcap_stat *stats)
{
	stats->ps_recv = handle->md.packets_read;
	stats->ps_drop = 0;
	stats->ps_ifdrop = 0;
	return 0;
}

static int
netfilter_inject_linux(pcap_t *handle, const void *buf, size_t size)
{
	snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "inject not supported on netfilter devices");
	return (-1);
}                           

struct my_nfattr {
	u_int16_t nfa_len;
	u_int16_t nfa_type;
	void *data;
};

static int
nflog_send_config_msg(const pcap_t *handle, u_int8_t family, u_int16_t res_id, const struct my_nfattr *mynfa)
{
	char buf[1024] __attribute__ ((aligned));

	struct nlmsghdr *nlh = (struct nlmsghdr *) buf;
	struct nfgenmsg *nfg = (struct nfgenmsg *) (buf + sizeof(struct nlmsghdr));

	struct sockaddr_nl snl;
	static unsigned int seq_id;
	
	if (!seq_id)
		seq_id = time(NULL);
	++seq_id;

	nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct nfgenmsg));
	nlh->nlmsg_type = (NFNL_SUBSYS_ULOG << 8) | NFULNL_MSG_CONFIG;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_pid = 0;	/* to kernel */
	nlh->nlmsg_seq = seq_id;

	nfg->nfgen_family = family;
	nfg->version = NFNETLINK_V0;
	nfg->res_id = htons(res_id);

	if (mynfa) {
		struct nfattr *nfa = (struct nfattr *) (buf + NLMSG_ALIGN(nlh->nlmsg_len));

		nfa->nfa_type = mynfa->nfa_type;
		nfa->nfa_len = NFA_LENGTH(mynfa->nfa_len);
		memcpy(NFA_DATA(nfa), mynfa->data, mynfa->nfa_len);
		nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) + NFA_ALIGN(nfa->nfa_len);
	}

	memset(&snl, 0, sizeof(snl));
	snl.nl_family = AF_NETLINK;

	if (sendto(handle->fd, nlh, nlh->nlmsg_len, 0, (struct sockaddr *) &snl, sizeof(snl)) == -1)
		return -1;

	/* waiting for reply loop */
	do {
		socklen_t addrlen = sizeof(snl);
		int len;

		/* ignore interrupt system call error */
		do {
			len = recvfrom(handle->fd, buf, sizeof(buf), 0, (struct sockaddr *) &snl, &addrlen);
		} while ((len == -1) && (errno == EINTR));

		if (len <= 0)
			return len;

		if (addrlen != sizeof(snl) || snl.nl_family != AF_NETLINK) {
			errno = EINVAL;
			return -1;
		}

		nlh = (struct nlmsghdr *) buf;
		if (snl.nl_pid != 0 || seq_id != nlh->nlmsg_seq)	/* if not from kernel or wrong sequence skip */
			continue;

		while (len >= NLMSG_SPACE(0) && NLMSG_OK(nlh, len)) {
			if (nlh->nlmsg_type == NLMSG_ERROR || (nlh->nlmsg_type == NLMSG_DONE && nlh->nlmsg_flags & NLM_F_MULTI)) {
				if (nlh->nlmsg_len < NLMSG_ALIGN(sizeof(struct nlmsgerr))) {
					errno = EBADMSG;
					return -1;
				}
				errno = -(*((int *)NLMSG_DATA(nlh)));
				return (errno == 0) ? 0 : -1;
			}
			nlh = NLMSG_NEXT(nlh, len);
		}
	} while (1);

	return -1; /* never here */
}

static int
nflog_send_config_cmd(const pcap_t *handle, u_int16_t group_id, u_int8_t cmd, u_int8_t family)
{
	struct nfulnl_msg_config_cmd msg;
	struct my_nfattr nfa;

	msg.command = cmd;

	nfa.data = &msg;
	nfa.nfa_type = NFULA_CFG_CMD;
	nfa.nfa_len = sizeof(msg);

	return nflog_send_config_msg(handle, family, group_id, &nfa);
}

static int 
nflog_send_config_mode(const pcap_t *handle, u_int16_t group_id, u_int8_t copy_mode, u_int32_t copy_range)
{
	struct nfulnl_msg_config_mode msg;
	struct my_nfattr nfa;

	msg.copy_range = htonl(copy_range);
	msg.copy_mode = copy_mode;

	nfa.data = &msg;
	nfa.nfa_type = NFULA_CFG_MODE;
	nfa.nfa_len = sizeof(msg);

	return nflog_send_config_msg(handle, AF_UNSPEC, group_id, &nfa);
}

static int
nflog_activate(pcap_t* handle)
{
	const char *dev = handle->opt.source;
	unsigned short groups[32];
	int group_count = 0;
	int i;

	if (strncmp(dev, NFLOG_IFACE, strlen(NFLOG_IFACE)) == 0) {
		dev += strlen(NFLOG_IFACE);

		/* nflog:30,33,42 looks nice, allow it */
		if (*dev == ':')
			dev++;

		while (*dev) {
			long int group_id;
			char *end_dev;

			if (group_count == 32) {
				snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
						"Maximum 32 netfilter groups! dev: %s", 
						handle->opt.source);
				return PCAP_ERROR;
			}

			group_id = strtol(dev, &end_dev, 0);
			if (end_dev != dev) {
				if (group_id < 0 || group_id > 65535) {
					snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
							"Netfilter group range from 0 to 65535 (got %ld)",
							group_id);
					return PCAP_ERROR;
				}

				groups[group_count++] = (unsigned short) group_id;
				dev = end_dev;
			}
			if (*dev != ',')
				break;
			dev++;
		}
	}

	if (*dev) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE,
				"Can't get netfilter group(s) index from %s", 
				handle->opt.source);
		return PCAP_ERROR;
	}

	/* if no groups, add default: 0 */
	if (!group_count) {
		groups[0] = 0;
		group_count = 1;
	}

	/* Initialize some components of the pcap structure. */
	handle->bufsize = 128 + handle->snapshot;
	handle->offset = 0;
	handle->linktype = DLT_NFLOG;
	handle->read_op = nflog_read_linux;
	handle->inject_op = netfilter_inject_linux;
	handle->setfilter_op = install_bpf_program; /* no kernel filtering */
	handle->setdirection_op = NULL;
	handle->set_datalink_op = NULL;
	handle->set_datalink_op = netfilter_set_datalink;
	handle->getnonblock_op = pcap_getnonblock_fd;
	handle->setnonblock_op = pcap_setnonblock_fd;
	handle->stats_op = netfilter_stats_linux;

	/* Create netlink socket */
	handle->fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_NETFILTER);
	if (handle->fd < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "Can't create raw socket %d:%s", errno, pcap_strerror(errno));
		return PCAP_ERROR;
	}

	handle->dlt_list = (u_int *) malloc(sizeof(u_int) * 2);
	if (handle->dlt_list != NULL) {
		handle->dlt_list[0] = DLT_NFLOG;
		handle->dlt_list[1] = DLT_IPV4;
		handle->dlt_count = 2;
	}

	handle->buffer = malloc(handle->bufsize);
	if (!handle->buffer) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "Can't allocate dump buffer: %s", pcap_strerror(errno));
		goto close_fail;
	}

	if (nflog_send_config_cmd(handle, 0, NFULNL_CFG_CMD_PF_UNBIND, AF_INET) < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "NFULNL_CFG_CMD_PF_UNBIND: %s", pcap_strerror(errno));
		goto close_fail;
	}

	if (nflog_send_config_cmd(handle, 0, NFULNL_CFG_CMD_PF_BIND, AF_INET) < 0) {
		snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "NFULNL_CFG_CMD_PF_BIND: %s", pcap_strerror(errno));
		goto close_fail;
	}

	/* Bind socket to the nflog groups */
	for (i = 0; i < group_count; i++) {
		if (nflog_send_config_cmd(handle, groups[i], NFULNL_CFG_CMD_BIND, AF_UNSPEC) < 0) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "Can't listen on group group index: %s", pcap_strerror(errno));
			goto close_fail;
		}

		if (nflog_send_config_mode(handle, groups[i], NFULNL_COPY_PACKET, handle->snapshot) < 0) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "NFULNL_COPY_PACKET: %s", pcap_strerror(errno));
			goto close_fail;
		}
	}

	if (handle->opt.rfmon) {
		/*
		 * Monitor mode doesn't apply to netfilter devices.
		 */
		pcap_cleanup_live_common(handle);
		return PCAP_ERROR_RFMON_NOTSUP;
	}

	if (handle->opt.buffer_size != 0) {
		/*
		 * Set the socket buffer size to the specified value.
		 */
		if (setsockopt(handle->fd, SOL_SOCKET, SO_RCVBUF, &handle->opt.buffer_size, sizeof(handle->opt.buffer_size)) == -1) {
			snprintf(handle->errbuf, PCAP_ERRBUF_SIZE, "SO_RCVBUF: %s", pcap_strerror(errno));
			goto close_fail;
		}
	}

	handle->selectable_fd = handle->fd;
	return 0;

close_fail:
	pcap_cleanup_live_common(handle);
	return PCAP_ERROR;
}

pcap_t *
nflog_create(const char *device, char *ebuf)
{
	pcap_t *p;

	p = pcap_create_common(device, ebuf);
	if (p == NULL)
		return (NULL);

	p->activate_op = nflog_activate;
	return (p);
}

int 
netfilter_platform_finddevs(pcap_if_t **alldevsp, char *err_str)
{
	pcap_if_t *found_dev = *alldevsp;
	int sock;
	
	sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_NETFILTER);
	if (sock < 0) {
		/* if netlink is not supported this this is not fatal */
		if (errno == EAFNOSUPPORT)
			return 0;
		snprintf(err_str, PCAP_ERRBUF_SIZE, "Can't open netlink socket %d:%s",
			errno, pcap_strerror(errno));
		return -1;
	}
	close(sock);

	if (pcap_add_if(&found_dev, NFLOG_IFACE, 0, "Linux netfilter log (NFLOG) interface", err_str) < 0)
		return -1;
	return 0;
}

