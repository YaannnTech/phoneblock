#include <assert.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "rtp_linux.h"

static int receiver_socket;
static int packet_count;
static int valid_packet;
static volatile sig_atomic_t stop_requested;

static void *receiver_thread(void *unused)
{
    (void)unused;
    unsigned char packet[2048];
    struct sockaddr_in sender;
    socklen_t sender_length = sizeof(sender);
    ssize_t length = recvfrom(receiver_socket, packet, sizeof(packet), 0,
                              (struct sockaddr *)&sender, &sender_length);
    if (length == 172 && packet[0] == 0x80 && packet[1] == 8
            && packet[2] != 0) {
        valid_packet = 1;
        packet_count++;
    }
    stop_requested = 1;
    return NULL;
}

int main(void)
{
    const char *path = "/tmp/pb-rtp-test.alaw";
    FILE *audio = fopen(path, "wb");
    assert(audio != NULL);
    unsigned char samples[160];
    memset(samples, 0xd5, sizeof(samples));
    assert(fwrite(samples, 1, sizeof(samples), audio) == sizeof(samples));
    fclose(audio);

    receiver_socket = socket(AF_INET, SOCK_DGRAM, 0);
    assert(receiver_socket >= 0);
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
        .sin_port = 0,
    };
    assert(bind(receiver_socket, (struct sockaddr *)&address, sizeof(address)) == 0);
    socklen_t address_length = sizeof(address);
    assert(getsockname(receiver_socket, (struct sockaddr *)&address, &address_length) == 0);
    pthread_t thread;
    assert(pthread_create(&thread, NULL, receiver_thread, NULL) == 0);
    assert(pb_linux_rtp_stream_alaw("127.0.0.1", ntohs(address.sin_port),
                                    path, 0, &stop_requested) == 0);
    pthread_join(thread, NULL);
    assert(packet_count == 1);
    assert(valid_packet);
    close(receiver_socket);
    remove(path);
    puts("test_rtp_linux: all tests passed");
    return 0;
}