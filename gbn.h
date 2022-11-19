//
// Created by Juho Hong, Lee Seungyong on 2022/11/19.
//

#ifndef GBN_UDP_HEADER_H
#define GBN_UDP_HEADER_H

#define FILE_NAME_LEN 256
#define BUF_SIZE 500

// Define Packet type names
#define SYN 0
#define ACK 1
#define NAK 2
#define DATA 3
#define FIN 4

#define SIGTYPE SIGALRM
#define TIMEOUT 30

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>


// Header structure for packet
typedef struct {
    uint8_t pack_type;
    uint16_t seq_num;
    uint16_t checksum;
    size_t data_len;
    char file_name[FILE_NAME_LEN];
    char data[BUF_SIZE];
} header;


#endif //GBN_UDP_HEADER_H
