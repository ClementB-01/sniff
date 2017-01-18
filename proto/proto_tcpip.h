#ifndef PROTO_TCPIP_H_
#define PROTO_TCPIP_H_


#define UDP_PORTTYP_START  0x0
#define TCP_PORTTYP_START  0x40

#define VLAN_FLAG           0x80000000

//  �����н���ע���õĶ˿�,�����Ķ˿�Ӱ��ɿ� /etc/services �ļ�
#define UDP_PORTTYP_UNKNOWN (UDP_PORTTYP_START+1)
#define TCP_PORTTYP_UNKNOWN (TCP_PORTTYP_START)

#define UDPPORTTYP_TFTP     (UDP_PORTTYP_START +10) //  69  �˿�
#define UDPPORTTYP_DNS      (UDP_PORTTYP_START +11) //  53  �˿�
#define UDPPORTTYP_QUIC     (UDP_PORTTYP_START +12) //  443 �˿�
#define UDPPORTTYP_NETBIOSNS    (UDP_PORTTYP_START +20) //  137  �˿�
#define UDPPORTTYP_NETBIOSDGM   (UDP_PORTTYP_START +21) //  138  �˿�

#define TCPPORTTYP_FTPDATA  (TCP_PORTTYP_START +1)  //  20  �˿�
#define TCPPORTTYP_FTPCMD   (TCP_PORTTYP_START +2)  //  21  �˿�
#define TCPPORTTYP_SSH      (TCP_PORTTYP_START +3)  //  22  �˿�
#define TCPPORTTYP_TELNET   (TCP_PORTTYP_START +4)  //  23  �˿�
#define TCPPORTTYP_SMTP     (TCP_PORTTYP_START +5)  //  25  �˿�
#define TCPPORTTYP_DNS      (TCP_PORTTYP_START +10) //  53  �˿�
#define TCPPORTTYP_BOOTPS   (TCP_PORTTYP_START +11) //  67  �˿�
#define TCPPORTTYP_BOOTPC   (TCP_PORTTYP_START +12) //  68  �˿�
#define TCPPORTTYP_HTTP     (TCP_PORTTYP_START +20) //  80  �˿�
#define TCPPORTTYP_POP3     (TCP_PORTTYP_START +21) //  110  �˿�
#define TCPPORTTYP_RPC      (TCP_PORTTYP_START +22) //  111  �˿�
#define TCPPORTTYP_SFTP     (TCP_PORTTYP_START +23) //  115  �˿�
#define TCPPORTTYP_NTP      (TCP_PORTTYP_START +24) //  123  �˿�
#define TCPPORTTYP_NETBIOSNS    (TCP_PORTTYP_START +25) //  137  �˿�
#define TCPPORTTYP_NETBIOSDGM   (TCP_PORTTYP_START +26) //  138  �˿�
#define TCPPORTTYP_NETBIOSSSN   (TCP_PORTTYP_START +27) //  139  �˿�
#define TCPPORTTYP_SNMP     (TCP_PORTTYP_START +30) //  161  �˿�
#define TCPPORTTYP_BGP      (TCP_PORTTYP_START +35) //  179  �˿�
#define TCPPORTTYP_IRC      (TCP_PORTTYP_START +36) //  194  �˿�
#define TCPPORTTYP_IMAP3    (TCP_PORTTYP_START +37) //  220  �˿�
#define TCPPORTTYP_HTTPS    (TCP_PORTTYP_START +38) //  443  �˿�
#define TCPPORTTYP_RDP      (TCP_PORTTYP_START +39) //  3389  �˿�

#define TCPPORTTYP_VNC      (TCP_PORTTYP_START +80) //  5900-5999  �˿�

#define IS_VNC_PORT(port)   ((port) >= 5900 && (port) <= 6599 )
#define IS_QUIC_PORT(port)   ((port == 443 || ((port) >= 5900 && (port) <= 6599 ))

#define IP_STR_LEN      16


#define PROTO_IGNORE_LIST   {   UDPPORTTYP_NETBIOSNS,   \
    UDPPORTTYP_NETBIOSDGM,  \
    TCPPORTTYP_SSH,         \
    TCPPORTTYP_NETBIOSNS,   \
    TCPPORTTYP_NETBIOSDGM,  \
    TCPPORTTYP_NETBIOSSSN,  \
    TCPPORTTYP_RDP,         \
    0                       \
}   
struct TcpIpInfo{
    char            srcip[IP_STR_LEN];
    char            dstip[IP_STR_LEN];
    uint16_t        srcport;
    uint16_t        dstport;

    uint8_t         servport_side;      //  ����˿�����λ��, 0:no  1:src, 2: dst */
    char            pad[3];

    const struct iphdr    *iphdr;
    union{
        const struct tcphdr   *tcphdr;
        const struct udphdr   *udphdr;
    };
    const unsigned char *content;
    int             contentlen;
};


void UDPQuic_DecInfo(const struct TcpIpInfo *ptTcpIp);

void Arp_DecInfo(const uint8_t *data,int len,int ucDecHex);
void TCP_DecInfo(const struct TcpIpInfo *ptTcpIp,uint16_t ipflag,int ucDecHex);
void UDP_DecInfo(const struct TcpIpInfo *ptTcpIp,uint16_t ipflag,int ucDecHex);

#endif
