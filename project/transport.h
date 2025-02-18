#pragma once

#include <stdint.h>
#include <unistd.h>

// Main function of transport layer; never quits
int listen_loop(int sockfd, struct sockaddr_in* addr, int type,
                 ssize_t (*input_p)(uint8_t*, size_t),
                 void (*output_p)(uint8_t*, size_t));

int normal_loop(int sockfd, struct sockaddr_in *addr, int type,
                 ssize_t (*input_p)(uint8_t *, size_t), void (*output_p)(uint8_t *, size_t),
                 int cur_ack, int cur_win, int cur_seq);