#define T_TCP 1
#define T_UDP 2
#define T_ICMPRPL 3
#define T_ICMPREQ 4
#define T_ARPRPL 5
#define T_ARPREQ 6 
#define T_RARP 7

int parse_byte(char *start, int nbytes);
void print_ip(char *prefix, unsigned char *ip_start, int next_line);
void print_mac(char *prefix, unsigned char *mac_start, int next_line);
void print_info(int info_type, unsigned char *data_head, int length, double arrive_duration, long frame_num);
void print_protocol_name(char *name);