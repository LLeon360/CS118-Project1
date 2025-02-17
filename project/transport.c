#include "ack_queue.h"
#include "consts.h"
#include "io.h"
#include "ooo_buffer.h"
#include "sender_window.h"
#include "transport.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <sys/time.h>

// want to define the number of packets in window, this way we can allocate space for full packet
#define MAX_WINDOW_COUNT MAX_WINDOW / MAX_PAYLOAD
#define HEADER_SIZE 16 * 3

// Main function of transport layer; never quits
int listen_loop(int sockfd, struct sockaddr_in *addr, int type,
                 ssize_t (*input_p)(uint8_t *, size_t), void (*output_p)(uint8_t *, size_t)) {

    socklen_t addr_size = sizeof(struct sockaddr_in);

    // Client code
    if (type == CLIENT) {
        // Phase 1: Establish SYN

        // Create the SYN packet
        int seq_num = 17; // Anything under 1000 is fine
        char twh_syn_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
        packet *twh_syn = (packet *)&twh_syn_buf;
        size_t twh_syn_data_len = input_io(twh_syn->payload, MAX_PAYLOAD);
        twh_syn->seq = htons(seq_num);
        seq_num++;
        // Don't need to set twh_syn->ack
        twh_syn->length = htons(twh_syn_data_len);
        twh_syn->win = htons(MAX_WINDOW);
        twh_syn->flags = SYN;
        if ((bit_count(twh_syn) & 1) == 1) {
            twh_syn->flags |= PARITY;
        }

        // Send the SYN (Omar says we can assume the handshakes are never dropped)
        if (sendto(sockfd, twh_syn, sizeof(packet) + twh_syn_data_len, 0, (struct sockaddr *)addr,
                   sizeof(struct sockaddr)) < 0) {
            fprintf(stderr, "Error sending three way handshake SYN\n");
            return errno;
        }

        // Phase 2: Wait for SYN ACK
        int ack_num;
        int flow_window_size = MAX_PAYLOAD;
        char twh_synack_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
        packet *twh_synack = (packet *)&twh_synack_buf;

        while (true) {
            int bytes_recvd = recvfrom(sockfd, twh_synack, sizeof(packet) + MAX_PAYLOAD, 0,
                                       (struct sockaddr *)addr, &addr_size);
            if (bytes_recvd < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
                // The above errnos are fine, just means no data
                fprintf(stderr, "Error receiving three way handshake SYN ACK\n");
                return errno;
            }
            if (bytes_recvd > 0) {
                // Check flags, ack, parity
                if (((twh_synack->flags & SYN) == SYN) && ((twh_synack->flags & ACK) == ACK) &&
                    (ntohs(twh_synack->ack) == seq_num) && ((bit_count(twh_synack) & 1) == 0)) {
                    ack_num = ntohs(twh_synack->seq) + 1;
                    flow_window_size = ntohs(twh_synack->win);

                    output_io(twh_synack->payload, ntohs(twh_synack->length));

                    break;
                }
            }
        }

        // Phase 3: Send ACK

        // Create the ACK packet
        char twh_ack_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
        packet *twh_ack = (packet *)&twh_ack_buf;
        size_t twh_ack_data_len = input_io(twh_ack->payload, MAX_PAYLOAD);
        // See the three way handshake description on the spec
        // Already added one before to seq_num
        seq_num = (twh_syn_data_len == 0) ? 0 : seq_num;
        twh_ack->seq = htons(seq_num);
        seq_num++;
        twh_ack->ack = htons(ack_num);
        twh_ack->length = htons(twh_ack_data_len);
        twh_ack->win = htons(MAX_WINDOW);
        twh_ack->flags = ACK;
        if ((bit_count(twh_ack) & 1) == 1) {
            twh_ack->flags |= PARITY;
        }

        // Send the ACK (Omar says we can assume the handshakes are never dropped)
        if (sendto(sockfd, twh_ack, sizeof(packet) + twh_ack_data_len, 0, (struct sockaddr *)addr,
                   sizeof(struct sockaddr)) < 0) {
            fprintf(stderr, "Error sending three way handshake ACK\n");
            return errno;
        }

        // Phase 4: Normal loop
        normal_loop(sockfd, addr, CLIENT, input_p, output_p,
                    /* need_to_ack     = */ -1,
                    /* next_expected   = */ ack_num,
                    /* cur_win         = */ flow_window_size,
                    /* cur_seq         = */ seq_num);
    }

    // Server Code
    else {
        // Phase 1: Wait for SYN from client
        int ack_num;
        int flow_window_size = MAX_PAYLOAD;
        char twh_syn_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
        packet *twh_syn = (packet *)&twh_syn_buf;

        while (true) {
            int bytes_recvd = recvfrom(sockfd, twh_syn, sizeof(packet) + MAX_PAYLOAD, 0,
                                       (struct sockaddr *)addr, &addr_size);
            if (bytes_recvd < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
                // The above errnos are fine, just means no data
                fprintf(stderr, "Error receiving three way handshake SYN\n");
                return errno;
            }
            if (bytes_recvd > 0) {
                // Check flags and parity
                if (((twh_syn->flags & SYN) == SYN) && ((twh_syn->flags & ACK) == 0) &&
                    ((bit_count(twh_syn) & 1) == 0)) {
                    ack_num = ntohs(twh_syn->seq) + 1;
                    flow_window_size = ntohs(twh_syn->win);

                    output_io(twh_syn->payload, ntohs(twh_syn->length));

                    break;
                }
            }
        }

        // Phase 2: Respond with SYN ACK

        // Create the SYN ACK packet
        int seq_num = 1000; // Anything under 1000 is fine, making it different from client to make
                            // debugging easier
        char twh_synack_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
        packet *twh_synack = (packet *)&twh_synack_buf;
        size_t twh_synack_data_len = input_io(twh_synack->payload, MAX_PAYLOAD);
        twh_synack->seq = htons(seq_num);
        seq_num++;
        twh_synack->ack = htons(ack_num);
        twh_synack->length = htons(twh_synack_data_len);
        twh_synack->win = htons(MAX_WINDOW);
        twh_synack->flags = SYN | ACK;
        if ((bit_count(twh_synack) & 1) == 1) {
            twh_synack->flags |= PARITY;
        }

        // Send the SYN ACK (Omar says we can assume the handshakes are never dropped)
        if (sendto(sockfd, twh_synack, sizeof(packet) + twh_synack_data_len, 0,
                   (struct sockaddr *)addr, sizeof(struct sockaddr)) < 0) {
            fprintf(stderr, "Error sending three way handshake SYN ACK\n");
            return errno;
        }

        // Phase 3: Await ACK
        char twh_ack_buf[sizeof(packet) + MAX_PAYLOAD] = {0};
        packet *twh_ack = (packet *)&twh_ack_buf;

        // incase the ACK packet carries a payload, pending ack needs to be sent in the normal loop
        int need_to_ack_val = -1;

        while (true) {
            int bytes_recvd = recvfrom(sockfd, twh_ack, sizeof(packet) + MAX_PAYLOAD, 0,
                                       (struct sockaddr *)addr, &addr_size);
            if (bytes_recvd < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
                // The above errnos are fine, just means no data
                fprintf(stderr, "Error receiving three way handshake ACK\n");
                return errno;
            }
            if (bytes_recvd > 0) {
                // Check flags, ack, parity
                if (((twh_ack->flags & SYN) == 0) && ((twh_ack->flags & ACK) == ACK) &&
                    (ntohs(twh_ack->ack) == seq_num) &&
                    ((bit_count(twh_ack) & 1) == 0)) {
                    ack_num = ntohs(twh_ack->seq) + 1;
                    flow_window_size = ntohs(twh_ack->win);

                    output_io(twh_ack->payload, ntohs(twh_ack->length));

                    if (ntohs(twh_ack->length) > 0) {
                        need_to_ack_val = ntohs(twh_ack->seq) + 1;
                    }
                    break;
                }
            }
        }

        return normal_loop(sockfd, addr, SERVER, input_p, output_p,
                    /* need_to_ack     = */ need_to_ack_val,
                    /* cur_ack         = */ ack_num,
                    /* cur_win         = */ flow_window_size,
                    /* cur_seq         = */ seq_num);
    }
}

