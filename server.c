// server.c
// Juho Hong, Seungyong Lee
// 2022/11/17
// UDP Socket Program that uses GBN ARQ to receive file from client

#include "gbn.h"

int validate_checksum(header *gbn_header) {
    uint16_t checksum_received = gbn_header->checksum;
    // Set checksum to 0 to calculate checksum
    gbn_header->checksum = 0;
    uint16_t checksum_generated = 0;

    // Calculate checksum
    for (int i = 0; i < sizeof(header); i++) {
        checksum_generated += ((uint8_t *) gbn_header)[i];
    }

    // Set checksum back to original value
    gbn_header->checksum = checksum_received;

    return checksum_received == checksum_generated;
}


void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0); // Debug won't print if this is not set?

    int str_len;
    int serv_sock;
    int clnt_addr_size;

    struct sockaddr_in serv_addr;
    struct sockaddr_in clnt_addr;

    if (argc < 3) {
        error_handling("Enter port and debugging true/false as arguments");
    }

    int port = atoi(argv[1]);
    int debug = strcmp(argv[2], "true") == 0;

    serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (serv_sock == -1)
        error_handling("UDP socket error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("203.252.112.26");
    serv_addr.sin_port = htons(port);

    if (bind(serv_sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind() error");

    printf("Server started, waiting file transfer...");

    // Accept file transfer
    header *gbn_header = malloc(sizeof(header));
    memset(gbn_header->file_name, 0, FILE_NAME_LEN);
    memset(gbn_header->data, 0, BUF_SIZE);

    while (1) {
        // Start receiving header with SYN from client
        clnt_addr_size = sizeof(clnt_addr);
        str_len = recvfrom(serv_sock, gbn_header, sizeof(header), 0, (struct sockaddr *) &clnt_addr, &clnt_addr_size);
        if (str_len == -1)
            error_handling("recvfrom() error");

        else if (gbn_header->pack_type == SYN) { // If successful SYN
            // Send ACK to sender
            gbn_header->pack_type = ACK;
            sendto(serv_sock, gbn_header, sizeof(header), 0, (struct sockaddr *) &clnt_addr, sizeof(clnt_addr));
            break;
        }
        sleep(1);
    }

    char *file_name = gbn_header->file_name;
    FILE *fp = fopen(file_name, "wb");
    uint16_t expected_seq_num = 1;

    // Start receiving file
    while (1) {
        clnt_addr_size = sizeof(clnt_addr);
        str_len = recvfrom(serv_sock, gbn_header, sizeof(header), 0, (struct sockaddr *) &clnt_addr, &clnt_addr_size);
        if (str_len == -1)
            error_handling("recvfrom() error");

        if (gbn_header->pack_type == DATA) {
            if (gbn_header->seq_num == expected_seq_num) {
                // Validate checksum
                if (!validate_checksum(gbn_header)) {
                    // Send NAK to sender
                    gbn_header->pack_type = NAK;
                    sendto(serv_sock, gbn_header, sizeof(header), 0, (struct sockaddr *) &clnt_addr, sizeof(clnt_addr));
                    continue;
                }

                // Write received data to file
                fwrite(gbn_header->data, 1, gbn_header->data_len, fp);

                gbn_header->pack_type = ACK;
                gbn_header->seq_num = expected_seq_num;
                sendto(serv_sock, gbn_header, sizeof(header), 0, (struct sockaddr *) &clnt_addr, sizeof(clnt_addr));

                expected_seq_num++;

                if (debug)
                    printf("Received packet %d\n", gbn_header->seq_num);

            } else if (gbn_header->seq_num < expected_seq_num) {
                printf("Received duplicate packet %d\n", gbn_header->seq_num);

                expected_seq_num = gbn_header->seq_num;
                gbn_header->pack_type = ACK;
                gbn_header->seq_num = expected_seq_num;
                sendto(serv_sock, gbn_header, sizeof(header), 0, (struct sockaddr *) &clnt_addr, sizeof(clnt_addr));
            }
        }
            // If FIN and all packets received
        else if (gbn_header->pack_type == FIN) {
            gbn_header->pack_type = ACK;
            gbn_header->seq_num = 0;
            sendto(serv_sock, gbn_header, sizeof(header), 0, (struct sockaddr *) &clnt_addr, sizeof(clnt_addr));
            break;
        }
    }

    fclose(fp);
    printf("File transfer complete.\n");
    close(serv_sock);
    return 0;
}
