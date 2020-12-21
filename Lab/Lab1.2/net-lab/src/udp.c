#include "udp.h"
#include "ip.h"
#include "icmp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * @brief udp处理程序表
 * 
 */
static udp_entry_t udp_table[UDP_MAX_HANDLER];

/**
 * @brief udp伪校验和计算
 *        1. 你首先调用buf_add_header()添加UDP伪头部
 *        2. 将IP头部拷贝出来，暂存被UDP伪头部覆盖的IP头部
 *        3. 填写UDP伪头部的12字节字段
 *        4. 计算UDP校验和，注意：UDP校验和覆盖了UDP头部、UDP数据和UDP伪头部
 *        5. 再将暂存的IP头部拷贝回来
 *        6. 调用buf_remove_header()函数去掉UDP伪头部
 *        7. 返回计算后的校验和。  
 * 
 * @param buf 要计算的包
 * @param src_ip 源ip地址
 * @param dest_ip 目的ip地址
 * @return uint16_t 伪校验和
 */
static uint16_t udp_checksum(buf_t *buf, uint8_t *src_ip, uint8_t *dest_ip)
{
    udp_hdr_t *udp_header = (udp_hdr_t *)buf->data;
    uint16_t udp_total_len = udp_header->total_len;
    // TODO
    buf_add_header(buf, sizeof(udp_peso_hdr_t));
    udp_peso_hdr_t *udp_peso_header = (udp_peso_hdr_t *)buf->data;
    memcpy(udp_peso_header->dest_ip, dest_ip, NET_IP_LEN); 
    memcpy(udp_peso_header->src_ip, src_ip, NET_IP_LEN); 
    udp_peso_header->placeholder = 0;
    udp_peso_header->protocol = NET_PROTOCOL_UDP;
    /* udp 长度 ？ 到底是什么？？？ */
    udp_peso_header->total_len = udp_total_len;
    if(buf->len % 2 != 0){
        uint8_t *padding_data = (uint8_t *)malloc(buf->len + 1);
        memset(padding_data, 0, buf->len + 1);
        for (int i = 0; i < buf->len; i++)
        {
            padding_data[i] = buf->data[i];
        }
        padding_data[buf->len] = 0;
        uint16_t check_sum = checksum16((uint16_t *)padding_data, buf->len + 1);
        buf_remove_header(buf, sizeof(udp_peso_hdr_t));
        return check_sum;
    }
    else
    {
        uint16_t check_sum = checksum16((uint16_t *)buf->data, buf->len);
        buf_remove_header(buf, sizeof(udp_peso_hdr_t));
        return check_sum;   
    }
}

/**
 * @brief 处理一个收到的udp数据包
 *        你首先需要检查UDP报头长度
 *        接着计算checksum，步骤如下：
 *          （1）先将UDP首部的checksum缓存起来
 *          （2）再将UDP首都的checksum字段清零
 *          （3）调用udp_checksum()计算UDP校验和
 *          （4）比较计算后的校验和与之前缓存的checksum进行比较，如不相等，则不处理该数据报。
 *       然后，根据该数据报目的端口号查找udp_table，查看是否有对应的处理函数（回调函数）
 *       
 *       如果没有找到，则调用buf_add_header()函数增加IP数据报头部(想一想，此处为什么要增加IP头部？？)
 *       然后调用icmp_unreachable()函数发送一个端口不可达的ICMP差错报文。
 * 
 *       如果能找到，则去掉UDP报头，调用处理函数（回调函数）来做相应处理。
 * 
 * @param buf 要处理的包
 * @param src_ip 源ip地址
 */