int normal_loop(int sockfd, struct sockaddr_in *addr, int type,
                 ssize_t (*input_p)(uint8_t *, size_t), void (*output_p)(uint8_t *, size_t),
                 int need_to_ack, int cur_ack, int cur_win, int cur_seq) {
    // This is the normal loop after the handshake
    // You can use this to send and receive packets
    // The handshake code is in listen_loop()

    /**
     * cur_seq is the SEQ num for the first packet that will be sent out by the normal loop, will
     * be used continuously for the sender window as it moves, needs to handle wraparound
     * need_to_ack is the SEQ num to pick up off of the incompleteness of the TWH for the server
     * which received an ACK with payload (if -1, no need to ack anything, either because payload is
     * empty or this is client) 
     * cur_ack is the first packet that the sender is
     * expecting to receive, this is used to determine if a packet is in order or out of order
     */

    /**
    NOTE: There is a bit of scuffness in picking off where the twh leaves off due to the
    incompleteness of the TWH (if there's a payload on the last ACK from client -> server). This
    because: On the client side, it has just sent out 3rd packet of ACK which may have payload and
    thus may need an ACK. On the server side, it has just receieved ACK which may have payload and
    thus may need to send an ACK out

    On the client side, there will be weirdness in that the first ACK expected will not correspond
    to any packet in the sender window but also shouldn't be treated as a NACK. On the server side,
    the server will need to start with a pending ACK queued to ack the client-ACK from TWH

    Client carries over the last_ack from the TWH as the last unacked packet but it doesn't need to
    move it's sender window. Client can probably discard ACKS that fall below the sender window (in
    that it doesn't move the sender window)

    Sender carries in a need_to_ack which is the last packet it needs to ack into the pending acks
     */

    // need to store a sender window, for window of packets that are in flight / not acked
    sender_window_queue* sender_window = malloc(sizeof(sender_window_queue));
    init_sender_window_queue(sender_window);

    // Initialize out-of-order table
    ooo_buffer *recv_buffer = ooo_buffer_create(DEFAULT_OUT_OF_ORDER_CAPACITY);

    // buffer up ACKS to be paired into outgoing data
    ack_queue* acks_queued = malloc(sizeof(ack_queue));
    init_ack_queue(acks_queued);

    // if given need_to_ack, to pick up where the TWH left off
    if (need_to_ack != -1) {
        enqueue_ack(acks_queued, need_to_ack);
    }

    // track dup ACKs
    int dup_acks = 0;
    int last_dup_ack = -1;

    struct timeval send_time_of_earliest_packet = {0};
    int sent_first_packet = 0;

    while (true) {
        // Send as much as possible out of the stdout into the sender window
        while (sender_window->byte_count < cur_win) {
            // try reading from stdin, if len is 0, nothing left to send
            char* buf = calloc(1, sizeof(packet) + MAX_PAYLOAD);
            packet *p = (packet *) buf;
            // Note that when calling free on p, C will know to free the space in the payload, so calling free on p is ok

            size_t data_len = input_io(p->payload, MIN(cur_win - sender_window->byte_count, MAX_PAYLOAD));
            if (data_len == 0) {
                free(p);
                break;
            }
            // Create the packet
            p->seq = htons(cur_seq);
            fprintf(stderr, "User %d is sending packet number %d\n", type, cur_seq);
            cur_seq++;
            // check if we have an ACK to send
            if (acks_queued->count > 0) {
                p->flags |= ACK;
                p->ack = htons(dequeue_ack(acks_queued));
            }
            p->length = htons(data_len);
            p->win = htons(MAX_WINDOW);
            if ((bit_count(p) & 1) == 1) {
                p->flags |= PARITY; // set the parity bit
            }

            // Send the packet
            if (sendto(sockfd, p, sizeof(packet) + data_len, 0, (struct sockaddr *)addr,
                        sizeof(struct sockaddr)) < 0) {
                fprintf(stderr, "Error sending data packet\n");
                return errno;
            }

            if (sent_first_packet == 0) {
                // If this is the first packet sent, set the send time
                gettimeofday(&send_time_of_earliest_packet, NULL);
                sent_first_packet = 1;
            }

            // Add the packet to the sender window
            enqueue_sender_window(sender_window, p);
        }
        // since we can no longer bundle acks with data, we need to send a packet with just the
        // ACK, these don't need to be buffered up
        while (acks_queued->count > 0) {
            // Create the packet
            // Since we don't need to buffer these, no need to dynamically allocate them
            char buf[sizeof(packet) + MAX_PAYLOAD] = {0};
            packet *p = (packet *) &buf;
            // Pure ACK packets do not increase SEQ number
            // In fact, we'll just set SEQ to 0
            p->seq = 0;
            p->ack = htons(dequeue_ack(acks_queued));
            p->flags |= ACK;
            p->length = 0;
            p->win = htons(MAX_WINDOW);
            if ((bit_count(p) & 1) == 1) {
                p->flags |= PARITY; // set the parity bit
            }

            fprintf(stderr, "User %d is sending ACK packet number %d\n", type, ntohs(p->ack));
            // Send the packet
            if (sendto(sockfd, p, sizeof(packet), 0, (struct sockaddr *)addr,
                        sizeof(struct sockaddr)) < 0) {
                fprintf(stderr, "Error sending ACK packet\n");
                return errno;
            }
        }

        // Check if there is anything to read since recvfrom is nonblocking
        // Receive a packet
        char buf[sizeof(packet) + MAX_PAYLOAD] = {0};
        packet *p = (packet *) &buf;
        socklen_t addr_size = sizeof(struct sockaddr_in);
        int bytes_recvd = recvfrom(sockfd, p, sizeof(packet) + MAX_PAYLOAD, 0,
                                    (struct sockaddr *)addr, &addr_size);
        if (bytes_recvd < 0 && !(errno == EAGAIN || errno == EWOULDBLOCK)) {
            // An actual error occurred
            fprintf(stderr, "Error receiving packet\n");
            return errno;
        }
        if (bytes_recvd > 0) {
            // do basic validation on the packet for (seq in range (no order check in this),
            // valid len, valid window, parity)
            fprintf(stderr, "User %d got a packet\n", type);
            if (((bit_count(p) & 1) == 0)) {
                // if packet has the ACK flag, must handle that, logic is separate from handling data
                if (p->flags & ACK) {
                    // check if the ACK is in the sender window
                    int ack_num = ntohs(p->ack);
                    fprintf(stderr, "Received ACK %d\n", ack_num);

                    if ((peek_sender_window(sender_window) != NULL) && (ack_num <= ntohs(peek_sender_window(sender_window)->seq))) {
                        if (ack_num == last_dup_ack) {
                            // duplicate ACK
                            dup_acks++;
                            fprintf(stderr, "Increasing duplicate ACKs, count is %d\n", dup_acks);
                        }
                        else {
                            // new ACK
                            dup_acks = 0;
                            last_dup_ack = ack_num;
                            fprintf(stderr, "New (too small) ACK num %d\n", ack_num);
                        }
                        if (dup_acks == 3) {
                            // handle duplicate ACK
                            // resend the first packet in the sender window
                            packet *sent_pkt = peek_sender_window(sender_window);
                            if (sent_pkt != NULL) fprintf(stderr, "Duplicate ACKs, resending packet %d\n", ntohs(sent_pkt->seq));

                            if ((sent_pkt != NULL) && (sendto(sockfd, sent_pkt,
                                        sizeof(packet) + ntohs(sent_pkt->length), 0,
                                        (struct sockaddr *)addr, sizeof(struct sockaddr)) < 0)) {
                                fprintf(stderr, "Error resending packet\n");
                                return errno;
                            }
                            dup_acks = 0;
                        }
                    }
                    else {
                        dup_acks = 0;
                        last_dup_ack = ack_num;

                        fprintf(stderr, "Packets to remove from sending buffer\n");

                        // Remove packets with a SEQ number less than the received ACK number from our sender window
                        while ((peek_sender_window(sender_window) != NULL) && (ack_num > ntohs(peek_sender_window(sender_window)->seq))) {
                            packet *sent_pkt = dequeue_sender_window(sender_window);
                            fprintf(stderr, "Removing packet %d from sender window\n", ntohs(sent_pkt->seq));
                            if (ntohs(sent_pkt->seq) == ack_num) {
                                free(sent_pkt);
                                break;
                            }
                            else {
                                free(sent_pkt);
                            }
                        }

                        // reset the retransmission timer
                        gettimeofday(&send_time_of_earliest_packet, NULL);
                    }
                }

                // Handle data in packet
                int pkt_seq = ntohs(p->seq);
                int pkt_len = ntohs(p->length);
                cur_win = ntohs(p->win);

                fprintf(stderr, "User %d received SEQ %d, expecting SEQ %d\n", type, pkt_seq, cur_ack);

                // If packet is exactly what we're expecting, output immediately
                if (pkt_seq == cur_ack) {
                    output_io(p->payload, pkt_len);
                    cur_ack++;
                    ooo_buffer_flush(recv_buffer, &cur_ack);
                    enqueue_ack(acks_queued, cur_ack);
                }
                else if (pkt_seq > cur_ack &&
                            pkt_seq < cur_ack + MAX_WINDOW_COUNT &&
                                // make sure there is enough space
                            !ooo_buffer_is_full(recv_buffer) &&
                                // don't bother storing the 0 length pure ACK packets
                            pkt_len != 0) {
                    // add out of order packet
                    ooo_buffer_store(recv_buffer, pkt_seq, pkt_len, p->payload);
                    enqueue_ack(acks_queued, cur_ack);
                }
            }
        }

        // Check if the retransmission timer has expired
        struct timeval current_time;
        gettimeofday(&current_time, NULL);

        // 1 sec timeout
        if (TV_DIFF(current_time, send_time_of_earliest_packet) > 1) {
            // Resend the first packet in the sender window
            packet *sent_pkt = peek_sender_window(sender_window);
            if (peek_sender_window(sender_window) != NULL) fprintf(stderr, "Timeout, resending packet %d\n", ntohs(sent_pkt->seq));
            if ((peek_sender_window(sender_window) != NULL) && (sendto(sockfd, sent_pkt, sizeof(packet) + ntohs(sent_pkt->length), 0,
                       (struct sockaddr *)addr, sizeof(struct sockaddr)) < 0)) {
                fprintf(stderr, "Error resending packet\n");
                return errno;
            }
            gettimeofday(&send_time_of_earliest_packet, NULL);
        }
    }

    // This will never happen, but just for good measure here are some frees
    ooo_buffer_destroy(recv_buffer);
    free(sender_window);
    free(acks_queued);
    return 0;
}
