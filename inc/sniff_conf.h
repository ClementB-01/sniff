/*
 * =====================================================================================
 *
 *       Filename:  sniff_conf.h
 *
 *    Description:  SNIFF ���ò���
 *
 * =====================================================================================
 */
#ifndef SNIFF_CONF_H_
#define SNIFF_CONF_H_

#include <arpa/inet.h>

//  getoptѡ����
#define SNIFF_OPCODE_DEVNAME    'i'
#define SNIFF_OPCODE_RDCAPFILE  'r'
#define SNIFF_OPCODE_WRCAPFILE  'w'
#define SNIFF_OPCODE_PROMISC    'p'

#define SNIFF_OPCODE_MMAP_QLEN  'f'
#define SNIFF_OPCODE_CAPNUM     'c'

#define SNIFF_OPCODE_SHOWMATCH  'm'
#define SNIFF_OPCODE_SHOWNOMATCH 'M'
#define SNIFF_OPCODE_HEX        'x'
#define SNIFF_OPCODE_HEXALL     'X'
#define SNIFF_OPCODE_RELATIMESTAMP  't'
#define SNIFF_OPCODE_SILENT     's'
#define SNIFF_OPCODE_DECETH     '0'
#define SNIFF_OPCODE_DECVLAN    '1'

#define SNIFF_OPCODE_PROTO      'P'
#define SNIFF_OPCODE_FILTER     'F'
#define SNIFF_OPCODE_ALIAS      'A'

#define SNIFF_OPCODE_REMOTE     '2'
#define SNIFF_OPCODE_BCAST      '3'
#define SNIFF_OPCODE_DATA       '4'



#define SNIFF_SHOWMODE_MATCH    0
#define SNIFF_SHOWMODE_UNMATCH  1
#define SNIFF_SHOWMODE_SILENT   2
#define SNIFF_MATCH_MAX         64

#define SNIFF_HEX_UNKNOWNPKG    1
#define SNIFF_HEX_ALLPKG        2

struct SniffConf{
    char    strEthname[32];             //  ������
    char    strCapFileRd[256];          //  �����ʹ������,���ĸ��ļ�������
    char    strCapFileWr[256];          //  ץ������ļ���
    char    strAlias[256];              //  IP�������ƣ�����Ϊname=x.y.z.a,name2=c.d.e.f

    uint32_t        dwCapNum;           //  ץ����,0��ʾ����
    uint16_t        wEthFrameType;      //  Ĭ����̫��֡����,��ptFilter�õ�
    uint16_t        wMmapQLen;          //  mmap��ʽ�հ�ʱmmap���д�С, 0��ʾ��ʹ�� mmap��ʽ
    uint8_t         bPromisc;           //  �Ƿ�ʹ�û���ģʽ        
    uint8_t         bVlanOk;            //  �Ƿ����VLAN��װ�ı���

    uint8_t         ucRelateTimestamp;  //  ��ʾ��Ե�һ֡��ʱ��
    uint8_t         ucDecHex;           //  �Ƿ���ʮ��������ʾδ֪����,1: 16������ʾ����ʶ�ı���, 2: 16������ʾ���б���
    uint8_t         bDecEth;            //  �Ƿ���ʾ����ͷ��Ϣ
    uint8_t         ucShowmode;         //  ��ʾģʽ: 0 ��ʾƥ�� 1: ��ʾ��ƥ�� 2:����ʾ
    char            strMatch[SNIFF_MATCH_MAX];      //  ��ucShowmode = [0|1]ʱ,��Ӧ�Ĳ���
    struct SFilterCtl   *ptFilter;      //  Э�������
};


#define FILTER_MODE_DENY_OR          0
#define FILTER_MODE_ALLOW_OR         1
#define FILTER_MODE_DENY_AND         2
#define FILTER_MODE_ALLOW_AND        3
#define FILTER_MODE_IS_ALLOW(mode)   ((mode) == FILTER_MODE_ALLOW_OR || (mode) ==FILTER_MODE_ALLOW_AND)


enum EProtoNum{
    EIGMPProto,     /*  �ظ�֡����  */
    ETCPProto,
    EUDPProto,
    EIPProto,       /*  �����ǵ�һ��������֡������ʼ��������治������ͬ֡�����������  */
    EARPProto,
    ERARPProto,
    ESCTPProto,
    EOtherProto,
    ELastProto      /*  ���һ��Э��    */
};

union filter_item{
    unsigned char   mac[8];             /*  mac[0-2]=0��ʾ��mac��Ч */
    unsigned int    val;                /*  ֵΪ0��ʾ��ֵ��Ч       */
};

struct filter_ctl{
    int                     mode;    /*  �Ƿ�ܾ�����    */
    union filter_item      *excsrc;    /*  Դ�����б� */
    union filter_item      *excdst;    /*  Ŀ�������б� */
};

#define FILTER_MAX_ITEM     256




enum LimitType{
    FILTER_LIMITMODE_FALSE  = 0,
    FILTER_LIMITMODE_TRUE   = 1,
    FILTER_LIMITMODE_ALL    = 2
};

struct SFilterCtl {
    char                    protoallow[ELastProto]; /* �����Э�� */
    struct filter_ctl       mac;
    struct filter_ctl       ip;
    struct filter_ctl       tcp;
    struct filter_ctl       udp;

    /*  �������˿���            */
    int                     remote;       /*  �Ƿ����Զ�̿��Ʊ���(TCP 22/23/10000�˿�) */
    enum LimitType          bcastok;      /*  mac������ƣ�0:�����չ㲥���鲥,1ֻ���չ㲥���鲥,2:��������    */
    enum LimitType          dataok;       /*  Э�������ƣ�0:����������,1ֻ��������,2:��������    */
};


int Sniff_ParseArgs(struct SniffConf *ptConf,int argc, char ** argv);

static inline const char* ip2str(uint32_t ip,char *cache){
    struct in_addr  addr    = {htonl(ip)};
    inet_ntop(AF_INET,&addr,cache,16);
    return cache;
}

#endif

