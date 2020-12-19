#include "arp.h"
#include "ip.h"
#include "utils.h"
#include "ethernet.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

/**
 * @brief 初始的arp包
 * 
 */
static const arp_pkt_t arp_init_pkt = {
    .hw_type = swap16(ARP_HW_ETHER),
    .pro_type = swap16(NET_PROTOCOL_IP),
    .hw_len = NET_MAC_LEN,
    .pro_len = NET_IP_LEN,
    .sender_ip = DRIVER_IF_IP,
    .sender_mac = DRIVER_IF_MAC,
    .target_mac = {0}
};

/**
 * @brief arp地址转换表
 * 
 */
arp_entry_t arp_table[ARP_MAX_ENTRY];

/**
 * @brief 长度为1的arp分组队列，当等待arp回复时暂存未发送的数据包
 * 
 */
arp_buf_t arp_buf;

/**
 * @brief 更新arp表
 *        你首先需要依次轮询检测ARP表中所有的ARP表项是否有超时，如果有超时，则将该表项的状态改为无效。
 *        接着，查看ARP表是否有无效的表项，如果有，则将arp_update()函数传递进来的新的IP、MAC信息插入到表中，
 *        并记录超时时间，更改表项的状态为有效。
 *        如果ARP表中没有无效的表项，则找到超时时间最长的一条表项，
 *        将arp_update()函数传递进来的新的IP、MAC信息替换该表项，并记录超时时间，设置表项的状态为有效。
 * 
 * @param ip ip地址
 * @param mac mac地址
 * @param state 表项的状态
 */
void arp_update(uint8_t *ip, uint8_t *mac, arp_state_t state)
{
    // TODO
    for (int i = 0; i < ARP_MAX_ENTRY; i++)
    {
        if(time(NULL) > arp_table[i].timeout ){
            arp_table[i].state = ARP_INVALID;
        }
    }
    int has_free = 0;
    for (int i = 0; i < ARP_MAX_ENTRY; i++)
    {
        if(arp_table[i].state == ARP_INVALID){
            has_free = 1;
            memcpy(arp_table[i].ip, ip, NET_IP_LEN);
            memcpy(arp_table[i].mac, mac, NET_MAC_LEN);
            arp_table[i].state = state;
            arp_table[i].timeout = time(NULL) + ARP_TIMEOUT_SEC;
            break;
        }
    }
    if(!has_free){
        int max_time = -99999;
        int max_time_index = -1;
        for (int i = 0; i < ARP_MAX_ENTRY; i++)
        {
            if(arp_table[i].timeout > max_time){
                max_time = arp_table[i].timeout;
                max_time_index = i;
            }
        }
        memcpy(arp_table[max_time_index].ip, ip, NET_IP_LEN);
        memcpy(arp_table[max_time_index].mac, mac, NET_MAC_LEN);
        arp_table[max_time_index].state = state;
        arp_table[max_time_index].timeout = time(NULL) + ARP_TIMEOUT_SEC;
    }
}

/**
 * @brief 从arp表中根据ip地址查找mac地址
 * 
 * @param ip 欲转换的ip地址
 * @return uint8_t* mac地址，未找到时为NULL
 */
static uint8_t *arp_lookup(uint8_t *ip)
{
    for (int i = 0; i < ARP_MAX_ENTRY; i++)
        if (arp_table[i].state == ARP_VALID && memcmp(arp_table[i].ip, ip, NET_IP_LEN) == 0)
            return arp_table[i].mac;
    return NULL;
}
/**
 * @brief 发送一个arp请求
 *        你需要调用buf_init对txbuf进行初始化
 *        填写ARP报头，将ARP的opcode设置为ARP_REQUEST，注意大小端转换
 *        将ARP数据报发送到ethernet层
 * 
 * @param target_ip 想要知道的目标的ip地址
 * 
 * 
 * 本实验发送的为无回报ARP报文
 */
