
#include <unistd.h>
#include <stdint.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#include "sniff_error.h"
#include "sniff_conf.h"
#include "sniff.h"
#include "sniff_parser.h"


static struct SniffDevCtl  s_tDevCtl;

static struct option sniff_options[] = {
    //  name        has arg[0 no 1 must 2 opt]  returnval  returncode
    //  ����/�������
    {"i",            1, 0, SNIFF_OPCODE_DEVNAME},
    {"r",            1, 0, SNIFF_OPCODE_RDCAPFILE},
    {"w",            1, 0, SNIFF_OPCODE_WRCAPFILE},

    {"p",            0, 0, SNIFF_OPCODE_PROMISC},
    {"vlan",         0, 0, SNIFF_OPCODE_DECVLAN},

    {"f",            1, 0, SNIFF_OPCODE_MMAP_QLEN},
    {"c",            1, 0, SNIFF_OPCODE_CAPNUM},

    //  Э����˻���
    {"P",            1, 0, SNIFF_OPCODE_PROTO},
    {"F",            1, 0, SNIFF_OPCODE_FILTER},
    {"remote",       0, 0, SNIFF_OPCODE_REMOTE},
    {"bcast",        1, 0, SNIFF_OPCODE_BCAST},
    {"data",         1, 0, SNIFF_OPCODE_DATA},

    //  ��ʾ����
    {"alias",        1, 0, SNIFF_OPCODE_ALIAS},
    {"m",            1, 0, SNIFF_OPCODE_SHOWMATCH},
    {"M",            1, 0, SNIFF_OPCODE_SHOWNOMATCH},
    {"x",            0, 0, SNIFF_OPCODE_DECHEX},
    {"ttttt",        0, 0, SNIFF_OPCODE_RELATIMESTAMP},
    {"s",            0, 0, SNIFF_OPCODE_SILENT},
    {"eth",          0, 0, SNIFF_OPCODE_DECETH},

    {NULL,           0, 0,  0},
};


static void help(const char *appname)
{
    //  �������
    printf("%s : capture netcard's traffic and decode it, options include:\n",appname);
    printf("\tauth: mushuanli@163.com/lizlok@gmail.com\n");
    printf("\t-i     - ethname to sniff\n"
            "\t-r     - read sniff data from file\n"
            "\t-f     - set mmap buf frame count to fnum(def:200),0: disable mmap mode\n"
            "\t-p     - use promisc mode(default off)\n"
            "\t-c     - capture package count(default(0) not limit)\n");


    //  ���˹���
    printf("\n\t-P     - which protocol(default all) will be sniff?\n"
            "           protocol include: ARP,RARP,TCP,UDP,IGMP,DALL\n"
            "\t-F     - specialfy sniff filters(default allow all)\n"
            "\t         Filter syntax example:\n"
            "\t         ETH{SMAC=ff:fa:fb:fc:fd:fe,DMAC!ff:fa:fb:fc:fd:fe,DALL}\n"
            "\t         IP{SADDR=10.10.10.10,DADDR!10.20.30.40,DAND}\n"
            "\t         TCP{SPORT!80,DPORT=30,ALL}\n"
            "\t         UDP{SPORT!80,DPORT=30,AND}\n"
            "\t         'TCP{PORT=80,PORT=110}UDP{PORT=53}'\n"
            "\t         DALL allow only match (one condition) item\n"
            "\t         DAND deny match all condition item \n"
            "\t         ALL  only deny match (one condition) item\n"
            "\t         AND  only deny match (all condition) item\n"
            "\t-bcast - =[0|1|2] 0: only capture unicast, 1: only capture bcast, 2: capture all \n"
            "\t-data  - =[0|1|2] 0: only capture proto,   1: only capture data, 2: capture all\n"
            "\t-remote - capture remote control package(ignore TCP port 22/23/10000)\n"
            "special keyword: DALL - deny all except spedial, ! - except, = - allow\n"
            "NOTE: the filter param should use '' to quote,else it won't correct send\n");

    //  ��ʾ����
    printf("\n\t-m     - only show match record\n"
            "\t-M     - filter match record(don't show)\n"
            "\t-alias - ='name1=1.2.3.4,name2=5.6.7.8',show IP as alias\n"
            "\t-x     - use hex to decode unknown tcp frame\n"
            "\t-w     - write capture result to filename\n"
            "\t-s     - silient mode(don't decode package to screen)\n"
            "\t-ttttt - Print a delta (micro-second resolution) between current and first line on each dump line.\n"
            "\t-eth   - show ether head\n"
            "\t-vlan  - support decode vlan\n");

    return ;
}

