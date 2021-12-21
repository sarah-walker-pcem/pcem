#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "nethandler.h"

#include "ibm.h"
#include "device.h"

#include "ne2000.h"
#include "timer.h"

int network_card_current = 0;
static int network_card_last = 0;

typedef struct
{
        char name[32];
        char internal_name[24];
        device_t *device;
} NETWORK_CARD;

static NETWORK_CARD network_cards[] =
{
        {"None",                  "",          NULL},
        {"Novell NE2000",         "ne2000",    &ne2000_device},
        {"Realtek RTL8029AS",     "rtl8029as", &rtl8029as_device},
        {"", "", NULL}
};

int network_card_available(int card)
{
        if (network_cards[card].device)
                return device_available(network_cards[card].device);

        return 1;
}

char *network_card_getname(int card)
{
        return network_cards[card].name;
}

device_t *network_card_getdevice(int card)
{
        return network_cards[card].device;
}

int network_card_has_config(int card)
{
        if (!network_cards[card].device)
                return 0;
        return network_cards[card].device->config ? 1 : 0;
}

char *network_card_get_internal_name(int card)
{
        return network_cards[card].internal_name;
}

int network_card_get_from_internal_name(char *s)
{
	int c = 0;
	
	while (strlen(network_cards[c].name))
	{
		if (!strcmp(network_cards[c].internal_name, s))
			return c;
		c++;
	}
	
	return 0;
}

void network_card_init()
{
        if (network_cards[network_card_current].device)
                device_add(network_cards[network_card_current].device);
        network_card_last = network_card_current;
}

static struct
{
        void (*poller)(void *p);
        void *priv;
} vlan_handlers[8];

static int vlan_handlers_num;

static pc_timer_t vlan_poller_timer;

void vlan_handler(void (*poller)(void *p), void *p)
//void vlan_handler(int (*can_receive)(void *p), void (*receive)(void *p, const uint8_t *buf, int size), void *p)
{
      /*  vlan_handlers[vlan_handlers_num].can_receive = can_receive; */
        vlan_handlers[vlan_handlers_num].poller = poller;
        vlan_handlers[vlan_handlers_num].priv = p;
        vlan_handlers_num++;
}

void vlan_poller(void *priv)
{
        int c;

        timer_advance_u64(&vlan_poller_timer, (uint64_t)((double)TIMER_USEC * (1000000.0 / 8.0 / 1500.0)));

        for (c = 0; c < vlan_handlers_num; c++)
                vlan_handlers[c].poller(vlan_handlers[c].priv);
}

void vlan_reset()
{
        timer_add(&vlan_poller_timer, vlan_poller, NULL, 1);

        vlan_handlers_num = 0;
}
