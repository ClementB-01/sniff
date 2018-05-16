

#define _GNU_SOURCE
#include <getopt.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <poll.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <signal.h>

#include "sniff_error.h"
#include "sniff.h"




struct normalrcv_ctl{
    unsigned char       buf[PER_PACKET_SIZE];
    struct  timeval     ts;
};

/*  mmap��ʽ���ղ���        */
struct mmaprcv_ctl{
    char                *buf;      /*  Ӧ��mmap�հ�ʱ�����λ�����  */
    int                 qnum;      /*  ��������֡��Ŀ              */
    int                 index;     /*  �������ڵ�ǰδ������±�    */
    struct tpacket_hdr  *curhdr;   /*  ��ǰ�������֡��Ϣ          */
    struct  timeval     ts;
};

/*-----------------------------------------------------------------------------
 *  
 *  �������
 *  
 -----------------------------------------------------------------------------*/
struct ethdev_ctl{
    int                 sd;             /*  �׽�����    */

    int                 ifindex;        /*  ����������  */
    int                 lo_ifindex;     /*  �ػ��ӿ�������  */
    int                 promisc;        /*  �Ƿ�ʹ���˻���ģʽ  */

    union{
        struct normalrcv_ctl    normalinfo;
        struct mmaprcv_ctl      mmapinfo;
    };
};



#if 0
/** 
 * @brief �����������õ�����id
 * 
 * @param fd 
 * @param device 
 * 
 * @return  -1:     ����
 *          ����:   �ɹ�
 */
static int iface_get_id(int fd, const char *device)
{
    struct ifreq	ifr;

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, device, sizeof(ifr.ifr_name));

    if (ioctl(fd, SIOCGIFINDEX, &ifr) == -1) {
        PRN_MSG("SIOCGIFINDEX %s: %d", device,errno);
        return 0;
    }

    return ifr.ifr_ifindex;
}
#else
#define iface_get_id(sd,device) if_nametoindex(device)
#endif


/** 
 * @brief ��raw socket�󶨵�ָ��������
 * 
 * @param fd 
 * @param ifindex 
 */
static int iface_bind(int fd, int ifindex)
{
    struct sockaddr_ll	sll;
    int			err;
    socklen_t		errlen = sizeof(err);

    memset(&sll, 0, sizeof(sll));
    sll.sll_family		= AF_PACKET;
    sll.sll_ifindex		= ifindex;
    sll.sll_protocol	= htons(ETH_P_ALL);

    if (bind(fd, (struct sockaddr *) &sll, sizeof(sll)) == -1) {
        if (errno == ENETDOWN) {
            /*
             * Return a "network down" indication, so that
             * the application can report that rather than
             * saying we had a mysterious failure and
             * suggest that they report a problem to the
             * libpcap developers.
             */
            PRN_MSG("net down ");
            return -1;
        } else {
            PRN_MSG("bind err :%d ",errno);
            return -1;
        }
    }

    /* Any pending errors, e.g., network is down? */


    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen) == -1) {
        PRN_MSG("bind err and get sock opt err:%d ",errno);
        return 0;
    }

    if (err == ENETDOWN) {
        /*
         * Return a "network down" indication, so that
         * the application can report that rather than
         * saying we had a mysterious failure and
         * suggest that they report a problem to the
         * libpcap developers.
         */
        PRN_MSG("net down ");
        return -1;
    } else if (err > 0) {
        PRN_MSG("sock old err :%d ",errno);
        return 0;
    }

    return 1;
}

static char * iface_mmap(int sd,int framenum)
{
    /*  sample:   http://hi.baidu.com/ah__fu/blog/item/8aadf895fad570007af48000.html
     *  bug:      https://lists.linux-foundation.org/pipermail/bugme-new/2003-October/009110.html
     */
    char    *buf            = NULL;
    struct tpacket_req      req;
    int     pagesize        = getpagesize();

    if( !framenum ) /*  0   ��ʾ��ʹ��mmap��ʽ      */
        return NULL;

    memset(&req,0,sizeof(req));
    req.tp_block_size       = pagesize;
    req.tp_block_nr         = (PER_PACKET_SIZE * framenum) /pagesize;
    req.tp_frame_size       = PER_PACKET_SIZE;
    req.tp_frame_nr         = framenum;

    if( setsockopt(sd, SOL_PACKET, PACKET_RX_RING, (void *) &req, sizeof(req)) ){
        perror("pkt_mmap fail");
        return NULL;
    }

    buf = (char*)mmap(
            0, (PER_PACKET_SIZE * framenum), 
            PROT_READ|PROT_WRITE, MAP_SHARED, sd, 0);

    if( !buf ){
        perror("mmap fail");
        memset(&req, 0, sizeof(req));
        setsockopt(sd, SOL_PACKET, PACKET_RX_RING, &req, sizeof(req));
    }

    return buf;
}