/*  ����ץ������,num != -1 ʱֻץ num ����  */
static int ParseArgs(struct SniffConf *ptConf,int argc, char ** argv)
{
    int ret = 0;
    int c;
    opterr  = 0;

    memset(ptConf,0,sizeof(*ptConf));

    ptConf->ptFilter    = SFilter_Init();
    if( !ptConf->ptFilter ) {
        return ERRCODE_SNIFF_NOMEM;
    }

    ptConf->wMmapQLen      = DEFAULT_MMAP_QLEN;
    ptConf->wEthFrameType   = DEFAULT_ETH_FRAMETYPE;

    while ( ret == 0 && (c = getopt_long_only(argc, argv, "i:r:f:pc:P:F:m:M:w:s", sniff_options, NULL)) != -1 ) {  
        switch ( c ) {  
            case SNIFF_OPCODE_DEVNAME:
                strncpy(ptConf->strEthname,optarg,sizeof(ptConf->strEthname)-1);
                break;

            case SNIFF_OPCODE_RDCAPFILE:
                strncpy(ptConf->strCapFileRd,optarg,sizeof(ptConf->strCapFileRd)-1);
                break;

            case SNIFF_OPCODE_WRCAPFILE:
                strncpy(ptConf->strCapFileWr,optarg,sizeof(ptConf->strCapFileWr)-1);
                break;

            case SNIFF_OPCODE_PROMISC:
                ptConf->bPromisc    = TRUE;
                break;

            case SNIFF_OPCODE_MMAP_QLEN:
                ptConf->wMmapQLen  = strtoul(optarg,NULL,0);
                break;

            case SNIFF_OPCODE_CAPNUM:
                ptConf->dwCapNum   = strtoul(optarg,NULL,0);
                break;

            case SNIFF_OPCODE_SHOWMATCH:
                ptConf->ucShowmode = SNIFF_SHOWMODE_MATCH;
                memset(ptConf->strMatch,0,sizeof(ptConf->strMatch));
                strncpy(ptConf->strMatch,optarg,sizeof(ptConf->strMatch)-1);
                break;

            case SNIFF_OPCODE_SHOWNOMATCH:
                ptConf->ucShowmode = SNIFF_SHOWMODE_UNMATCH;
                memset(ptConf->strMatch,0,sizeof(ptConf->strMatch));
                strncpy(ptConf->strMatch,optarg,sizeof(ptConf->strMatch)-1);
                break;

            case SNIFF_OPCODE_ALIAS:
                memset(ptConf->strAlias,0,sizeof(ptConf->strAlias));
                strncpy(ptConf->strAlias,optarg,sizeof(ptConf->strAlias)-1);
                break;

            case SNIFF_OPCODE_RELATIMESTAMP:
                ptConf->ucRelateTimestamp   = 1;
                break;

            case SNIFF_OPCODE_DECHEX:
                ptConf->ucDecHex   = 1;
                break;

            case SNIFF_OPCODE_SILENT:
                ptConf->ucShowmode = SNIFF_SHOWMODE_SILENT;
                break;

            case SNIFF_OPCODE_DECETH:
                ptConf->bDecEth    = TRUE;
                break;

            case SNIFF_OPCODE_DECVLAN:
                ptConf->bVlanOk    = TRUE;
                break;

            case SNIFF_OPCODE_PROTO:
            case SNIFF_OPCODE_FILTER:
            case SNIFF_OPCODE_REMOTE:
            case SNIFF_OPCODE_BCAST:
            case SNIFF_OPCODE_DATA:
                ret = SFilter_Analyse(ptConf->ptFilter,c,optarg);
                if( ret != 0 ){
                    PRN_MSG("parse arg %c %s fail:%d, unsupport\n",c,optarg,ret);
                }
                break;

            default:
                ret = 1;
                help(argv[0]);
                break;
        }

    }


    if( ret != 0 ){
        goto end;
    }

    //  �������Ƿ������������
    //  ##  ����������
    if( !ptConf->strEthname[0] && !ptConf->strCapFileRd[0] ){
        PRN_MSG("no input device/file, please use -i [ethname] or -r capfilename to special program input, -h for help\n");
        ret = ERRCODE_SNIFF_PARAMERR;
        goto end;
    }
    if( ptConf->strEthname[0] && ptConf->strCapFileRd[0] ){
        PRN_MSG("duplicate input device/file, only can use -i [ethname] or -r capfilename to special one program input, -h for help\n");
        ret = ERRCODE_SNIFF_PARAMERR;
        goto end;
    }

    //  ##  ����������
    if( ptConf->ucShowmode == SNIFF_SHOWMODE_SILENT ) {
        if( !ptConf->strCapFileWr[0] ){
            PRN_MSG("no output device/file, please use -w [capfilename] to output to file or don't use -s , -h for help\n");
            ret = ERRCODE_SNIFF_PARAMERR;
            goto end;
        }

        ptConf->bDecEth = FALSE;
    }

    //  ##  �����˲���
    ret = SFilter_Validate(ptConf->ptFilter,&ptConf->wEthFrameType);
    if( ret != 0 ){
        PRN_MSG("no output device/file, please use -w [capfilename] to output to file or don't use -s , -h for help\n");
        ret = ERRCODE_SNIFF_PARAMERR;
        goto end;
    }

end:
    if( ret != 0 ) {
        SFilter_Release(ptConf->ptFilter);
    }

    return ret;
}




