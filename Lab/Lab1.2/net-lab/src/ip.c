#include "ip.h"
#include "arp.h"
#include "icmp.h"
#include "udp.h"
#include <string.h>
#include <stdio.h>

uint32_t id = -1;

/**
 * @brief 处理一个收到的数据包
 *        你首先需要做报头检查，检查项包括：版本号、总长度、首部长度等。
 * 
 *        接着，计算头部校验和，注意：需要先把头部校验和字段缓存起来，再将校验和字段清零，
 *        调用checksum16()函数计算头部检验和，比较计算的结果与之前缓存的校验和是否一致，
 *        如果不一致，则不处理该数据报。
 * 
 *        检查收到的数据包的目的IP地址是否为本机的IP地址，只处理目的IP为本机的数据报。
 * 
 *        检查IP报头的协议字段：
 *        如果是ICMP协议，则去掉IP头部，发送给ICMP协议层处理
 *        如果是UDP协议，则去掉IP头部，发送给UDP协议层处理
 *        如果是本实验中不支持的其他协议，则需要调用icmp_unreachable()函数回送一个ICMP协议不可达的报文。
 *          
 * @param buf 要处理的包
 */
void ip_in(buf_t *buf)
{
    // TODO 
    ip_hdr_t* ip_header = (ip_hdr_t *)buf->data;
    if(ip_header->version != IP_VERSION_4)
    {
        printf("this packect is not IP V4 \n");
        return;
    }
    uint16_t cache_check_sum = ip_header->hdr_checksum;
    ip_header->hdr_checksum = 0;
    uint16_t check_sum = checksum16((uint16_t *)buf->data, ip_header->hdr_len * IP_HDR_LEN_PER_BYTE);
    ip_header->hdr_checksum = cache_check_sum;
    if(check_sum != cache_check_sum){
        printf("check sum error !\n");
        return;
    }
    else
    {
        if(memcmp(ip_header->dest_ip, net_if_ip, NET_IP_LEN) == 0){
            if(ip_header->protocol == NET_PROTOCOL_ICMP){
                buf_remove_header(buf, sizeof(ip_hdr_t));
                icmp_in(buf, ip_header->src_ip);
            }
            else if (ip_header->protocol == NET_PROTOCOL_UDP)
            {
                buf_remove_header(buf, sizeof(ip_hdr_t));
                udp_in(buf, ip_header->src_ip);
            }
            /* 说明本机无法解析该协议，为协议不可达 */
            else
            {
                icmp_unreachable(buf, ip_header->src_ip, ICMP_CODE_PROTOCOL_UNREACH);
            }
        }   
    }
}

/**
 * @brief 处理一个要发送的分片
 *        你需要调用buf_add_header增加IP数据报头部缓存空间。
 *        填写IP数据报头部字段。
 *        将checksum字段填0，再调用checksum16()函数计算校验和，并将计算后的结果填写到checksum字段中。
 *        将封装后的IP数据报发送到arp层。
 * 
 * @param buf 要发送的分片
 * @param ip 目标ip地址
 * @param protocol 上层协议
 * @param id 数据包id
 * @param offset 分片offset，必须被8整除
 * @param mf 分片mf标志，是否有下一个分片
 */
void ip_fragment_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol, int id, uint16_t offset, int mf)
{
    // TODO
    buf_add_header(buf, sizeof(ip_hdr_t));
    ip_hdr_t* ip_header = (ip_hdr_t *)buf->data;
    ip_header->version = IP_VERSION_4;
    ip_header->hdr_len = 20 / IP_HDR_LEN_PER_BYTE; 
    ip_header->tos = 0;
    ip_header->total_len = swap16(buf->len);
    ip_header->id = swap16(id);
    if(mf){
        ip_header->flags_fragment = swap16(offset);
        ip_header->flags_fragment |= IP_MORE_FRAGMENT;
    }
    else
    {
        ip_header->flags_fragment = swap16(offset);
        /* 记得加括号，铁汁 */
        ip_header->flags_fragment &= ~(IP_MORE_FRAGMENT);
    }
    printf("fragment: %x\n", ip_header->flags_fragment);
    ip_header->ttl = IP_DEFALUT_TTL;
    ip_header->protocol = protocol;
    ip_header->hdr_checksum = 0;
    memcpy(ip_header->src_ip, net_if_ip, NET_IP_LEN);
    memcpy(ip_header->dest_ip, ip, NET_IP_LEN);
    ip_header->hdr_checksum = checksum16((uint16_t *)buf->data, ip_header->hdr_len * IP_HDR_LEN_PER_BYTE);
    arp_out(buf, ip, NET_PROTOCOL_IP);
}

/**
 * @brief 处理一个要发送的数据包
 *        你首先需要检查需要发送的IP数据报是否大于以太网帧的最大包长（1500字节 - ip包头长度）。
 *        
 *        如果超过，则需要分片发送。 
 *        分片步骤：
 *        （1）调用buf_init()函数初始化buf，长度为以太网帧的最大包长（1500字节 - ip包头头长度）
 *        （2）将数据报截断，每个截断后的包长度 = 以太网帧的最大包长，调用ip_fragment_out()函数发送出去
 *        （3）如果截断后最后的一个分片小于或等于以太网帧的最大包长，
 *             调用buf_init()函数初始化buf，长度为该分片大小，再调用ip_fragment_out()函数发送出去
 *             注意：最后一个分片的MF = 0
 *    
 *        如果没有超过以太网帧的最大包长，则直接调用调用ip_fragment_out()函数发送出去。
 * 
 * @param buf 要处理的包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void ip_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol)
{
    id++;
    // ETHERNET_MTU 应该仅代表以太网协议所能携带 的 数据长度， 1500字节, 
    // 20字节的ip包头，故一个数据包为1480，1480 / 8 = 185，偏移以185为单位
    // TODO 
    int eth_max_pac_len = ETHERNET_MTU - sizeof(ip_hdr_t);
    int offset_per_group = eth_max_pac_len / IP_HDR_OFFSET_PER_BYTE;
    /* 超过了MTU */
    if(buf->len > eth_max_pac_len){
        int groups = buf->len / eth_max_pac_len;
        int remain = buf->len % eth_max_pac_len;
        for (int group = 0; group < groups; group++)
        {
            buf_init(&txbuf, eth_max_pac_len);
            /** 不能这样乱指  */
            /** txbuf.data = buf->data + group * eth_max_pac_len; */
            memcpy(txbuf.data, buf->data + group * eth_max_pac_len, eth_max_pac_len);
            /* 是最后一个整分组 且 没有剩余了，则最后一个整分组就是 最后一个 */
            if(group == groups - 1 && !remain){
                ip_fragment_out(&txbuf, ip, protocol, id, group * offset_per_group, 0);
            }
            else
            {
                ip_fragment_out(&txbuf, ip, protocol, id, group * offset_per_group, 1);
            }
        }
        if(remain){
            buf_init(&txbuf, remain);
            /*  txbuf.data = buf->data + groups * eth_max_pac_len; */
            memcpy(txbuf.data, buf->data + groups * eth_max_pac_len, eth_max_pac_len);
            ip_fragment_out(&txbuf, ip, protocol, id, groups * offset_per_group, 0);
        }
    }
    /* 未超过MTU */
    else
    {
        ip_fragment_out(buf, ip, protocol, id, 0, 0);
    }
    
}