static void iface_munmap(int sd,int framenum,char *buf)
{
    struct tpacket_req      req;
    if( framenum ){
        memset(&req,0,sizeof(req));
        setsockopt(sd, SOL_PACKET, PACKET_RX_RING, &req, sizeof(req));
        munmap(buf,(PER_PACKET_SIZE * framenum));
    }
}

static int iface_promisc(int sd,int ifindex)
{
    struct packet_mreq	mr;
    memset(&mr, 0, sizeof(mr));
    mr.mr_ifindex = ifindex;
    mr.mr_type    = PACKET_MR_PROMISC;
    if (setsockopt(sd, SOL_PACKET, PACKET_ADD_MEMBERSHIP,
                &mr, sizeof(mr)) == -1) {
        PRN_MSG("setsockopt MR_PROMISC fail, err:%d",errno);
        return -1;
    }

    PRN_MSG("setsockopt MR_PROMISC succ");
    return 0;
}

static void iface_unpromisc(int sd,int ifindex)
{
    struct ifreq	ifr;
    memset(&ifr, 0, sizeof(ifr));

    if( !if_indextoname(ifindex,ifr.ifr_name) ){
        PRN_MSG("conv index to name fail:%d\n",errno);
        return ;
    }

    /*
     * We put the interface into promiscuous mode;
     * take it out of promiscuous mode.
     *
     * XXX - if somebody else wants it in promiscuous
     * mode, this code cannot know that, so it'll take
     * it out of promiscuous mode.  That's not fixable
     * in 2.0[.x] kernels.
     */
    if (ioctl(sd , SIOCGIFFLAGS, &ifr) == -1) {
        PRN_MSG("ioctl %d:%s SIOCGIFFLAGS fail, err:%d",ifindex,ifr.ifr_name,errno);
    } else {
        if (ifr.ifr_flags & IFF_PROMISC) {
            /*
             * Promiscuous mode is currently on;
             * turn it off.
             */
            ifr.ifr_flags &= ~IFF_PROMISC;
            if (ioctl(sd , SIOCSIFFLAGS,&ifr) == -1) {
                PRN_MSG("ioctl SIOCSIFFLAGS fail, err:%d",errno);
            }
        }
    }
}





/*-----------------------------------------------------------------------------
 *  
 *  mmap��ʽ�������
 *  
 -----------------------------------------------------------------------------*/


static void mmap_release_hdr(struct mmaprcv_ctl *ptCtl)
{
    ptCtl->curhdr->tp_len   = 0;
    ptCtl->curhdr->tp_status= TP_STATUS_KERNEL;
    ptCtl->curhdr           = (struct tpacket_hdr  *)((char *)ptCtl->curhdr + PER_PACKET_SIZE);

    if ( ++ ptCtl->index >= ptCtl->qnum ) {
        ptCtl->index        = 0;
        ptCtl->curhdr       = (struct tpacket_hdr  *)ptCtl->buf;
    }
}

static int mmap_postread_callback(
        struct SniffDevCtl  *ptCtl)
{
    struct ethdev_ctl   *ptEthCtl   = (struct ethdev_ctl *)ptCtl->priv;
    mmap_release_hdr(&ptEthCtl->mmapinfo);
    return 0;
}

static int mmap_read_callback(
        struct SniffDevCtl  *ptCtl)
{
    struct ethdev_ctl   *ptEthCtl   = (struct ethdev_ctl *)ptCtl->priv;
    struct mmaprcv_ctl  *pmmap      = &ptEthCtl->mmapinfo;

    do{
        pmmap->curhdr = (struct tpacket_hdr*)(
                pmmap->buf + pmmap->index*PER_PACKET_SIZE);

        /*  ����ں�û�յ����ģ���ʹ�� poll �ȴ�    */
        if( pmmap->curhdr->tp_status ==TP_STATUS_KERNEL ){
            struct pollfd       pfd;
            pfd.fd              = ptEthCtl->sd;
            //pfd.events          = POLLIN | POLLRDNORM | POLLERR;
            pfd.events          = POLLIN;
            pfd.revents         = 0;

            switch (poll(&pfd, 1, -1)){
                case -1:
                    return -1;
                default:
                    break;
            }

            DBG_ECHO(">>>>>\tpoll %d index :%d\n",ptEthCtl->sd,pmmap->index);

            /*  �����յ��Ļ�����    */
            while( pmmap->curhdr->tp_status ==TP_STATUS_KERNEL ){
                pmmap->curhdr->tp_len   = 0;
                if ( ++ pmmap->index >= pmmap->qnum )
                    pmmap->index = 0;
                pmmap->curhdr = (struct tpacket_hdr*)(pmmap->buf + pmmap->index*PER_PACKET_SIZE);
            }
        }

        if (pmmap->curhdr->tp_len>= 16 && pmmap->curhdr->tp_snaplen <= ETH_MTU_MAX)
            break;

        DBG_ECHO("parse index :%d stat:%x sz:%d:%d err\n",
                pmmap->index,pmmap->curhdr->tp_status,pmmap->curhdr->tp_len,pmmap->curhdr->tp_snaplen);
        mmap_release_hdr(pmmap);
    }while(1);

