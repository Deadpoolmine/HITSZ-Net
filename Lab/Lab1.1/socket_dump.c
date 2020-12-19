#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h> 
#include <sys/types.h>
#include <linux/if_ether.h>
#include <netinet/in.h>
#include "protocol_helper.h"
#include "protocol_field.h"
#include "time.h"
#include <string.h> 
#define BUFFER_MAX 2048
#define CHANGE_LINE 1
#define NCHANGE_LINE 0
#define MAXSTR 100

char* 
info_type2str(int info_type){
    char *str = (char *)malloc(MAXSTR * sizeof(char));
    switch (info_type)
    {
    case T_TCP:
        /* code */
        str = "tcp";
        break;
    case T_UDP:
        str = "udp";
        break;
    case T_ARPRPL:
    case T_ARPREQ:
        str = "arp";
        break;
    case T_ICMPREQ:
    case T_ICMPRPL:
        str = "icmp";
        break;
    case T_RARP:
        str = "rarp";
        break;
    default:
        str = "unknown";
        break;
    }
}

int 
main(int argc, char const *argv[])
{

    int fd;
    int str_len;
    char buffer[BUFFER_MAX];
    char *ethernet_head;
    /** 数据段头  */
    char *data_head;
    /** 记录宏观上包的类型  */
    int type_field;
    clock_t start, finish;
    double  duration;
    /** 记录细分的包类型，从而进行打印 */
    int current_type = T_TCP;
    /** 接收到的包号  */
    long frame_num = 0;

    start = clock();
   
    unsigned char *p;
    if((fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) < 0)
    {
        printf("error create raw socket\n");
        return -1;
    }
    while (1)
    {
        frame_num++;
        str_len = recvfrom(fd,buffer,2048,0,NULL,NULL);
        /** 因为最少的情况就是  ARP REQUEST*/
        if (str_len < 42)
        {
            printf("error when recv msg\n");
            return -1;
        }
        ethernet_head = buffer;
        
        printf("==========================BLOCK START=============================\n");
        //p = ethernet_head;
        print_mac("DstMACaddress: ", ethernet_head, CHANGE_LINE);
        print_mac("SrcMACaddress: ", ethernet_head + 6, CHANGE_LINE);
        type_field = parse_byte(ethernet_head + 12, 2); 
        //printf("type_field: %x, %d\n",type_field, type_field);
        data_head = ethernet_head + 14;
        switch (type_field)
        {
        case F_IP:{
            /* ip */
            int ip_type_field;
            ip_type_field = (data_head + 9)[0];
            switch(ip_type_field){ 
                case IPPROTO_ICMP: {
                    //printf("icmp\n");
                    int icmp_type_field;
                    icmp_type_field = /* (data_head + 20)[0]; */parse_byte(data_head + 20, 1); 
                    switch (icmp_type_field)
                    {
                    case F_ICMPRPL:
                        current_type = T_ICMPRPL;
                        break;
                    case F_ICMPREQ:
                        current_type = T_ICMPREQ;
                        break;
                    default:
                        break;
                    }
                    break; 
                }
                case IPPROTO_TCP: {
                    //printf("tcp\n");
                    current_type = T_TCP;
                    break;
                } 
                case IPPROTO_UDP: {
                    //printf("udp\n");
                    current_type = T_UDP;
                    break; 
                }
                default: break; 
            } 
            break;
        }
        case F_ARP:{
            //printf("arp\n");
            int arp_type_field = parse_byte(data_head + 6, 2); 
            switch (arp_type_field)
            {
            case F_ARPREQ:
                current_type = T_ARPREQ;
                break;
            case F_ARPRPL:
                current_type = T_ARPRPL;
                break;
            default:
                break;
            }
            break;
        }
        case F_RARP:
            current_type = F_RARP;
            break;
        default:
            break;
        }
        finish = clock();
        duration = (double)(finish - start) * 100 / CLOCKS_PER_SEC;
        print_info(current_type, data_head, str_len, duration, frame_num);
        printf("==========================BLOCK END===============================\n\n");
    }
    return -1;
}


int parse_byte(char *start, int nbytes){
    int i = nbytes - 1;
    int counter = 0;
    int result = 0;
    unsigned int mask = 0xFF;
    while (i != 0)
    {   
        result |= (start[counter] << (i * 8));
        mask = ((mask << (i * 8)) | 0xFF);
        i--;
        counter++;
    }
    result |= start[counter];
    result &= mask;
    return result;
}