void udp_in(buf_t *buf, uint8_t *src_ip)
{
    // TODO
    if(buf->len < 8){
        printf("udp() too short for udp!\n");
        return;
    }
    udp_hdr_t *udp_header = (udp_hdr_t *)buf->data;
    buf->len = swap16(udp_header->total_len);
    uint16_t cache_check_sum = udp_header->checksum;
    udp_header->checksum = 0;

    ip_hdr_t *cache_ip_header = (ip_hdr_t *)malloc(sizeof(ip_hdr_t));
    ip_hdr_t *origin_ip_header = (ip_hdr_t *)(buf->data - sizeof(ip_hdr_t));
    memcpy(cache_ip_header, origin_ip_header, sizeof(ip_hdr_t));
    uint16_t check_sum = udp_checksum(buf, cache_ip_header->src_ip, cache_ip_header->dest_ip);
    memcpy(buf->data - sizeof(ip_hdr_t), cache_ip_header, sizeof(ip_hdr_t));

    if(check_sum != cache_check_sum){
        printf("udp() check sum error!\n");
        return;
    }
    int is_find = 0;
    for (int i = 0; i < UDP_MAX_HANDLER; i++)
    {
        if(udp_table[i].port == swap16(udp_header->dest_port) 
        && udp_table[i].valid 
        && udp_table[i].handler){
            is_find = 1;
            buf_remove_header(buf, sizeof(udp_hdr_t));
            udp_table[i].handler(&udp_table[i], src_ip, udp_header->src_port, buf);
            break;
        }
    }
    if(!is_find){
        /* 为了调用icmp_unreachable，我们应该加入ip头部，可以将icmp视作介于网络层和传输层的一层协议 */
        /** 事实上,ip头部就在udp前  */
        buf_add_header(buf, sizeof(ip_hdr_t));
        // ip_hdr_t* ip_header = (ip_hdr_t *) buf->data;
        // ip_header->version = IP_VERSION_4;
        // ip_header->hdr_len = 20 / IP_HDR_LEN_PER_BYTE; 
        // ip_header->tos = 0;
        // ip_header->total_len = swap16(buf->len);
        // ip_header->id = 0;
        // ip_header->flags_fragment = 0;
        // ip_header->ttl = IP_DEFALUT_TTL;
        // ip_header->protocol = NET_PROTOCOL_UDP;
        // ip_header->hdr_checksum = 0;
        // memcpy(ip_header->src_ip, net_if_ip, NET_IP_LEN);
        // memcpy(ip_header->dest_ip, src_ip, NET_IP_LEN);
        icmp_unreachable(buf, src_ip, ICMP_CODE_PORT_UNREACH);
    }
    
}

/**
 * @brief 处理一个要发送的数据包
 *        你首先需要调用buf_add_header()函数增加UDP头部长度空间
 *        填充UDP首部字段
 *        调用udp_checksum()函数计算UDP校验和
 *        将封装的UDP数据报发送到IP层。    
 * 
 * @param buf 要处理的包
 * @param src_port 源端口号
 * @param dest_ip 目的ip地址
 * @param dest_port 目的端口号
 */
void udp_out(buf_t *buf, uint16_t src_port, uint8_t *dest_ip, uint16_t dest_port)
{
    // TODO
    buf_add_header(buf, sizeof(udp_hdr_t));
    udp_hdr_t* udp_header = (udp_hdr_t *)buf->data;
    udp_header->src_port = swap16(src_port);
    udp_header->dest_port = swap16(dest_port);
    /* 冗余的 */
    udp_header->total_len = swap16(buf->len);
    udp_header->checksum = 0;
    udp_header->checksum = udp_checksum(buf, net_if_ip, dest_ip); 
    ip_out(buf, dest_ip, NET_PROTOCOL_UDP);
}

/**
 * @brief 初始化udp协议
 * 
 */
void udp_init()
{
    for (int i = 0; i < UDP_MAX_HANDLER; i++)
        udp_table[i].valid = 0;
}

/**
 * @brief 打开一个udp端口并注册处理程序
 * 
 * @param port 端口号
 * @param handler 处理程序
 * @return int 成功为0，失败为-1
 */
int udp_open(uint16_t port, udp_handler_t handler)
{
    for (int i = 0; i < UDP_MAX_HANDLER; i++) //试图更新
        if (udp_table[i].port == port)
        {
            udp_table[i].handler = handler;
            udp_table[i].valid = 1;
            return 0;
        }

    for (int i = 0; i < UDP_MAX_HANDLER; i++) //试图插入
        if (udp_table[i].valid == 0)
        {
            udp_table[i].handler = handler;
            udp_table[i].port = port;
            udp_table[i].valid = 1;
            return 0;
        }
    return -1;
}

/**
 * @brief 关闭一个udp端口
 * 
 * @param port 端口号
 */
void udp_close(uint16_t port)
{
    for (int i = 0; i < UDP_MAX_HANDLER; i++)
        if (udp_table[i].port == port)
            udp_table[i].valid = 0;
}

/**
 * @brief 发送一个udp包
 * 
 * @param data 要发送的数据
 * @param len 数据长度
 * @param src_port 源端口号
 * @param dest_ip 目的ip地址
 * @param dest_port 目的端口号
 */
void udp_send(uint8_t *data, uint16_t len, uint16_t src_port, uint8_t *dest_ip, uint16_t dest_port)
{
    buf_init(&txbuf, len);
    memcpy(txbuf.data, data, len);
    udp_out(&txbuf, src_port, dest_ip, dest_port);
}