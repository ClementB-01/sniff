
#define _GNU_SOURCE
#include <getopt.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/filter.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include "sniff_error.h"
#include "sniff.h"
#include "sniff_conf.h"





/** 
 * @brief ����Э�������Ϣ
 * 
 * @param filter    [out]   ��¼Э��֧�����
 * @param syntax    [in]    �������﷨
 */
static void protocol_settype(struct SFilterCtl *filter,const char* syntax)
{
    unsigned char   allow[ELastProto];
    int             allowcount      = 0;
    unsigned char   deny[ELastProto];    
    int             denycount       = 0;

    int             denyall         = -1;    /*  Ĭ�Ͼܾ�����  */


    char            cache[256]  = "";
    char            *token;
    char            *lasts      = NULL;

    int             i;

    if( !syntax || !*syntax )
        return ;

    memset(allow,0,sizeof(allow));
    memset(deny,0,sizeof(deny));

    /*  �﷨:
     *  TCP,UDP,ARP,RARP,SCTP,DALL
     */
    for( i = 0; i < sizeof(cache)-1 && *syntax ; i ++ ) {
        while(isspace(*syntax) )   syntax++;
        if( *syntax )
            cache[i]    = toupper(*syntax++);
    }

    token   = strtok_r(cache,",",&lasts);
    while( token ) {
        unsigned char   *proto  = allow;
        int             *pcount = &allowcount;
        if( *token == '!' ) {
            proto   = deny ;
            pcount  = &denycount;
            token   ++;
        }

        (*pcount) ++;

        if( !strcmp("TCP",token) ) {
            if( proto  == allow )   proto[ETCPProto]    = 1;
        }
        else if( !strcmp("UDP",token) ) {
            if( proto  == allow )   proto[EUDPProto]    = 1;
        }
        else if( !strcmp("IGMP",token) ) {
            if( proto  == allow )   proto[EIGMPProto]   = 1;
        }
        else if( !strcmp("IP",token) ) {
            proto[EIPProto]     = 1;
        }
        else if( !strcmp("ARP",token) ) {
            proto[EARPProto]    = 1;
        }
        else if( !strcmp("RARP",token) ) {
            proto[ERARPProto]    = 1;
        }
        else if( !strcmp("SCTP",token) ) {
            proto[ESCTPProto]    = 1;
        }
        else if( !strcmp("DALL",token) ) {
            denyall    = 1;
        }
        else if( !strcmp("ALL",token) ) {
            denyall    = 0;
        }
        else{
            (*pcount) --;
        }


        token   = strtok_r(NULL,",",&lasts);
    }

    if( allowcount == 0 ){
        /*  ���û��ָ������ͨ����
            �ǽ���������
            */
        denyall = 0;
    }
    else{
        if( denycount == 0 ){
            denyall = 1;
        }
    }

    if( denyall == -1 ){
        /*  
         *  ���ָ���˾ܾ�Э�飬Ҳָ���˳���Э��
         *  ����û��ָ��DALL����ALL��
         *  ��Ĭ��ΪDALL
         */
        denyall = 1;
    }


    if( denyall ) {
        memset(filter->protoallow,0,sizeof(char )*ELastProto);
        if( allow[EIPProto] )       filter->protoallow[EIPProto]    = 1;
        if( allow[ETCPProto] )      filter->protoallow[ETCPProto]   = 1;
        if( allow[EUDPProto] )      filter->protoallow[EUDPProto]   = 1;
        if( allow[EIGMPProto] )     filter->protoallow[EIGMPProto]  = 1;

        if( allow[EARPProto] )      filter->protoallow[EARPProto]   = 1;
        if( allow[ERARPProto] )     filter->protoallow[ERARPProto]  = 1;
        if( allow[ESCTPProto] )     filter->protoallow[ESCTPProto]  = 1;
        filter->protoallow[EOtherProto]     = 0;
    }
    else {
        memset(filter->protoallow,1,sizeof(char )*ELastProto);
        if( deny[EIPProto] )        filter->protoallow[EIPProto]    = 0;
        if( deny[ETCPProto] )      filter->protoallow[ETCPProto]   = 0;
        if( deny[EUDPProto] )      filter->protoallow[EUDPProto]   = 0;
        if( deny[EIGMPProto] )     filter->protoallow[EIGMPProto]  = 0;

        if( deny[EARPProto] )      filter->protoallow[EARPProto]   = 0;
        if( deny[ERARPProto] )     filter->protoallow[ERARPProto]  = 0;
        if( deny[ESCTPProto] )     filter->protoallow[ESCTPProto]  = 0;
        filter->protoallow[EOtherProto]     = 1;
    }
}





