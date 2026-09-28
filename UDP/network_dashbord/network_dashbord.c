#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netpacket/packet.h>
#include <arpa/inet.h>



static int mask_to_prefix(struct in_addr mask) {
    uint32_t m = ntohl(mask.s_addr);
    int bits = 0;
    while (m & 0x80000000u) {
        bits++;
        m <<= 1;
    }
    return bits;
}

static int print_interfaces(void) {
    struct ifaddrs *list, *ifa;

    if (getifaddrs(&list) < 0) {
        perror("getifaddrs");
        return -1;
    }

    int count = 0;
    printf("%-16s %-20s %s\n", "INTERFACE", "IPv4/PREFIX", "MAC");

    // one AF_PACKET entry exists per interface -> use it as the interface list
    for (ifa = list; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_PACKET)
            continue;

        const char *name = ifa->ifa_name;
        if (name[0] == '\0' || strlen(name) >= IFNAMSIZ)
            continue;

        char ip[INET_ADDRSTRLEN];
        char ipcidr[INET_ADDRSTRLEN + 4] = "NULL";
        char mac[18] = "NULL";

        // find the IPv4 entry of the same interface
        for (struct ifaddrs *a = list; a != NULL; a = a->ifa_next) {
            if (a->ifa_addr == NULL || a->ifa_addr->sa_family != AF_INET)
                continue;
            if (strcmp(a->ifa_name, name) != 0)
                continue;

            struct sockaddr_in *sin = (struct sockaddr_in *)a->ifa_addr;
            inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof ip);

            if (a->ifa_netmask != NULL) {
                struct sockaddr_in *nm = (struct sockaddr_in *)a->ifa_netmask;
                snprintf(ipcidr, sizeof ipcidr, "%s/%d", ip, mask_to_prefix(nm->sin_addr));
            } else {
                snprintf(ipcidr, sizeof ipcidr, "%s", ip);
            }
            break;
        }

        struct sockaddr_ll *sll = (struct sockaddr_ll *)ifa->ifa_addr;
        if (sll->sll_hatype == ARPHRD_ETHER) {
            unsigned char *m = sll->sll_addr;
            snprintf(mac, sizeof mac, "%02x:%02x:%02x:%02x:%02x:%02x",
                     m[0], m[1], m[2], m[3], m[4], m[5]);
        }

        printf("%-16s %-20s %s\n", name, ipcidr, mac);
        count++;
    }

    if (count == 0)
        fprintf(stderr, "No interfaces found\n");

    freeifaddrs(list);
    return 0;
}

#define MAX_PORTS 1024
static char entries[MAX_PORTS][192];
static int n_entries = 0;

static int cmp(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

// state: 0x0A = TCP LISTEN, 0x07 = UDP unconnected (what ss shows as UNCONN)
static void read_proc(const char *path, const char *proto, int v6, unsigned want_state) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        return;

    char line[512];
    fgets(line, sizeof line, fp); // skip header

    while (fgets(line, sizeof line, fp) != NULL && n_entries < MAX_PORTS) {
        char hex[64];
        unsigned port, state;
        char addr[INET6_ADDRSTRLEN + 2];

        if (sscanf(line, " %*d: %63[0-9A-Fa-f]:%x %*[0-9A-Fa-f]:%*x %x",
                   hex, &port, &state) != 3)
            continue;
        if (state != want_state)
            continue;

        if (!v6) {
            struct in_addr a;
            a.s_addr = (uint32_t)strtoul(hex, NULL, 16);
            inet_ntop(AF_INET, &a, addr, sizeof addr);
        } else {
            struct in6_addr a6;
            uint32_t w[4];
            if (sscanf(hex, "%8x%8x%8x%8x", &w[0], &w[1], &w[2], &w[3]) != 4)
                continue;
            memcpy(&a6, w, sizeof a6);
            char tmp[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &a6, tmp, sizeof tmp);
            snprintf(addr, sizeof addr, "[%s]", tmp);
        }

        snprintf(entries[n_entries++], sizeof entries[0], "%s %s %u", proto, addr, port);
    }
    fclose(fp);
}

static int print_ports(void) {
    read_proc("/proc/net/tcp",  "tcp", 0, 0x0A);
    read_proc("/proc/net/tcp6", "tcp", 1, 0x0A);
    read_proc("/proc/net/udp",  "udp", 0, 0x07);
    read_proc("/proc/net/udp6", "udp", 1, 0x07);

    qsort(entries, n_entries, sizeof entries[0], cmp); // sort

    printf("\n%-10s %-38s %s\n", "PROTOCOL", "LISTEN ADDRESS", "PORT");

    int count = 0;
    char prev[192] = "";
    for (int i = 0; i < n_entries; i++) {
        if (strcmp(entries[i], prev) == 0) // -u: drop duplicates
            continue;
        strcpy(prev, entries[i]);

        char proto[16], addr[128], port[16];
        if (sscanf(entries[i], "%15s %127s %15s", proto, addr, port) != 3)
            continue;

        printf("%-10s %-38s %s\n", proto, addr, port);
        count++;
    }

    if (count == 0)
        printf("(no listening ports found)\n");

    return 0;
}

int main(void) {
    char answer;
    int check_ports = 0;

    printf("Check open ports on this system? [y/N]: ");
    scanf("%c",&answer);

    if ((answer == 'y' || answer == 'Y'))
        check_ports = 1;

    printf("\n");

    if (print_interfaces() < 0)
        return 1;

    if (check_ports && print_ports() < 0)
        return 1;

    return 0;
}
