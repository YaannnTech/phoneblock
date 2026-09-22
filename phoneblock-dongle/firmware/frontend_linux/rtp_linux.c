#include "rtp_linux.h"

#include "platform.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#define RTP_HEADER_SIZE 12
#define RTP_FRAME_SIZE 160

int pb_linux_rtp_stream_alaw(const char *destination_host, int destination_port,
                             const char *audio_path, int local_port,
                             volatile sig_atomic_t *stop_requested)
{
    if (!destination_host || !audio_path || !stop_requested
            || destination_port <= 0 || destination_port > 65535) return -1;
    FILE *audio = fopen(audio_path, "rb");
    if (!audio) return -1;

    char port_text[12];
    snprintf(port_text, sizeof(port_text), "%d", destination_port);
    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_DGRAM };
    struct addrinfo *resolved = NULL;
    if (getaddrinfo(destination_host, port_text, &hints, &resolved) != 0 || !resolved) {
        fclose(audio);
        return -1;
    }
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0) {
        freeaddrinfo(resolved);
        fclose(audio);
        return -1;
    }
    if (local_port > 0) {
        struct sockaddr_in local = {
            .sin_family = AF_INET,
            .sin_addr.s_addr = htonl(INADDR_ANY),
            .sin_port = htons(local_port),
        };
        if (bind(socket_fd, (struct sockaddr *)&local, sizeof(local)) != 0) {
            close(socket_fd);
            freeaddrinfo(resolved);
            fclose(audio);
            return -1;
        }
    }

    uint16_t sequence = (uint16_t)pb_random_u32();
    uint32_t timestamp = pb_random_u32();
    uint32_t ssrc = pb_random_u32();
    unsigned char packet[RTP_HEADER_SIZE + RTP_FRAME_SIZE];
    packet[0] = 0x80;
    packet[1] = 8;
    packet[8] = (unsigned char)(ssrc >> 24);
    packet[9] = (unsigned char)(ssrc >> 16);
    packet[10] = (unsigned char)(ssrc >> 8);
    packet[11] = (unsigned char)ssrc;
    int result = 0;
    while (!*stop_requested) {
        size_t count = fread(packet + RTP_HEADER_SIZE, 1, RTP_FRAME_SIZE, audio);
        if (count == 0) break;
        if (count < RTP_FRAME_SIZE) {
            memset(packet + RTP_HEADER_SIZE + count, 0xd5,
                   RTP_FRAME_SIZE - count);
        }
        packet[2] = (unsigned char)(sequence >> 8);
        packet[3] = (unsigned char)sequence;
        packet[4] = (unsigned char)(timestamp >> 24);
        packet[5] = (unsigned char)(timestamp >> 16);
        packet[6] = (unsigned char)(timestamp >> 8);
        packet[7] = (unsigned char)timestamp;
        ssize_t sent = sendto(socket_fd, packet, sizeof(packet), 0,
                              resolved->ai_addr, resolved->ai_addrlen);
        if (sent != (ssize_t)sizeof(packet)) {
            result = -1;
            break;
        }
        sequence++;
        timestamp += RTP_FRAME_SIZE;
        pb_task_sleep_ms(20);
    }
    freeaddrinfo(resolved);
    close(socket_fd);
    fclose(audio);
    return result;
}