#define CORRECT_PROTO_FILTER(protoname,filtername)   do{     \
    if( filter->protoallow[protoname] \
            && !filter->filtername.excsrc && !filter->filtername.excdst   \
            && FILTER_MODE_IS_ALLOW(filter->filtername.mode )) {    \
        filter->protoallow[protoname]    = 0;   \
    }}while(0)

#define DUMP_MAC_FILTERITEMS(items,name)  do{    \
    if( items ){ union filter_item *pitem  = items;         \
        while( pitem->mac[0] && pitem->mac[1] && pitem->mac[2] ){                \
            printf("\t " name ":%02x:%02x:%02x:%02x:%02x:%02x\n",   \
                    pitem->mac[0] , pitem->mac[1] , pitem->mac[2],  \
                    pitem->mac[3] , pitem->mac[4] , pitem->mac[5]); pitem ++; \
        }        }  }while(0)

#define DUMP_IP_FILTERITEMS(items,name)  do{    \
    if( items ){    union filter_item  *pitem  = items;         \
        while( pitem->val ){                \
            char cache[16]  = "";   \
            printf("\t" name ":%s\n",ip2str(pitem->val,0,cache));   pitem ++;   \
        }        }  }while(0)

#define DUMP_INT_FILTERITEMS(items,name)  do{   \
    if( items ){    union filter_item  *pitem  = items;         \
        while( pitem->val ){                \
            printf("\t" name ":%d\n",pitem->val);   pitem ++;   \
        }        }  }while(0)

#define DUMP_HEX_FILTERITEMS(items,name)  do{   \
    if( items ){    union filter_item  *pitem  = items;         \
        while( pitem->val ){                \
            printf("\t" name ":%x\n",pitem->val);   pitem ++;   \
        }        }  }while(0)

const char *filtermode2str(int mode){
    switch(mode){
        case FILTER_MODE_DENY_OR:       return "DENY_OR";
        case FILTER_MODE_DENY_AND:      return "DENY_AND";
        case FILTER_MODE_ALLOW_OR:      return "ALLOW_OR";
        case FILTER_MODE_ALLOW_AND:     return "ALLOW_AND";
        default:                        return "UNKNOWN";
    }
}

