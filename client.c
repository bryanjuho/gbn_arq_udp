// client.c
// Juho Hong, Seungyong Lee
// 2022/11/17
// UDP Socket Program that sends file to server

// Uses GBN ARQ to handle packet loss
#include "header.h"

#define FILE_NAME_SRC "input.docx"
#define FILE_NAME_DEST "output.docx"

timer_t timerID;

int timeout = 0;

uint16_t get_checksum(header *gbn_header) {
    uint16_t checksum_generated = 0;

    // Calculate checksum
    for (int i = 0; i < sizeof(header); i++) {
        checksum_generated += ((uint8_t *) gbn_header)[i];
    }

    return checksum_generated;
}

void timeout_handler(int sig) {
    printf("Timeout Error\n");
    timeout = 1;
}

void init_timer(timer_t *_timerID) {
    struct sigaction sa;
    struct sigevent te;

    sa.sa_flags = 0;
    sa.sa_sigaction = NULL;
    sa.sa_handler = timeout_handler;
    sigaction(SIGTYPE, &sa, NULL);

    // Set timer
    te.sigev_notify = SIGEV_SIGNAL;
    te.sigev_signo = SIGTYPE;
    te.sigev_value.sival_ptr = _timerID;
    timer_create(CLOCK_REALTIME, &te, _timerID);
}

void start_stop_timer(timer_t *_timerID, int value) {
    struct itimerspec its;
    its.it_value.tv_sec = value == 0 ? 0 : value;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    timer_settime(*_timerID, 0, &its, NULL);
}

int get_timer_expiration(timer_t *_timerID) {
    struct itimerspec its;
    timer_gettime(*_timerID, &its);
    return its.it_value.tv_sec;
}

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);

    int sock;
    int addr_size;

    struct sockaddr_in serv_addr;
    struct sockaddr_in from_addr;

    if (argc != 5) {
        error_handling("Usage: <IP> <port> <SWS> <Debug>\n");
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);
    int sws = atoi(argv[3]);
    int debug = strcmp(argv[4], "true") == 0;

    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock == -1)
        error_handling("UDP socket error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    char *filename = FILE_NAME_SRC;
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        error_handling("File not found\n");
    }

    // GBN ARQ to handle packet loss
    uint16_t base = 1;
    uint16_t nextseqnum = 1;
    uint16_t window_size = sws;

    // Establish connection w/ header
    header *gbn_header = (header *) malloc(sizeof(header));
    memset(gbn_header->file_name, 0, FILE_NAME_LEN);
    memset(gbn_header->data, 0, BUF_SIZE);

    gbn_header->pack_type = SYN; // Send SYN
    gbn_header->seq_num = 0;
    strcpy((char *) gbn_header->file_name, FILE_NAME_DEST);

    // Send header
    sendto(sock, gbn_header, sizeof(header), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr));

    // Receive ACK
    recvfrom(sock, gbn_header, sizeof(header), 0, (struct sockaddr *) &from_addr, &addr_size);

    if (gbn_header->pack_type != ACK) { // If not ACK
        error_handling("Error: Received wrong type of packet\n");
    }

    if (debug) {
        printf("Received ACK, Start file transfer...\n");
    }

    int last = 0;
    // Send file
    init_timer(&timerID);

    while (1) {
        while (nextseqnum < base + window_size) {
            // Read file and get file size
            char *file_buf = malloc(BUF_SIZE);
            memset(file_buf, 0, BUF_SIZE);
            size_t file_size = fread(file_buf, 1, BUF_SIZE, fp);

            // IF EOF
            if (file_size < BUF_SIZE) {
                if (file_size == 0) {
                    break;
                }
                last = 1;
            }

            printf("Sending packet %d, size %ld\n", nextseqnum, file_size);

            // Create packet
            gbn_header->pack_type = DATA;
            gbn_header->seq_num = nextseqnum;
            gbn_header->checksum = 0;

            // Copy file data to packet as binary
            memcpy(gbn_header->data, file_buf, file_size);
            gbn_header->data_len = file_size;
            gbn_header->checksum = get_checksum(gbn_header);

            // Send packet
            sendto(sock, gbn_header, sizeof(header), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
            if (debug) {
                printf("Sent packet %d\n", nextseqnum);
            }

            if (base == nextseqnum)
                start_stop_timer(&timerID, TIMEOUT);

            if (last) {
                break;
            }
            nextseqnum++;
        }
        // Receive ACK from server
        recvfrom(sock, gbn_header, sizeof(header), 0, (struct sockaddr *) &from_addr, &addr_size);

        // Check if ACK is for correct packet
        if (gbn_header->pack_type == ACK && gbn_header->seq_num >= base && gbn_header->seq_num < nextseqnum) {

            if (debug) {
                printf("Received ACK %d\n", gbn_header->seq_num);
            }

            base = gbn_header->seq_num + 1;

            if (base == nextseqnum) {
                start_stop_timer(&timerID, 0);
            } else {
                start_stop_timer(&timerID, TIMEOUT);
            }
        } else {
            if (debug) {
                printf("Received wrong ACK %d\n", gbn_header->seq_num);
            }
            sleep(1); // Debug
            base = 1;
            nextseqnum = 1;
        }

        // if all packets sent and server ACKed all
        if (feof(fp) && base == nextseqnum) {

            // Wait for server to ACK last packet
            if (debug) {
                printf("Waiting for last ACK...\n");
            }

            // Send EOT
            gbn_header->pack_type = FIN;
            sendto(sock, gbn_header, sizeof(header), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
            if (debug) {
                printf("Sent EOT\n");
            }

            // Receive ACK
            recvfrom(sock, gbn_header, sizeof(header), 0, (struct sockaddr *) &from_addr, &addr_size);
            if (gbn_header->pack_type != ACK) { // If not ACK
                error_handling("Error: Received wrong type of packet\n");
            } else {
                if (debug) {
                    printf("Received File transfer complete ACK\n");
                }
                break;
            }

        }

        // Window slide with SWS arg
        if (base == nextseqnum) {
            window_size = sws;
        } else {
            window_size = sws - (nextseqnum - base);
        }

        //  If timeout
        if (timeout == 1) {
            start_stop_timer(&timerID, TIMEOUT);
            // resend all pkts in window
            for (int j = base; j < nextseqnum; j++) {
                // Read file
                char *file_buf = malloc(BUF_SIZE);

                fread(file_buf, 1, BUF_SIZE, fp);

                if (feof(fp)) {
                    break;
                }

                // Create packet
                gbn_header->pack_type = DATA; // Send DATA
                gbn_header->seq_num = j;
                gbn_header->checksum = 0;
                gbn_header->data_len = strlen(file_buf);
                memcpy(gbn_header->data, file_buf, BUF_SIZE);
                gbn_header->checksum = get_checksum(gbn_header);

                // Send packet
                sendto(sock, gbn_header, sizeof(header), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
                if (debug) {
                    printf("After timeout: Sent packet %d\n", j);
                }
            }
        }

    }

    close(sock);
    return 0;
}