void 
print_ip(char *prefix, unsigned char *ip_start, int next_line){
    printf("%s%d.%d.%d.%d ",prefix,
        ip_start[0],ip_start[1],ip_start[2],ip_start[3]);
    if (next_line)
    {
        printf("\n");
    }
    return;
}

void 
print_mac(char *prefix, unsigned char *mac_start, int next_line){
    printf("%s%.2x:%02x:%02x:%02x:%02x:%02x ",prefix, 
        mac_start[0],mac_start[1],mac_start[2],mac_start[3],mac_start[4],mac_start[5]);
    if (next_line)
    {
        printf("\n");
    }
    return;
}

void 
print_protocol_name(char *name){
    int len = strlen(name);
/*     
    +---+
    |ARP|
    +---+ 
*/
    int tot = len + 4;
    printf("+");
    for (size_t i = 1; i <= len + 2; i++)
    {
        printf("-");
    }
    printf("+\n");

    printf("| ");
    printf("%s",name);
    printf(" |\n");

    printf("+");
    for (size_t i = 1; i <= len + 2; i++)
    {
        printf("-");
    }
    printf("+\n");
}

void 
print_info(int info_type, unsigned char *data_head, int length, double arrive_duration, long frame_num){
    unsigned char *src_mac;
    unsigned char *dest_mac;
    unsigned char *src_ip;
    unsigned char *dest_ip;

    print_protocol_name(info_type2str(info_type));
    printf("Frame No. %ld \n", frame_num);
    printf("Time: %lf; \n", arrive_duration);
    switch (info_type)
    {
    case T_TCP:
        src_ip = data_head + 12;
        dest_ip = src_ip + 4;
        print_ip("Source: ", src_ip, CHANGE_LINE);
        print_ip("Target: ", dest_ip, CHANGE_LINE);
        break;
    case T_UDP:
        src_ip = data_head + 12;
        dest_ip = src_ip + 4;
        print_ip("Source: ", src_ip, CHANGE_LINE);
        print_ip("Target: ", dest_ip, CHANGE_LINE);
        sleep(1);
        break;
    case T_ICMPRPL:{
        printf("Protocol: ICMP \n");
        printf("Length: %d; \n", length);
        printf("Info: Echo (ping) reply ");
        int id = parse_byte(data_head + 24, 2); 
        printf("id=%04x, ", id);
        int seq =  parse_byte(data_head + 27, 1);
        printf("seq=%d/%d, ", seq, seq << 8);
        int ttl = parse_byte(data_head + 8, 1); 
        printf("ttl=%d ", ttl);
        printf("(reply in %ld)", frame_num);
        sleep(1);
        break; 
    }
    case T_ICMPREQ:{
        printf("Protocol: ICMP \n");
        printf("Length: %d; \n", length);
        printf("Info: Echo (ping) request ");
        int id = parse_byte(data_head + 24, 2); 
        printf("id=%04x, ", id);
        int seq = parse_byte(data_head + 27, 1);
        printf("seq=%d/%d, ", seq, seq << 8);
        int ttl = parse_byte(data_head + 8, 1); 
        printf("ttl=%d ", ttl);
        printf("(request in %ld)", frame_num);
        sleep(1);
        break;
    }
    case T_ARPRPL:{
        src_mac = data_head + 8;
        src_ip = src_mac + 6;
        dest_mac = src_ip + 4;
        dest_ip = dest_mac + 6;
        
        print_mac("Source: ", src_mac, CHANGE_LINE);
        print_mac("Destination: ", dest_mac, NCHANGE_LINE);

        printf("Protocol: ARP \n");
        printf("Length: %d \n", length);
        print_ip("Info: Who has ", dest_ip, NCHANGE_LINE);
        print_ip("? Tell ", src_ip, CHANGE_LINE);
        sleep(1);
        break;
    }
    case T_ARPREQ:{
        src_mac = data_head + 8;
        src_ip = src_mac + 6;
        dest_mac = src_ip + 4;
        dest_ip = dest_mac + 6;

        print_mac("Source: ", src_mac, CHANGE_LINE);
        print_mac("Destination: ", dest_mac, CHANGE_LINE);

        printf("Protocol: ARP \n");
        printf("Length: %d \n", length);
        print_ip("Info: ", src_ip, NCHANGE_LINE);
        print_mac("is at ", src_mac, CHANGE_LINE);
        sleep(1);
        break;
    }
    case T_RARP:
        printf("RARP, cannot parse\n");
        break;
    default:
        break;
    }
    //sleep(1);
    printf("\n");
    
}