static int check_filter_conf(struct SFilterCtl *filter)
{
    int i   = 0;

    /*
     *  ���� -P ��-F��������֧�ֵ�Э��
     */
    CORRECT_PROTO_FILTER(ETCPProto,tcp);
    CORRECT_PROTO_FILTER(EUDPProto,udp);
    CORRECT_PROTO_FILTER(EIPProto,ip);

    for( i = 0; i < ELastProto && !filter->protoallow[i]; i ++ ) 
        ;

    if( i == ELastProto ){
        PRN_MSG("error: no proto allow,exit\n");
        return ERRCODE_SNIFF_PARAMERR;
    }

    if( ! filter->protoallow[ETCPProto] ){
        filter->remote  = 0;
    }

    printf("PROTOCOL: ALLOW  %s%s%s%s%s%s%s%s Remote:%d\n",
            filter->protoallow[EIPProto]?"IP ":"",
            filter->protoallow[ETCPProto]?"TCP ":"",
            filter->protoallow[EUDPProto]?"UDP ":"",
            filter->protoallow[EARPProto]?"ARP ":"",
            filter->protoallow[ERARPProto]?"RARP ":"",
            filter->protoallow[ESCTPProto]?"SCTP ":"",
            filter->protoallow[EIGMPProto]?"IGMP ":"",
            filter->protoallow[EOtherProto]?"OTHER ":"",
            filter->remote
          );

    if( !filter->mac.excsrc && !filter->mac.excdst ) {
        if( FILTER_MODE_IS_ALLOW(filter->mac.mode) ) {
            PRN_MSG("wrong filter syntax,skip\n");
            return ERRCODE_SNIFF_PARAMERR;
        }
    }
    else {
        printf("MAC FILTER: %s :\n",
                filtermode2str(filter->mac.mode));
        DUMP_MAC_FILTERITEMS(filter->mac.excsrc,"SRC MAC");
        DUMP_MAC_FILTERITEMS(filter->mac.excdst,"DST MAC");
    }   //  if( !filter->mac.excsrc && !filter->mac.excdst )


    if( (filter->protoallow[EIPProto] 
                || filter->protoallow[ETCPProto]
                || filter->protoallow[EUDPProto]
                || filter->protoallow[EIGMPProto]
        )
            && (filter->ip.excsrc || filter->ip.excdst) ) {

        printf("IP FILTER: %s :\n",
                filtermode2str(filter->ip.mode));
        DUMP_IP_FILTERITEMS(filter->ip.excsrc,"SRC IP");
        DUMP_IP_FILTERITEMS(filter->ip.excdst,"DST IP");
    }   //  end if( filter->protoallow[EIPProto] )

    if( filter->protoallow[ETCPProto] 
            && (filter->tcp.excsrc || filter->tcp.excdst) ) {
        printf("TCP FILTER: %s :\n",
                filtermode2str(filter->tcp.mode));
        DUMP_INT_FILTERITEMS(filter->tcp.excsrc,"SRC PORT");
        DUMP_INT_FILTERITEMS(filter->tcp.excdst,"DST PORT");
    }   //  end if( filter->protoallow[EIPProto] )

    if( filter->protoallow[EUDPProto] 
            && (filter->udp.excsrc || filter->udp.excdst) ) {
        printf("UDP FILTER: %s :\n",
                filtermode2str(filter->udp.mode));
        DUMP_INT_FILTERITEMS(filter->udp.excsrc,"SRC PORT");
        DUMP_INT_FILTERITEMS(filter->udp.excdst,"DST PORT");
    }   //  end if( filter->protoallow[EIPProto] )


    return ERRCODE_SNIFF_SUCCESS;
}


static inline int comp_mac(const union filter_item  *pitem,void *value)
{
    return memcmp(pitem->mac,(unsigned char *)value,6) ? -1:0;
}

static inline int comp_uint(const union filter_item  *pitem,void *value)
{
    return pitem->val == *(unsigned int *)value ? 0:-1;
}

/** 
 * @brief 
 * 
 * @param pitem 
 * @param value 
 * @param int(*comp 
 * @param pitem 
 * @param value 
 * @param  
 * 
 * @return 0:   �ҵ�ƥ����
 *          -1: û�ҵ�ƥ����
 */
static inline int find_match_item(
        const union filter_item  *pitem,
        void *value,
        int (*comp)(const union filter_item  *pitem,void *value)) 
{
    if( pitem ) {
        while( pitem->val ){
            if( !comp(pitem,value) )
                return 0;
            pitem ++;
        }
    }

    return -1;
}

static inline int is_filter(const struct filter_ctl *ctl,
        void *srcval,void *dstval,
        int (*comp)(const union filter_item  *pitem,void *value))
{
    int src = find_match_item(ctl->excsrc,srcval,comp );
    int dst = find_match_item(ctl->excdst,dstval,comp );
    switch( ctl->mode ){
        case FILTER_MODE_ALLOW_OR:
            return (!src) || (!dst) ? 0:1;
        case FILTER_MODE_ALLOW_AND:
            return (!src) && (!dst) ? 0:1;
        case FILTER_MODE_DENY_OR:
            return (!src) || (!dst) ? 1:0;
        case FILTER_MODE_DENY_AND:
            return (!src) && (!dst) ? 1:0;
        default:
            return 0;
    }
}