static void ReleaseClean()
{
    SniffParser_Release();

    if( s_tDevCtl.release ){
        s_tDevCtl.release(&s_tDevCtl);
        s_tDevCtl.release = NULL;
    }

    if( s_tDevCtl.ptFilter ){
        SFilter_Release( s_tDevCtl.ptFilter);
        s_tDevCtl.ptFilter    = NULL;
    }

}

void sig_handler( int sig)
{
    if(sig == SIGINT){
        ReleaseClean();
        exit(1);
    }
}


static int Init(struct SniffDevCtl *ptDev,const struct SniffConf *ptConf,struct SFilterCtl *ptFilter)
{
    int ret = 0;
    memset(ptDev,0,sizeof(*ptDev));
    ptDev->ptFilter     = ptFilter;


    //  �������豸
    if( ptConf->strEthname[0] ){
        ret = EthCapDev_Init(ptDev,ptConf->strEthname,
                ptConf->bPromisc,ptConf->wMmapQLen,
                ptConf->wEthFrameType);
    }
    else{
        ret = PCapDev_Init(ptDev,ptConf->strCapFileRd);
    }

    if( ret != 0 ){
        PRN_MSG("init input %s fail, ret:%d\n",ptConf->strEthname[0]? ptConf->strEthname : ptConf->strCapFileRd,ret);
        return ret;
    }

    //  ע������豸
    ret = SnifParser_Init(ptConf);
    if( ret != 0 ){
        PRN_MSG("init parser fail, ret:%d\n",ret);
    }
    return ret;
}


static int Run(struct SniffDevCtl *ptDev,const struct SniffConf *ptConf)
{
    int         ret     = 0;
    int         recvcnt = 0;
    do{
        int len = ptDev->readframe(ptDev);
        if( len < 0 ){
            ret = len;
            PRN_MSG("recv frame fail, ret:%d\n",ret);
            break;
        }
        else if( len == 0 ){
            break;
        }

        if( 0 == SFilter_IsDeny(
                    ptDev->ptFilter,ptConf->bVlanOk,
                    ptDev->tRcvFrame.buf,len) ){
            recvcnt ++;
            SnifParser_Exec(ptDev->tRcvFrame.ts,ptDev->tRcvFrame.buf,len);
        }

        if( ptDev->postread )
            ret = ptDev->postread(ptDev);

    }while( ret == 0 && (ptConf->dwCapNum == 0 || recvcnt != ptConf->dwCapNum) );

    PRN_MSG("recv %d package, stop for err:%d\n",recvcnt,ret);
    return ret;
}

int main(int argc, char **argv)
{
    struct SniffConf tConf;
    int ret;

    printf("\tdeveloper: mushuanli@163.com|lizlok@gmail.com %s pid:%d\n",argv[0],getpid());
    
    memset(&tConf,0,sizeof(tConf));

    ret = ParseArgs(&tConf,argc,argv);
    if( ret != 0 ){
        return 1;
    }

    //  ע������������
    atexit(ReleaseClean);
    signal(SIGINT, sig_handler);

    ret = Init(&s_tDevCtl,&tConf,tConf.ptFilter);
    if( ret != 0 ){
        return 1;
    }

    ret = Run(&s_tDevCtl,&tConf);
    return ret;
}