static void arp_req(uint8_t *target_ip)
{
    // TODO
    buf_init(&txbuf, 0);
    buf_add_header(&txbuf, sizeof(arp_pkt_t));
    arp_pkt_t *arp_pkt = (arp_pkt_t *)txbuf.data;
    /* 不能直接arp_pkt = &arp_init_pkt */
    arp_pkt->hw_len = arp_init_pkt.hw_len;
    arp_pkt->hw_type = arp_init_pkt.hw_type;
    arp_pkt->opcode = swap16(ARP_REQUEST);
    arp_pkt->pro_len = arp_init_pkt.pro_len;
    arp_pkt->pro_type = arp_init_pkt.pro_type;
    memcpy(arp_pkt->sender_mac , arp_init_pkt.sender_mac, NET_MAC_LEN);
    memcpy(arp_pkt->sender_ip , arp_init_pkt.sender_ip, NET_IP_LEN);
    memcpy(arp_pkt->target_mac , arp_init_pkt.target_mac, NET_MAC_LEN);
    memcpy(arp_pkt->target_ip , target_ip, NET_IP_LEN);

    ethernet_out(&txbuf, ether_broadcast_mac, NET_PROTOCOL_ARP);
    printf("arp_req here\n");
}

/**
 * @brief 处理一个收到的数据包
 *        你首先需要做报头检查，查看报文是否完整，
 *        检查项包括：硬件类型，协议类型，硬件地址长度，协议地址长度，操作类型
 *        
 *        接着，调用arp_update更新ARP表项
 *        查看arp_buf是否有效，如果有效，则说明ARP分组队列里面有待发送的数据包。
 *        此时，收到了该request的应答报文。然后，根据IP地址来查找ARM表项，如果能找到该IP地址对应的MAC地址，
 *        即上一次调用arp_out()发送来自IP层的数据包时，由于没有找到对应的MAC地址进而先发送的ARP request报文
 *        则将缓存的数据包arp_buf再发送到ethernet层。
 * 
 *        如果arp_buf无效，还需要判断接收到的报文是否为request请求报文，并且，该请求报文的目的IP正好是本机的IP地址，
 *        则认为是请求本机MAC地址的ARP请求报文，则回应一个响应报文（应答报文）。
 *        响应报文：需要调用buf_init初始化一个buf，填写ARP报头，目的IP和目的MAC需要填写为收到的ARP报的源IP和源MAC。
 * 
 * @param buf 要处理的数据包
 * 
 * 从下往上拆解，从下收到一个buf，判断该buf是否为arp，不是的话，直接放走
 */
void arp_in(buf_t *buf)
{
    // TODO
    arp_pkt_t* arp_pkt = (arp_pkt_t *)buf->data;
    if(arp_pkt->hw_type != swap16(ARP_HW_ETHER)){
        printf("invalid hardware type !\n");
        return;
    }
    if(arp_pkt->pro_type != swap16(NET_PROTOCOL_IP)){
        printf("arp_in() arp can not deal with protocol except ip \n");
        return;
    }
    if(arp_pkt->hw_len != NET_MAC_LEN || arp_pkt->pro_len != NET_IP_LEN){
        printf("arp_in() mac len is not 6 or ip len is not 4 \n");
        return;
    }
    if(arp_pkt->opcode != swap16(ARP_REQUEST) && arp_pkt->opcode != swap16(ARP_REPLY)){
        printf("arp_in() unknown operation \n");
        return;
    }
    /* 更新arp表 */
    arp_update(arp_pkt->sender_ip, arp_pkt->sender_mac, ARP_VALID);
    /* 查看是否有上次没发送的数据包 */
    if(arp_buf.valid){
        uint8_t* target_mac = arp_lookup(arp_buf.ip);
        if (arp_buf.protocol == NET_PROTOCOL_IP)
        {
            ip_hdr_t* ip_header = (ip_hdr_t*)arp_buf.buf.data;
            printf("ip_header type is UDP? %d", ip_header->protocol == NET_PROTOCOL_UDP);
        }
        printf("ip_header type is UDP? %x", arp_buf.protocol);
        if(target_mac){
            ethernet_out(&arp_buf.buf, target_mac, arp_buf.protocol);
            arp_buf.valid = 0;  //已经发送了数据包，故无效了
        }
    }
    /* 没有 */
    else
    {
        /* 操作类型为request，目标ip为自己，回发响应报文 */
        if(arp_pkt->opcode == swap16(ARP_REQUEST) && 
                            !memcmp(arp_pkt->target_ip, net_if_ip, NET_IP_LEN)){
            arp_pkt_t *request_arp_pkt = arp_pkt;
            buf_init(&txbuf, 0);
            /* 记得加一个头 */
            buf_add_header(&txbuf, sizeof(arp_pkt_t));
            arp_pkt_t *reply_arp_pkt = (arp_pkt_t *)txbuf.data;

            reply_arp_pkt->hw_len = arp_init_pkt.hw_len;
            reply_arp_pkt->hw_type = arp_init_pkt.hw_type;
            reply_arp_pkt->opcode = swap16(ARP_REPLY);
            reply_arp_pkt->pro_len = arp_init_pkt.pro_len;
            reply_arp_pkt->pro_type = arp_init_pkt.pro_type;
            memcpy(reply_arp_pkt->sender_mac , arp_init_pkt.sender_mac, NET_MAC_LEN);
            memcpy(reply_arp_pkt->sender_ip , arp_init_pkt.sender_ip, NET_IP_LEN);

            memcpy(reply_arp_pkt->target_mac , request_arp_pkt->sender_mac, NET_MAC_LEN);
            memcpy(reply_arp_pkt->target_ip , request_arp_pkt->sender_ip, NET_IP_LEN);

            ethernet_out(&txbuf, request_arp_pkt->sender_mac, NET_PROTOCOL_ARP);
        }
    }
    
}

