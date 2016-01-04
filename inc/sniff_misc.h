
#ifndef SNIFF_MISC_H_
#define SNIFF_MISC_H_


//  ���Դ�ӡ����
#define PRN_MSG(fmt,arg...)     printf("%s:%d " fmt "\n",__func__,__LINE__,##arg)



#define PER_PACKET_SIZE             (2048)  //  pcap �ļ���ÿ����������С
#define ETH_MTU_MAX                 (1800)  //  ��Ϊ�����Ϸ��� MTU ��С,�ݶ�Ϊ����1540

/*  pcap�ļ������ӿ�
 */
FILE* SniffPcap_Init(const char* fname,int isread);
void SniffPcap_Close(FILE *fp);
void SniffPcap_Write(FILE *fp,struct timeval *ts,const unsigned char* data,int len);
int SniffPcap_Read(FILE *fp,struct timeval *ts,unsigned char* data,int len);


/*
 *  ������ģ������ӿ�
 */

struct SFilterCtl;

/*  ���ع�����֧�ֵ�ѡ����һ��ѡ���name = NULL
 *  ������ʹ�õ�option.val ��ΧΪ��'A'-'Z'
 */
const struct option*    SFilter_GetOptions();
/*  ��ʾѡ���Ӧ�İ���
 */
const char*             SFilter_GetHelp();

/*  ��ʼ����������Ϣ    */
struct  SFilterCtl * SFilter_Init(void);
/*  �ͷŹ�������Ϣ      */
void    SFilter_Release(struct SFilterCtl *filter);

/*  ����һ���������﷨  
 */
int     SFilter_Analyse(struct SFilterCtl *filter,char opcode,const char *optarg);
/*  ��Ч�������﷨, �ڷ����������﷨��Ӧ�õ��ô˺�������Ч
 */
int     SFilter_Validate(struct SFilterCtl *filter, unsigned short *pframetype);
/*  �жϱ����Ƿ�����ͨ��    */
int     SFilter_IsDeny(struct SFilterCtl *filter,int vlanok,
        const unsigned char *data,int len);
/*  ��socket��Ӧ��BPF����������ʵ���ں˹��˹��ܣ��������   */
int SFilter_setBPF(struct SFilterCtl *filter,int sd,int frametype);





/*  ���յ���֡��Ϣ
 */
struct RcvFrameInfo{
    struct timeval              *ts;        /*  ֡����ʱ��                  */
    char                        *buf;       /*  ���յ���֡����              */
};

/*
 *  �����հ�����ص�
 *  ���ԹҶ��������
 *  ������֡���ͣ�����0xffff��0��ֻ�ܹ�һ��������
 */

typedef void *PARSE_HANDLE;
typedef int (*SNIFF_INIT_CALLBACK)(PARSE_HANDLE *handle);
typedef int (*SNIFF_PARSE_CALLBACK)(
            const struct RcvFrameInfo *rcvinfo, /*  ���յ���֡��Ϣ          */
            int                 framelen,       /*  ֡����                  */
            PARSE_HANDLE        handle);        /*  ������ʹ�õ�˽�о��  */
typedef int (*SNIFF_RELEASE_CALLBACK)(PARSE_HANDLE *handle);

struct FrameParseItem{
    unsigned    short           frametype;  /*  ֡����,0��ʾ���������һ��,
                                                0xffff��ʾ��Ӧ����֡
                                                ������ʾ��Ӧ�ض�֡����
                                                */
    
    SNIFF_INIT_CALLBACK         init;       /*  �˴����ܵĳ�ʼ���ص�      */
    SNIFF_PARSE_CALLBACK        parse;      /*  �����֡���͵Ĵ���ص�      */
    SNIFF_RELEASE_CALLBACK      release;    /*  �˴����ܵ��ͷŻص�        */

    PARSE_HANDLE                priv;       /*  �ص�����ʹ�õ�˽��������    */
};


struct SniffDevCtl;
typedef int (*SNIFFDEV_READ_CALLBACK)(
            struct SniffDevCtl  *ptCtl);
typedef int (*SNIFFDEV_POSTREAD_CALLBACK)(
            struct SniffDevCtl  *ptCtl);
typedef int (*SNIFFDEV_RELEASE_CALLBACK)(
            struct SniffDevCtl  *ptCtl);

struct SniffDevCtl{
    struct SFilterCtl           *filter;    /*  ���������                  */
    SNIFFDEV_READ_CALLBACK      readframe;  /*  ��һ������֡                */
    SNIFFDEV_POSTREAD_CALLBACK  postread;   /*  ����������֡����������    */
    SNIFFDEV_RELEASE_CALLBACK   release;    /*  �ͷ�                        */

    struct RcvFrameInfo         rcvframe;   /*  ���յ���֡��readframe�и�ֵ   */

    void                        *priv;      /*  �ص�����ʹ�õ�˽������      */
};



/*  ץ����ʼ��,
 *  �����ʼ���ɹ�����filter�͹� handle ����
 *  */
int EthCapDev_Init(struct SniffDevCtl *ptCtl,const char *devname,
        int promisc,int mmapqnum,
        unsigned short frametype,struct SFilterCtl *filter);
int PCapDev_Init(struct SniffDevCtl *ptCtl,const char *capfilename);




struct SniffConf{
    int promisc;            /*  �Ƿ�ʹ�û���ģʽ            */
    int mmap_qlen;          /*  mmap��ʽ�հ�ʱmmap���д�С  */
    int vlanok;             /*  �Ƿ����VLAN��װ�ı���      */
};


/*  ����ץ������,num != -1 ʱֻץ num ����  */
int Sniff_LoadConf(struct SniffConf *ptConf,int argc, char ** argv)
{
}

int Sniff_Init(struct SniffDevCtl *ptCtl,const char *devname,struct SFilterCtl *filter);
int Sniff_Run(struct SniffDevCtl *ptCtl,int num,struct FrameParseItem *ptItems);

#endif