static int is_ip_filter(const struct SFilterCtl *filter,
        const unsigned char *data,int len)
{
    const struct iphdr      *piphdr     = (struct iphdr    *)(data);
    unsigned int            saddr       = htonl(piphdr->saddr);
    unsigned int            daddr       = htonl(piphdr->daddr);

    if( !filter->protoallow[EIPProto] 
            && !filter->protoallow[ETCPProto] 
            && !filter->protoallow[EUDPProto] 
            && !filter->protoallow[EIGMPProto] ) {
        DBG_ECHO("ignore by proto\n");
        return 1;
    }

    if( is_filter(&filter->ip,&saddr,&daddr,comp_uint) ){
        DBG_ECHO("ignore by ip %x %x\n",saddr,daddr);
        return 1;
    }

    data += 4 * piphdr->ihl;
    switch( piphdr->protocol ) {
        case IPPROTO_TCP:
            if( !filter->protoallow[ETCPProto] && !filter->protoallow[EIPProto]) {
                DBG_ECHO("ignore tcp proto\n");
                return 1;
            }
            else{
                struct tcphdr   *ptcphdr    = (struct tcphdr*)(data);
                unsigned int    sport       = htons(ptcphdr->source); 
                unsigned int    dport       = htons(ptcphdr->dest); 

                if( is_filter(&filter->tcp,&sport,&dport,comp_uint) ){
                    DBG_ECHO("ignore tcp port %d->%d \n",sport,dport);
                    return 1;
                }

                if( !filter->remote ){
                    if( sport == 22 || sport == 23 
                            || dport == 22 || dport == 23 ){
                        DBG_ECHO("ignore tcp remote port %d->%d \n",sport,dport);
                        return 1;
                    }
                }
            }
            break;

        case IPPROTO_UDP:
            if( !filter->protoallow[EUDPProto] && !filter->protoallow[EIPProto]) {
                DBG_ECHO("ignore udp proto\n");
                return 1;
            }
            else{
                struct  udphdr   *pudphdr   = (struct udphdr *)(data);
                unsigned int    sport       = htons(pudphdr->source); 
                unsigned int    dport       = htons(pudphdr->dest); 
                if( is_filter(&filter->udp,&sport,&dport,comp_uint) ){
                    DBG_ECHO("ignore udp port %d->%d \n",sport,dport);
                    return 1;
                }
            }
            break;

        case IPPROTO_IGMP:
            if( !filter->protoallow[EIGMPProto] && !filter->protoallow[EIPProto] ) {
                DBG_ECHO("ignore igmp package\n");
                return 1;
            }
            break;

        default:
            break;
    }

    return 0;
}






/*
 *  ������ͨ�����﷨
 */
           struct NormalAnalyseCtl{
               int                     count;
               union filter_item       items[FILTER_MAX_ITEM];
           };
#define COPY_TO_FILTER(filter,type,value)   do{ \
    if( value.count > 0 ){               \
        filter->type   = malloc(sizeof(filter->type[0])*(value.count+1));   \
        if( !filter->type ){    perror(" malloc");  return -1; }    \
        memcpy(filter->type,value.items,sizeof(filter->type[0])*(value.count)); \
        memset(&filter->type[value.count],0,sizeof(filter->type[0])); \
    }}while(0)

static inline void set_filter_value(struct NormalAnalyseCtl *pctl,
        const char *val,
        int (*value_callback)(union filter_item *out,const char *val))
{
    if( pctl && pctl->count < FILTER_MAX_ITEM ) {
        if( !value_callback(
                    &pctl->items[pctl->count],
                    val ) ){
            pctl->count ++;
        }
    }
}