    /*  ���ö����Ľ��
     *  ptCtl->tRcvFrame.tsָ��pmmap->ts, ����ֻҪ���º��ߵ�ֵ
     */
    pmmap->ts.tv_sec      = pmmap->curhdr->tp_sec;
    pmmap->ts.tv_usec     = pmmap->curhdr->tp_usec;

    ptCtl->tEthFrame.heth = (struct ethhdr*)((unsigned char*)pmmap->curhdr+pmmap->curhdr->tp_mac);

    return pmmap->curhdr->tp_snaplen;
}











/*-----------------------------------------------------------------------------
 *  
 *  ԭʼ��socket��ʽ�������
 *  
 -----------------------------------------------------------------------------*/
static int normal_read_callback(
        struct SniffDevCtl  *ptCtl)
{
    struct ethdev_ctl       *ptEthDev   = (struct ethdev_ctl *)ptCtl->priv;
    int     packet_len                  = -1;

    /*  ����һ���Ϸ��ı��� */
    do{
        struct sockaddr_ll  from;
        socklen_t		fromlen     = sizeof(from);
        packet_len  = recvfrom(ptEthDev->sd, 
                ptEthDev->normalinfo.buf,sizeof(ptEthDev->normalinfo.buf), MSG_TRUNC,
                (struct sockaddr *) &from, &fromlen);

        DBG_ECHO("recv pkg len%d err:%d\n",packet_len,errno);

        if( packet_len == -1 ){
            /*  ��������ϵ�����״̬�������ź��жϡ�����ͣ�õ�      */
            if( (errno != EINTR) && (errno != ENETDOWN) && (errno != EAGAIN) ){
                PRN_MSG("recvfrom errno:%d",errno);
                return -1;
            }

            if( errno == ENETDOWN )
                usleep(300);

            continue;
        }

        /*
         * Unfortunately, there is a window between socket() and
         * bind() where the kernel may queue packets from any
         * interface.  If we're bound to a particular interface,
         * discard packets not from that interface.
         *
         * (If socket filters are supported, we could do the
         * same thing we do when changing the filter; however,
         * that won't handle packet sockets without socket
         * filter support, and it's a bit more complicated.
         * It would save some instructions per packet, however.)
         */
        if (ptEthDev->ifindex != -1 &&
                from.sll_ifindex != ptEthDev->ifindex){
            packet_len  = -1;
            continue;
        }

        /*
         * Do checks based on packet direction.
         * We can only do this if we're using PF_PACKET; the
         * address returned for SOCK_PACKET is a "sockaddr_pkt"
         * which lacks the relevant packet type information.
         */
        if (from.sll_pkttype == PACKET_OUTGOING) {
            /*
             * Outgoing packet.
             * If this is from the loopback device, reject it;
             * we'll see the packet as an incoming packet as well,
             * and we don't want to see it twice.
             */
            if (from.sll_ifindex == ptEthDev->lo_ifindex) {
                packet_len  = -1;
                continue;
            }
        }

        /*  ��ȡ�����յ���ʱ��  */
        if (ioctl(ptEthDev->sd, SIOCGSTAMP, &ptEthDev->normalinfo.ts) == -1) {
            PRN_MSG("rcv pkg time info fail:%d\n",errno);
            packet_len  = -1;
        } 
    }while( packet_len == -1 );

    /*  ����,����ptCtl.tRcvFrame �Ѿ��� init�й̶�ָ��buf,���治�ٸı�    */
    return packet_len;
}