/**
 * @brief 处理一个要发送的数据包
 *        你需要根据IP地址来查找ARP表
 *        如果能找到该IP地址对应的MAC地址，则将数据报直接发送给ethernet层
 *        如果没有找到对应的MAC地址，则需要先发一个ARP request报文。
 *        注意，需要将来自IP层的数据包缓存到arp_buf中，等待arp_in()能收到ARP request报文的应答报文
 * 
 * @param buf 要处理的数据包
 * @param ip 目标ip地址
 * @param protocol 上层协议(网络层协议)
 */
void arp_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol)
{
    /* 首先进入第一个udp分片，其次 */
    // TODO
    uint8_t* target_mac = arp_lookup(ip);
    if(target_mac){
        ethernet_out(buf, target_mac, protocol);
    }
    else
    {
        arp_req(ip);
        printf("arp_out() arp requset: %s \n", iptos(ip));
        printf("protocol == swap16(NET_PROTOCOL_IP)?: %d\n",protocol == NET_PROTOCOL_IP);
        if(!arp_buf.valid){
            /* 拷贝buf */
            buf_copy(&arp_buf.buf, buf);
            /* 置当前buf有效 */
            arp_buf.valid = 1;
            /* 拷贝ip */
            memcpy(arp_buf.ip , ip, NET_IP_LEN);
            /* 拷贝protocol */
            arp_buf.protocol = protocol;
        }
        else
        {
            /* 如果是来自IP的包 */
            if(protocol == NET_PROTOCOL_IP){
                ip_hdr_t *ip_header = (ip_hdr_t*)buf->data;
                /* 如果IP包封装的是UDP包，则予以替换，保证UDP在buffer中优先级最高 */
                if(ip_header->protocol == NET_PROTOCOL_UDP){
                    memset(&arp_buf.buf, 0, BUF_MAX_LEN);
                    /* 拷贝buf */
                    buf_copy(&arp_buf.buf, buf);
                    /* 置当前buf有效 */
                    arp_buf.valid = 1;
                    /* 拷贝ip */
                    memcpy(arp_buf.ip , ip, NET_IP_LEN);
                    /* 拷贝protocol */
                    arp_buf.protocol = protocol;
                    printf("hhh1: %d ,%d\n",ip_header->protocol, ip_header->total_len);
                    printf("hhh2: %d ,%d\n",((ip_hdr_t*)arp_buf.buf.data)->protocol,((ip_hdr_t*)arp_buf.buf.data)->total_len);
                }
            }
        }
    }
}

/**
 * @brief 初始化arp协议
 * 
 */
void arp_init()
{
    for (int i = 0; i < ARP_MAX_ENTRY; i++)
        arp_table[i].state = ARP_INVALID;
    arp_buf.valid = 0;
    arp_req(net_if_ip);
}