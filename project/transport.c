#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include "consts.h"
#include <errno.h>
#include <string.h>

// Main function of transport layer; never quits
void listen_loop(int sockfd, struct sockaddr_in* addr, int type,
                 ssize_t (*input_p)(uint8_t*, size_t),
                 void (*output_p)(uint8_t*, size_t)) {
    
    socklen_t addr_size = sizeof(struct sockaddr_in);

    // Client code
    if (type == CLIENT) {
        // Phase 1: Establish SYN

        // Create the SYN packet
        int seq_num = 0; // Anything under 1000 is fine
        char twh_syn_buf[sizeof(packet) + MAX_PAYLOAD] = {0}; 
        packet* twh_syn = (packet*) &twh_syn_buf; 
        size_t twh_syn_data_len = input_io(twh_syn->payload, MAX_PAYLOAD);
        // Can treat the last byte of data as the 0 seq number
        twh_syn->seq = htons(seq_num);
        // Don't need to set twh_syn->ack
        twh_syn->length = htons(twh_syn_data_len);
        twh_syn->win = htons(MAX_WINDOW);
        twh_syn->flags = SYN;
        if (bit_count(twh_syn) & 1 == 1) {
            twh_syn->flags |= PARITY;
        }
        
        // Send the SYN (Omar says we can assume the handshakes are never dropped)
        if (sendto(sockfd, twh_syn, sizeof(packet) + twh_syn_data_len, 0, 
            (struct sockaddr*) addr, sizeof(struct sockaddr)) < 0) {
                fprintf(stderr, "Error sending three way handshake SYN\n");
                return errno;
        }

        // Phase 2: Wait for SYN ACK
        int ack_num;
        int flow_window_size = MAX_PAYLOAD;
        char twh_synack_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
        packet* twh_synack = (packet*) &twh_synack_buf;

        while (true) {
            int bytes_recvd = recvfrom(sockfd, twh_synack, sizeof(packet) + MAX_PAYLOAD, 0, (struct sockaddr*) addr, &addr_size);
            if (bytes_recvd < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
                            // The above errnos are fine, just means no data
                fprintf(stderr, "Error receiving three way handshake SYN ACK\n");
                return errno;
            }
            if (bytes_recvd > 0) {
                // Check flags, ack, parity
                if ((twh_synack->flags & SYN == 1) && (twh_synack->flags & ACK == 1)
                        && (ntohs(twh_synack->ack) == seq_num + 1)
                        && (bit_count(twh_synack) & 1 == 0)) {
                    ack_num = ntohs(twh_synack->seq) + 1;
                    flow_window_size = ntohs(twh_synack->win);
                    
                    output_io(twh_synack->payload, twh_synack->length);

                    break;
                }
            }
        }

        // Phase 3: Send ACK

        // Create the ACK packet
        char twh_ack_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
        packet* twh_ack = (packet*) &twh_ack_buf; 
        size_t twh_ack_data_len = input_io(twh_ack->payload, MAX_PAYLOAD);
        seq_num += twh_syn_data_len;
        if (twh_syn_data_len == 0) {
            seq_num += 1;
        }
        twh_ack->seq = htons(seq_num);
        twh_ack->ack = htons(ack_num);
        twh_ack->length = htons(twh_ack_data_len);
        twh_ack->win = htons(MAX_WINDOW);
        twh_ack->flags = ACK;
        if (bit_count(twh_ack) & 1 == 1) {
            twh_ack->flags |= PARITY;
        }
        
        // Send the ACK (Omar says we can assume the handshakes are never dropped)
        if (sendto(sockfd, twh_ack, sizeof(packet) + twh_ack_data_len, 0, 
                (struct sockaddr*) addr, sizeof(struct sockaddr)) < 0) {
            fprintf(stderr, "Error sending three way handshake ACK\n");
            return errno;
        }

        // Phase 4: Normal
        while (true) {
            continue;
        }
    }
    
    // Server Code
    else {
        // Phase 1: Wait for SYN from client
        int ack_num;
        int flow_window_size = MAX_PAYLOAD;
        char twh_syn_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
        packet* twh_syn = (packet*) &twh_syn_buf;

        while (true) {
            int bytes_recvd = recvfrom(sockfd, twh_syn, sizeof(packet) + MAX_PAYLOAD, 0, (struct sockaddr*) addr, &addr_size);
            if (bytes_recvd < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
                            // The above errnos are fine, just means no data
                fprintf(stderr, "Error receiving three way handshake SYN\n");
                return errno;
            }
            if (bytes_recvd > 0) {
                // Check flags and parity
                if ((twh_syn->flags & SYN == 1) && (twh_syn->flags & ACK == 0) && (bit_count(twh_syn) & 1 == 0)) {
                    ack_num = ntohs(twh_syn->seq) + 1;
                    flow_window_size = ntohs(twh_syn->win);
                    
                    output_io(twh_syn->payload, twh_syn->length);

                    break;
                }
            }
        }
        
        // Phase 2: Respond with SYN ACK

        // Create the SYN ACK packet
        int seq_num = 1000; // Anything under 1000 is fine, making it different from client to make debugging easier
        char twh_synack_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
        packet* twh_synack = (packet*) &twh_synack_buf;
        size_t twh_synack_data_len = input_io(twh_synack->payload, MAX_PAYLOAD);
        // Can treat the last byte of data as the 0 seq number
        twh_synack->seq = htons(seq_num);
        twh_synack->ack = htons(ack_num);
        twh_synack->length = htons(twh_synack_data_len);
        twh_synack->win = htons(MAX_WINDOW);
        twh_synack->flags = SYN | ACK;
        if (bit_count(twh_synack) & 1 == 1) {
            twh_synack->flags |= PARITY;
        }
        
        // Send the SYN ACK (Omar says we can assume the handshakes are never dropped)
        if (sendto(sockfd, twh_synack, sizeof(packet) + twh_synack_data_len, 0, 
                (struct sockaddr*) addr, sizeof(struct sockaddr)) < 0) {
            fprintf(stderr, "Error sending three way handshake SYN ACK\n");
            return errno;
        }

        // Phase 3: Await ACK
        char twh_ack_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
        packet* twh_ack = (packet*) &twh_ack_buf;

        while (true) {
            int bytes_recvd = recvfrom(sockfd, twh_ack, sizeof(packet) + MAX_PAYLOAD, 0, (struct sockaddr*) addr, &addr_size);
            if (bytes_recvd < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
                            // The above errnos are fine, just means no data
                fprintf(stderr, "Error receiving three way handshake ACK\n");
                return errno;
            }
            if (bytes_recvd > 0) {
                // Check flags, ack, basic validation
                if ((twh_ack->flags & SYN == 0) && (twh_ack->flags & ACK == 1)
                        && (ntohs(twh_ack->ack) == seq_num + 1)
                        && basic_packet_validation(twh_ack, ack_num, flow_window_size)
                        ) {
                    ack_num = ntohs(twh_ack->seq) + 1;
                    flow_window_size = ntohs(twh_ack->win);
                    
                    output_io(twh_ack->payload, twh_ack->length);

                    break;
                }
            }
        }

        // Phase 4: Normal
        while (true) {
            continue;
        }

    }
}

// Checks that a few headers look correct
// DOES NOT check the SYN or ACK flags, do that yourself
// DON'T use this for the first two handshake messages, those messages are more lax
// Note that p->ack may not be cur_seq + 1, since it could be acknowledging a previous packet in your window
// Returns 1 on a good packet, 0 if something's wrong
int basic_packet_validation(packet* p, int cur_ack, int cur_win) {
    return 
        // Make sure seq is in expected range
        (ntohs(p->seq) >= cur_ack) && (ntohs(p->seq) < cur_ack + MAX_PAYLOAD - 1)
        // Make sure length is in expected range
        && (ntohs(p->length) <= MAX_PAYLOAD)
        // Make sure seq and length align
        && (ntohs(p->seq) == cur_ack - 1 + ntohs(p->length))
        // Make sure window didn't shrink
        // The spec said the window should never shrink
        && (ntohs(p->win) >= cur_win)
        // Check parity
        && (bit_count(p) & 1 == 0)
        ;
}