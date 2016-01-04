#ifndef SNIFF_H_
#define SNIFF_H_

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

//  ���Դ�ӡ����
#define PRN_MSG(fmt,arg...)     printf("%s:%d " fmt "\n",__func__,__LINE__,##arg)
#define DBG_ECHO(fmt,arg...)    //  PRN_MSG(fmt,##arg)



#define PER_PACKET_SIZE             (2048)  //  pcap �ļ���ÿ����������С
#define ETH_MTU_MAX                 (1800)  //  ��Ϊ�����Ϸ��� MTU ��С,�ݶ�Ϊ����1540
#define DEFAULT_MMAP_QLEN           200     //  Ĭ�ϵ�mmap���г���
#define DEFAULT_ETH_FRAMETYPE       0xffff  //  Ĭ����̫��֡����

#ifndef TRUE
#define TRUE                    1
#define FALSE                   0
#endif


/*  ���յ���֡��Ϣ
 */
struct RcvFrameInfo{
    struct timeval              *ts;        /*  ֡����ʱ��                  */
    char                        *buf;       /*  ���յ���֡����              */
};



struct SniffDevCtl;
typedef int (*SNIFFDEV_READ_CALLBACK)(
            struct SniffDevCtl  *ptCtl);
typedef int (*SNIFFDEV_POSTREAD_CALLBACK)(
            struct SniffDevCtl  *ptCtl);
typedef int (*SNIFFDEV_RELEASE_CALLBACK)(
            struct SniffDevCtl  *ptCtl);

struct SniffDevCtl{
    struct SFilterCtl           *ptFilter;    /*  ���������                  */
    SNIFFDEV_READ_CALLBACK      readframe;  /*  ��һ������֡                */
    SNIFFDEV_POSTREAD_CALLBACK  postread;   /*  ����������֡����������    */
    SNIFFDEV_RELEASE_CALLBACK   release;    /*  �ͷ�                        */

    struct RcvFrameInfo         tRcvFrame;   /*  ���յ���֡��readframe�и�ֵ   */

    void                        *priv;      /*  �ص�����ʹ�õ�˽������      */
};


int PCapDev_Init(struct SniffDevCtl *ptCtl,const char *capfilename);
int PCapOutput_Init(const char *outfilename);

struct SFilterCtl * SFilter_Init(void);
void SFilter_Release(struct SFilterCtl *filter);
int SFilter_Analyse(struct SFilterCtl *filter,char opcode,const char *optarg);
int SFilter_Validate(struct SFilterCtl *filter, unsigned short *pframetype);
int SFilter_setBPF(const struct SFilterCtl *filter,int sd,int frametype);
int SFilter_IsAllowTcpIp(const struct SFilterCtl *filter);

int SFilter_IsDeny(const struct SFilterCtl *filter,int vlanok,
        const unsigned char *data,int len);

#endif