static int analyse_normal_filter(struct filter_ctl *pfilter,
        char* param,const char *keyword,
        int (*value_callback)(union filter_item *out,const char *val))
{
    char    *lasts      = NULL;
    char    *token      = strtok_r(param,",",&lasts);
    int     keywordlen  = strlen(keyword);

    struct NormalAnalyseCtl           allowsrc;
    struct NormalAnalyseCtl           denysrc;
    struct NormalAnalyseCtl           allowdst;
    struct NormalAnalyseCtl           denydst;
    int     filtermode  = -1;

    memset(&allowsrc,0,sizeof(allowsrc));
    memset(&denysrc,0,sizeof(denysrc));
    memset(&allowdst,0,sizeof(allowdst));
    memset(&denydst,0,sizeof(denydst));

    while( token ) {
        if( !strcmp(token,"DALL") ) {
            filtermode    = FILTER_MODE_ALLOW_OR;
        }
        else if( !strcmp(token,"ALL") ) {
            filtermode    = FILTER_MODE_DENY_OR;
        }
        else if( !strcmp(token,"DAND") ) {
            filtermode    = FILTER_MODE_DENY_AND;
        }
        else if( !strcmp(token,"AND") ) {
            filtermode    = FILTER_MODE_ALLOW_AND;
        }
        else{
            int issrc       = -1;
            if( (token[0] == 'S' || token[0] == 'D') ){
                issrc       = token[0] == 'S' ? 1:0;
                token       ++;
            }
            if( !memcmp(token,keyword,keywordlen) ){
                int forbit  = -1;
                switch( token[keywordlen] ){
                    case '!':   forbit  = 1;    break;
                    case '=':   forbit  = 0;    break;
                    default:                    break;
                }

                if( forbit != -1 ){
                    switch( issrc ){
                        case -1:
                        case 0:
                            set_filter_value(
                                    forbit ? &denydst : &allowdst,
                                    token + 1 + keywordlen ,value_callback);
                            if( !issrc )
                                break;
                        case 1:
                            set_filter_value(
                                    forbit ? &denysrc : &allowsrc,
                                    token + 1 + keywordlen ,value_callback);
                        default:
                            break;
                    }
                }
            }
        }

        token  = strtok_r(NULL,",",&lasts);
    }

    if( allowsrc.count == 0 && allowdst.count == 0 ){
        /*
         *  û��ָ���ر�allow���,��Ϊ��allow����
         */
        if( filtermode != FILTER_MODE_DENY_AND )
            filtermode     = FILTER_MODE_DENY_OR;
    }
    else if( denysrc.count == 0 && denydst.count == 0 ){
        /*
         *  û��ָ���ر�deny���,��Ϊ��deny����
         */
        if( filtermode != FILTER_MODE_ALLOW_AND )
            filtermode     = FILTER_MODE_ALLOW_OR;
    }

    if( filtermode == -1 ){
        filtermode     = FILTER_MODE_ALLOW_OR;
    }

    if( FILTER_MODE_IS_ALLOW(filtermode) ) {
        COPY_TO_FILTER(pfilter,excsrc,allowsrc);
        COPY_TO_FILTER(pfilter,excdst,allowdst);
    }
    else {
        COPY_TO_FILTER(pfilter,excsrc,denysrc);
        COPY_TO_FILTER(pfilter,excdst,denydst);
    }

    pfilter->mode = filtermode;
    return 0;
}

static int str2hex(char val)
{
    if (val >= '0' && val <= '9')
        return val - '0';
    else if (val >= 'A' && val <= 'F')
        return val - 'A'+10;
    else if (val >= 'a' && val <= 'f')
        return val - 'a'+10;
    else
        return 0;
}

static int analyse_mac_item(union filter_item *out,const char *val)
{
    if( strlen(val) != 17 )
        return -1;

    out->mac[0] = (str2hex(val[0])<<4 ) | str2hex(val[1]);
    out->mac[1] = (str2hex(val[3])<<4 ) | str2hex(val[4]);
    out->mac[2] = (str2hex(val[6])<<4 ) | str2hex(val[7]);
    out->mac[3] = (str2hex(val[9])<<4 ) | str2hex(val[10]);
    out->mac[4] = (str2hex(val[12])<<4 ) | str2hex(val[13]);
    out->mac[5] = (str2hex(val[15])<<4 ) | str2hex(val[16]);
    return 0;
}

