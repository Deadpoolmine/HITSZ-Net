#include "icmp.h"
#include "ip.h"
#include <string.h>
#include <stdio.h>

/**
 * @brief 处理一个收到的数据包
 *        你首先要检查ICMP报头长度是否小于icmp头部长度
 *        接着，查看该报文的ICMP类型是否为回显请求，
 *        如果是，则回送一个回显应答（ping应答），需要自行封装应答包。
 * 
 *        应答包封装如下：
 *        首先调用buf_init()函数初始化txbuf，然后封装报头和数据，
 *        数据部分可以拷贝来自接收到的回显请求报文中的数据。
 *        最后将封装好的ICMP报文发送到IP层。  
 * 
 * @param buf 要处理的数据包
 * @param src_ip 源ip地址
 */
void icmp_in(buf_t *buf, uint8_t *src_ip)
{
    if(buf->len < sizeof(icmp_hdr_t)){
        printf("icmp() too short for icmp!\n");
        return;
    }
    // TODO
    icmp_hdr_t* icmp_header = (icmp_hdr_t *)buf->data;
    uint16_t cache_check_cum = icmp_header->checksum;
    icmp_header->checksum = 0;
    uint16_t check_sum = checksum16((uint16_t *)buf->data, buf->len); 
    icmp_header->checksum = cache_check_cum;
    if(cache_check_cum != check_sum){
        printf("icmp() check sum error\n");
        return;
    }
    if(icmp_header->type == ICMP_TYPE_ECHO_REQUEST){
        buf_init(&txbuf, buf->len);
        icmp_hdr_t* icmp_reply_header = (icmp_hdr_t *)txbuf.data;
        icmp_reply_header->type = ICMP_TYPE_ECHO_REPLY;
        icmp_reply_header->id = icmp_header->id;
        icmp_reply_header->checksum = 0;
        icmp_reply_header->code = 0;
        icmp_reply_header->seq = icmp_header->seq;
        /* 数据部分可以拷贝来自接收到的回显请求报文中的数据。 */
        memcpy(txbuf.data + sizeof(icmp_hdr_t), buf->data + sizeof(icmp_hdr_t), buf->len - sizeof(icmp_hdr_t));
        icmp_reply_header->checksum = checksum16((uint16_t *)txbuf.data, txbuf.len);
        printf("%x \n", icmp_reply_header->checksum);
        ip_out(&txbuf, src_ip, NET_PROTOCOL_ICMP);
    }

}

/**
 * @brief 发送icmp不可达
 *        你需要首先调用buf_init初始化buf，长度为ICMP头部 + IP头部 + 原始IP数据报中的前8字节 
 *        填写ICMP报头首部，类型值为目的不可达
 *        填写校验和
 *        将封装好的ICMP数据报发送到IP层。
 * 
 * @param recv_buf 收到的ip数据包
 * @param src_ip 源ip地址
 * @param code icmp code，协议不可达或端口不可达
 */
void icmp_unreachable(buf_t *recv_buf, uint8_t *src_ip, icmp_code_t code)
{
    /* recv_buf带有ip头部 */
    // TODO
    buf_init(&txbuf, sizeof(icmp_hdr_t) + sizeof(ip_hdr_t) + 8);
    icmp_hdr_t * icmp_header = (icmp_hdr_t *)txbuf.data;
    icmp_header->type = ICMP_TYPE_UNREACH;
    icmp_header->code = code;
    icmp_header->checksum = 0;
    /* 差错报文第5~8字节为0 */
    icmp_header->seq = 0;
    icmp_header->id = 0;
    /* 记得别传buf。。。。data、data、data */
    memcpy(txbuf.data + sizeof(icmp_hdr_t), recv_buf->data, sizeof(ip_hdr_t) + 8);
    icmp_header->checksum = checksum16((uint16_t *)txbuf.data, txbuf.len);
    ip_out(&txbuf, src_ip, NET_PROTOCOL_ICMP);
}