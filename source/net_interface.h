#ifndef NET_INTERFACE_H_
#define NET_INTERFACE_H_

#include <net/if.h>
#include <arpa/inet.h>

typedef struct {
    char name[IF_NAMESIZE];
    char ip[INET_ADDRSTRLEN];
    char mac[18];
    char url[INET_ADDRSTRLEN + 14]; // allow for "https://" and ":80801"
    char localurl[10 + 14]; // allow for "https://" "localhost" ":80801"
    char rawmac[18];
    bool isLocalurlTLS;
} net_iface;

/*
struct netinfo {
    char ifname[IFNAMSIZ];
    char ip[INET_ADDRSTRLEN];
    char mac[18];
};
*/

const char *get_ip_address_of_nameserver();
const net_iface *get_first_valid_interface();
//char *generate_mqtt_id(char *buf, int len);
char *generate_mqtt_id();

#endif //NET_INTERFACE_H_