static int analyse_ip_item(union filter_item *out,const char *val)
{
    return (out->val = htonl(inet_addr(val))) ? 0:-1;
}

static int analyse_port_item(union filter_item *out,const char *val)
{
    return (out->val = atoi(val)) ? 0 :-1;
}

/*
 *  �﷨����,
 *  �﷨��ʽ
 *      ����{����=[!]ֵ,DALL}
 */
static int analyse_filter(
        struct SFilterCtl *filter,const char* token ,char* param)
{
    printf(" ANALYSE %s-%s|\n",token,param);

    if( !strcmp(token,"ETH") ) {
        return analyse_normal_filter(&filter->mac,param,"MAC",analyse_mac_item);
    }
    if( !strcmp(token,"IP") ) {
        return analyse_normal_filter(&filter->ip,param,"ADDR",analyse_ip_item);
    }
    if( !strcmp(token,"TCP") ) {
        return analyse_normal_filter(&filter->tcp,param,"PORT",analyse_port_item);
    }
    if( !strcmp(token,"UDP") ) {
        return analyse_normal_filter(&filter->udp,param,"PORT",analyse_port_item);
    }

    return 0;
}

static int proto_setdetail(struct SFilterCtl *filter,const char* syntax)
{
    char    *pstr;
    char    *token;
    char    *param;
    char    *lasts;

    int     i   = 0;
    int     len;
    if( !syntax || !*syntax )
        return ERRCODE_SNIFF_SUCCESS;

    PRN_MSG("FILTER PARAM is:%s, if incorrect, use '' to quote the param\n",syntax);

    len         = strlen(syntax);
    pstr        = malloc(len+1);
    if( !pstr ) {
        perror("filter_setfilter malloc fail");
        return ERRCODE_SNIFF_NOMEM;
    }

    /*  �﷨:
     *  TCP,UDP,ARP,RARP,SCTP,DALL
     */
    for( i = 0; i < len && *syntax ; i ++ ) {
        while(isspace(*syntax) )   syntax++;
        if( *syntax )
            pstr[i]    = toupper(*syntax++);
    }

    token   = strtok_r(pstr,"{",&lasts);
    if( !token ) {
        free(pstr);
        return ERRCODE_SNIFF_SUCCESS;
    }

    while( token ) {
        param   = strtok_r(NULL,"}",&lasts);
        if( !param ) {
            break;
        }

        analyse_filter(filter,token,param);
        token   = strtok_r(NULL,"{",&lasts);
    }

    free(pstr);

    return 0;
}


#if 0
/*
 *      ����mac�����﷨
 */
        struct MacAnalyseCtl{
            int                     count;
            struct mac_filter_item  items[FILTER_MAX_ITEM];
        };


static int analyse_eth_filter(struct mac_filter *mac_filter,char* param)
{
    char    *lasts;
    char    *token  = strtok_r(param,",",&lasts);

    MacAnalyseCtl           allowsrc;
    MacAnalyseCtl           denysrc;
    MacAnalyseCtl           allowdst;
    MacAnalyseCtl           denydst;
    int                     denyall     = 0;

    memset(&allowsrc,0,sizeof(allowsrc));
    memset(&denysrc,0,sizeof(denysrc));
    memset(&allowdst,0,sizeof(allowdst));
    memset(&denydst,0,sizeof(denydst));

    while( token ) {
        struct MacAnalyseCtl    *pctl   = NULL;
        int     issrc   = -1;

        if( !memcmp(token,"SMAC",4) ) {
            issrc       = 1;
            pctl        = allow ? allowsrc : denysrc;
        }
        else if( !memcmp(token,"DMAC",4) ) {
            issrc       = 0;
            pctl        = allow ? allowdst : denydst;
        }
        else if( !strcmp(token,"DALL") ) {
            denayall    = 1;
            issrc       = 2;
        }

        if( issrc == 0 || issrc ==1 ){
            switch( token[4] ) {
                case '!':   pctl    = issrc ? denysrc: denydst; break;
                case '=':   pctl    = issrc ? allowsrc: allowsrc; break;
                default:    break;
            }
        }

        if( pctl && pctl->count < FILTER_MAX_ITEM ) {
            unsigned char   *mac    = pctl->items[pctl->count].mac;
            if( 6 == sscanf(token+5,"%X:%X:%X:%X:%X:%X",
                        pctl->mac[0],pctl->mac[1],pctl->mac[2],
                        pctl->mac[3],pctl->mac[4],pctl->mac[5]) ){
                pctl->count ++;
            }
        }

        token  = strtok_r(NULL,",",&lasts);
    }

    if( denyall ) {
        COPY_TO_FILTER(mac_filter,excsrc,allowsrc);
        COPY_TO_FILTER(mac_filter,excdst,allowdst);
    }
    else {
        COPY_TO_FILTER(mac_filter,excsrc,denysrc);
        COPY_TO_FILTER(mac_filter,excdst,denydst);
    }
    mac_filter->denyall = denyall;
    return 0;
}

