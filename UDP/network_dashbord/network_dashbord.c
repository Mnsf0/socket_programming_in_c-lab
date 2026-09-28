#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>

// I know using ip link here isn't best choice, but the reason behind it,
// this is my first time inject a bash script inside my code
// and I want to give it a shot and see how it gonna go. 


static const char *IF_SCRIPT =
    "ip -o link show" // ip link show: to list network cards, -o to make all the output start at the head of the line
    " | awk -F': ' '{print $2}'" // getting second field, which hold the name of netwrok card
    " | cut -d@ -f1"; // remove any '@' suffix 

static const char *PORT_SCRIPT =
    "ss -tuln" // list all listening ports on the machine 
    " | tail -n +2" // skip first two lines, which have the headers
    " | awk '{print $1, $5}'" // get field 1 and 5
    " | sort -u"; // sort it by alphabetically

//here we tying to get the subnet for IP 
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
    FILE *fp = popen(IF_SCRIPT, "r");
    if (fp == NULL) {
        perror("popen");
        return -1;
    }

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        pclose(fp);
        return -1;
    }

    char line[64];
    int count = 0;

    printf("%-16s %-20s %s\n", "INTERFACE", "IPv4/PREFIX", "MAC");

    while (fgets(line, sizeof line, fp) != NULL) {
        line[strcspn(line, "\n")] = '\0';

        if (line[0] == '\0' || strlen(line) >= IFNAMSIZ)
            continue;

    // I declared them inside loop to set these answer for each network card
        struct ifreq ifr;
        char ip[INET_ADDRSTRLEN]; // for ip, ex: 192.168.10.1
        char ipcidr[INET_ADDRSTRLEN + 4] = "NULL"; // finall output 192.168.10.1/24
        char mac[18] = "NULL"; // to print mac address

        memset(&ifr, 0, sizeof ifr);
        snprintf(ifr.ifr_name, sizeof ifr.ifr_name, "%s", line); // get the name of the network card into ifr

        if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) { // get IP address
            struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
            inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof ip);// safe in readable way

            memset(&ifr, 0, sizeof ifr);
            snprintf(ifr.ifr_name, sizeof ifr.ifr_name, "%s", line);

            if (ioctl(fd, SIOCGIFNETMASK, &ifr) == 0) { // getting subnet mask 
                struct sockaddr_in *nm = (struct sockaddr_in *)&ifr.ifr_netmask;
                snprintf(ipcidr, sizeof ipcidr, "%s/%d", ip, mask_to_prefix(nm->sin_addr));
            } else {
                snprintf(ipcidr, sizeof ipcidr, "%s", ip);
            }
        }

        memset(&ifr, 0, sizeof ifr);
        snprintf(ifr.ifr_name, sizeof ifr.ifr_name, "%s", line);

      // getting MAC address 
        if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0 &&
            ifr.ifr_hwaddr.sa_family == ARPHRD_ETHER) {
            unsigned char *m = (unsigned char *)ifr.ifr_hwaddr.sa_data;
            snprintf(mac, sizeof mac, "%02x:%02x:%02x:%02x:%02x:%02x",
                     m[0], m[1], m[2], m[3], m[4], m[5]);
        }

        printf("%-16s %-20s %s\n", line, ipcidr, mac);
        count++;
    }

    if (count == 0)
        fprintf(stderr, "No interfaces found\n");

    close(fd);
    pclose(fp);
    return 0;
}

static int print_ports(void) {
    FILE *fp = popen(PORT_SCRIPT, "r");
    if (fp == NULL) {
        perror("popen");
        return -1;
    }

    char line[256];
    int count = 0;

    printf("\n%-10s %-38s %s\n", "PROTOCOL", "LISTEN ADDRESS", "PORT");

    while (fgets(line, sizeof line, fp) != NULL) {
        char proto[16];
        char addr[128];

        if (sscanf(line, "%15s %127s", proto, addr) != 2)
            continue;

        char *colon = strrchr(addr, ':');
        if (colon == NULL)
            continue;

        *colon = '\0';
        const char *port = colon + 1;
        const char *address = addr[0] ? addr : "*";

        printf("%-10s %-38s %s\n", proto, address, port);
        count++;
    }

    if (count == 0)
        printf("(no listening ports found)\n");

    pclose(fp);
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
