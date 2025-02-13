#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>
#include "consts.h"
#include <errno.h>

// Main function of transport layer; never quits
void listen_loop(int sockfd, struct sockaddr_in* addr, int type,
                 ssize_t (*input_p)(uint8_t*, size_t),
                 void (*output_p)(uint8_t*, size_t)) {
    
    socklen_t addr_size = sizeof(struct sockaddr);

    // Client code
    if (type == CLIENT) {
        // Phase 1: Establish SYN

        // Create the SYN packet
        size_t twh_syn_data_len = 0; // FILL WITH REAL DATA LATER
        int seq_num = 0; // Anything under 1000 is fine
        char twh_syn_buf[sizeof(packet) + MAX_PAYLOAD] = {0}; 
        packet* twh_syn = (packet*) &twh_syn_buf; 
        twh_syn->win = htons(MAX_WINDOW);
        twh_syn->flags = SYN;
        // COME BACK AND SET PARITY BIT, LENGTH FIELD
        seq_num = seq_num + twh_syn_data_len;
        
        // Send the SYN (Omar says we can assume the handshakes are never dropped)
        if (sendto(sockfd, twh_syn, sizeof(packet) + twh_syn_data_len, 0, 
            (struct sockaddr*) addr, sizeof(struct sockaddr)) < 0) {
                fprintf(stderr, "Error sending three way handshake SYN\n");
                return errno;
        }

        // Phase 2: Wait for SYN ACK
        int ack_num;
        int flow_window_size = MAX_PAYLOAD;

        while (true) {
            char twh_synack_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
            packet* twh_synack = (packet*) &twh_synack_buf;
            int bytes_recvd = recvfrom(sockfd, twh_synack, sizeof(packet) + MAX_PAYLOAD, 0, (struct sockaddr*) addr, &addr_size);
            if (bytes_recvd < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
                            // The above errnos are fine, just means no data
                fprintf(stderr, "Error receiving message\n");
                return errno;
            }
            if (bytes_recvd > 0) {
                // Check flags
                if ((twh_synack->flags & SYN == 1) && (twh_synack->flags & ACK == 1)
                        && (ntohs(twh_synack->ack) == seq_num + 1)) {
                    // CHECK PARITY LATER
                    ack_num = ntohs(twh_synack->seq) + 1;
                    flow_window_size = ntohs(twh_synack->win);
                    
                    // CHECK DATA LATER
                }
            }
        }

        // Phase 3: Send ACK

        // Create the ACK packet
        size_t twh_ack_data_len = 0; // FILL WITH REAL DATA LATER
        char twh_ack_buf[sizeof(packet) + MAX_PAYLOAD] = {0}; // sequence number will be 0
        packet* twh_ack = (packet*) &twh_ack_buf; 
        twh_ack->win = htons(MAX_WINDOW);
        twh_ack->flags = ACK;
        // COME BACK AND SET PARITY BIT, LENGTH FIELD
        
        // Send the ACK (Omar says we can assume the handshakes are never dropped)
        if (sendto(sockfd, twh_ack, sizeof(packet), 0, 
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
        
        // Phase 2: Respond with SYN ACK

        // Phase 3: Await ACK

        // Phase 4: Normal

    }
}