filter->protoallow
#endif





static int get_filer_frametype(struct SFilterCtl *filter)
{
    if( filter->protoallow[EOtherProto] )
        return ETH_P_ALL;

    if( filter->protoallow[EIPProto] 
            + filter->protoallow[ESCTPProto]
            + filter->protoallow[EARPProto] 
            + filter->protoallow[ERARPProto] != 1 )
    {

        return ETH_P_ALL;
    }

    if( filter->protoallow[EIPProto] )
        return ETH_P_IP;
    if( filter->protoallow[EARPProto] )
        return ETH_P_ARP;
    if( filter->protoallow[ERARPProto] )
        return ETH_P_RARP;

    return ETH_P_ALL;
}









struct SFilterCtl * SFilter_Init(void)
{
    struct SFilterCtl *filter    = malloc(sizeof(*filter));
    if( filter ){
        memset(filter,0,sizeof(*filter));
        memset(filter->protoallow,1,sizeof(filter->protoallow));
        filter->bcastok       = FILTER_LIMITMODE_ALL;
        filter->dataok        = FILTER_LIMITMODE_ALL;
        return filter;
    }
    else{
        perror("init filter ,malloc fail");
        return NULL;
    }
}

#define FREE_FILTER_ITEM(item)  do{             \
    free(item.excsrc); item.excsrc    = NULL;   \
    free(item.excdst); item.excdst    = NULL;   \
    item.mode  = FILTER_MODE_DENY_OR;           \
}while(0)

void SFilter_Release(struct SFilterCtl *filter)
{
    if( filter ) {
        memset(filter->protoallow,0,sizeof(filter->protoallow));
        FREE_FILTER_ITEM(filter->mac);
        FREE_FILTER_ITEM(filter->ip);
        FREE_FILTER_ITEM(filter->tcp);
        FREE_FILTER_ITEM(filter->udp);
        free(filter);
    }
}

/** 
 * @brief ����������Ϣ
 * 
 * @param filter    [out]   ����������
 * @param opcode    [in]    ��������Ӧ���ڲ�����, ��ӦSFilter_GetOptions����ֵ��option.val
 * @param optarg    [in]    ����ֵ
 * 
 * @return  0:  �ɹ�
 *          -1: ��������
 *          -2: ����ʶ�Ĳ���
 */