static int ethdev_release(struct SniffDevCtl *ptCtl)
{
    if( ptCtl->priv ){
        struct ethdev_ctl   *ptEthCtl   = (struct ethdev_ctl *)ptCtl->priv;
        if( ptEthCtl->sd != -1 ){
            if( ptEthCtl->promisc )
                iface_unpromisc(ptEthCtl->sd,ptEthCtl->ifindex);

            if( ptCtl->readframe == mmap_read_callback )
                iface_munmap(ptEthCtl->sd,ptEthCtl->mmapinfo.qnum,ptEthCtl->mmapinfo.buf);

            close(ptEthCtl->sd );
            ptCtl->priv     = NULL;
        }
    }

    return 0;
}

/** 
 * @brief ��ʼ������ץ���豸
 * 
 * @param ptCtl     [out]   �Ѿ��ɹ���ʼ�����豸�����ʧ����ֵ������
 * @param devname   [in]    ����ʼ����������
 * @param promisc   [in]    �Ƿ�ʹ�û���ģʽ(����ץ�����ϵ�����IP�İ�)
 * @param mmapqnum  [in]    ���õ� mmap��ʽץ����С����� = 0 ��ʾ��ʹ��mmap��ʽ
 * @param frametype [in]    ������ֻץ�ض���֡����
 * @param filter    [in]    ������,�ɳ����� socket ������ BPF ���˹������������
 * 
 * @return 
 */
int EthCapDev_Init(struct SniffDevCtl *ptCtl,const char *devname,
        int promisc,int mmapqnum,
        unsigned short frametype)
{
    int                 ret;
    char                *mmapbuf    = NULL;
    struct ethdev_ctl   tEthCtl;
    struct ethdev_ctl   *ptEthCtl   = NULL;

    if( !ptCtl || !devname || !*devname )
        return ERRCODE_SNIFF_PARAMERR;

    memset(&tEthCtl,0,sizeof(tEthCtl));
    tEthCtl.sd     = socket(PF_PACKET, SOCK_RAW, htons(frametype));

    if( tEthCtl.sd == -1 )
        return ERRCODE_SNIFF_OPENSD;

    /*
     *      ��ȡ����ID
     */
    tEthCtl.lo_ifindex  = iface_get_id(tEthCtl.sd,"lo");
    tEthCtl.ifindex     = iface_get_id(tEthCtl.sd,devname);
    if( 0 == tEthCtl.ifindex ) {
        close(tEthCtl.sd);
        return ERRCODE_SNIFF_OPENSD;
    }

    /*  
     *      ������
     */
    if ( -1 ==iface_bind(tEthCtl.sd, tEthCtl.ifindex) ) {
        close(tEthCtl.sd);
        return ERRCODE_SNIFF_OPENSD;
    }

    /*      ���û���ģʽ
    */
    if( promisc && !iface_promisc(tEthCtl.sd,tEthCtl.ifindex)) {
        tEthCtl.promisc   = 1;
    }

    /*      ����BPF������
    */
    ret = SFilter_setBPF(tEthCtl.sd,frametype);
    if( ret != 0 ){
        PRN_MSG("warn:  set BPF to socket fail:%d:%d\n",ret,errno);
    }


    /*  ����������ɹ��ˣ���������Ϣ���浽��ε�˽��������
    */
    ptEthCtl            = (struct ethdev_ctl*)malloc(sizeof(struct ethdev_ctl));
    if( !ptEthCtl ){
        close(tEthCtl.sd);
        return ERRCODE_SNIFF_NOMEM;
    }

    memcpy(ptEthCtl,&tEthCtl,sizeof(*ptEthCtl));
    ptCtl->priv                     = ptEthCtl;

    /*  ����mmap��ʽ  */
    mmapbuf             = mmapqnum > 0 ? iface_mmap(tEthCtl.sd,mmapqnum) : NULL;
    if( mmapbuf ){
        PRN_MSG("capture mode: mmap qlen:%d\n",mmapqnum);
        ptEthCtl->mmapinfo.buf      = mmapbuf;
        ptEthCtl->mmapinfo.qnum     = mmapqnum;
        ptEthCtl->mmapinfo.index    = 0;

        ptEthCtl->mmapinfo.curhdr   = (struct tpacket_hdr  *)ptEthCtl->mmapinfo.buf;

        ptCtl->tEthFrame.ts          = &ptEthCtl->mmapinfo.ts;

        ptCtl->readframe            = mmap_read_callback;
        ptCtl->postread             = mmap_postread_callback;
    }
    else {
        PRN_MSG("capture mode: normal\n");

        ptCtl->readframe            = normal_read_callback;

        ptCtl->tEthFrame.heth      = (struct ethhdr*)ptEthCtl->normalinfo.buf;
        ptCtl->tEthFrame.ts       = &ptEthCtl->normalinfo.ts;
    }

    ptCtl->release              = ethdev_release;

    return 0;
}