int     SFilter_Analyse(struct SFilterCtl *filter,char opcode,const char *optarg)
{
    int val;
    switch( opcode ) {
        case    SNIFF_OPCODE_PROTO:     //  Э������
            protocol_settype(filter,optarg);
            return 0;

        case    SNIFF_OPCODE_FILTER:    //  Э���еľ�����˲���
            if( proto_setdetail(filter,optarg) ){
                PRN_MSG("wrong filter syntax\n");
                return -1;
            }
            return 0;

        case    SNIFF_OPCODE_REMOTE:
            filter->remote        = strtoul(optarg,0,0);
            PRN_MSG("remote: %d\n",filter->remote);
            return 0;
            break;

        case    SNIFF_OPCODE_BCAST:
            val     = strtoul(optarg,0,0);
            if( val >= FILTER_LIMITMODE_FALSE && val <= FILTER_LIMITMODE_ALL ){
                filter->bcastok   = val;
                PRN_MSG("bcastok: %d\n",val);
            }
            else{
                return -1;
            }
            return 0;
            break;

        case SNIFF_OPCODE_DATA:
            val     = strtoul(optarg,0,0);
            if( val >= FILTER_LIMITMODE_FALSE && val <= FILTER_LIMITMODE_ALL ){
                filter->dataok   = val;
                PRN_MSG("dataok: %d\n",val);
            }
            else{
                return -1;
            }
            return 0;
            break;

        default:
            break;
    }

    PRN_MSG("opt:%c %s unknown\n",opcode,optarg);
    return -2;
}

int     SFilter_Validate(struct SFilterCtl *filter, unsigned short *pframetype)
{
    int ret;

    if( !filter )
        return -1;

    if( pframetype  )   
        *pframetype = 0xffff;

    ret          = check_filter_conf(filter);
    if( ret != 0 )
        return ret;

    if( pframetype  )   
        *pframetype = get_filer_frametype(filter);

    return 0;
}

int SFilter_IsAllowTcpIp(const struct SFilterCtl *filter)
{
    if( filter->protoallow[EIGMPProto]
            + filter->protoallow[ETCPProto]
            + filter->protoallow[EUDPProto]
            + filter->protoallow[EIPProto]
            + filter->protoallow[EARPProto]
            + filter->protoallow[ERARPProto] != 0 )
        return 1;

    return 0;

}

/** 
 * @brief �ж�һ�������Ƿ�ᱻ����
 * 
 * @param filter    [in]    ���������
 * @param data      [in]    ���յ�����̫��֡
 * @param len       [in]    ��̫��֡����
 * 
 * @return  0:  ������
 *          1:  ����
 */
int SFilter_IsDeny(const struct SFilterCtl *filter,unsigned int vlanok,
        const unsigned char *data,int len)
{
    int                     ret = 0;
    struct ethhdr*  heth    = (struct ethhdr*)data;
    int             proto   = htons(heth->h_proto);

    if( !filter )
        return 0;

    data                    += ETH_HLEN;
    len                     -= ETH_HLEN;
    if( filter->bcastok != FILTER_LIMITMODE_ALL ){
        int isbc        = (memcmp(heth->h_dest,"\xff\xff\xff\xff\xff\xff",6) == 0 )
            || (memcmp(heth->h_dest,"\x1\x0\x5e",3) == 0) ;
        if( (isbc && ( filter->bcastok == FILTER_LIMITMODE_FALSE ))
                || ((!isbc) && ( filter->bcastok == FILTER_LIMITMODE_TRUE ) ) ){
            DBG_ECHO("ignore broadcast\n");
            return 1;
        }
    }

    if( filter->mac.excsrc || filter->mac.excdst ){
        if( is_filter(&filter->mac,heth->h_source,heth->h_dest,comp_mac) ){
            DBG_ECHO("ignore special mac\n");
            return 1;
        }
    }

    if( vlanok && proto == ETH_P_8021Q ){
        proto       = htons(*(unsigned short *)(data+2));
        data        += 4;
        len         -= 4;
    }

    if( len <= 0 )      //  ���ڴ�������ˣ������ڽ�һ������
        return 0;

    switch(proto ) {
        case ETH_P_IP:
            if( is_ip_filter(filter,data,len) ){
                return 1;
            }
            break;

        case ETH_P_ARP:
            ret = filter->protoallow[EARPProto] ? 0:1;
            break;

        case ETH_P_RARP: 
            ret = filter->protoallow[ERARPProto] ? 0:1;
            break;

        default:
            ret = filter->protoallow[EOtherProto] ? 0:1;
            break;
    }

    if( ret ){
        DBG_ECHO("filter by proto\n");
    }
    return ret;
}

