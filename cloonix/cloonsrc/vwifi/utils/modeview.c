/*
 * Copyright (c) 2020 Petro Bagrii
 * https://onethinglab.com
 *
 * View list of wireless interfaces with supported modes.
 *
 * Compile:
 * gcc modeview.c -o modeview -lnl-genl-3 -lnl-3 -I/usr/include/libnl3 -g -Wall -O3
*/

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <linux/nl80211.h>
#include <linux/if.h>
#include <stdbool.h>

#define UNUSED(x) (void)(x)

struct netif
{
    uint32_t phy_id;
    uint32_t supported_iftypes;
    char name[IFNAMSIZ];
};

struct dev_list
{
    struct netif dev;
    struct dev_list* next;
};

struct message
{
    struct nl_msg* msg;
    struct message* next;
};

struct state
{
    struct nl_sock* socket;
    struct nl_cb* callback;
    struct message* messages;
    struct dev_list* devices;
};

static const char* iftype_names[NL80211_IFTYPE_MAX + 1] = {
    "Unspecified",
    "IBSS",
    "Managed",
    "AP",
    "AP/VLAN",
    "WDS",
    "Monitor",
    "Mesh point",
    "P2P-client",
    "P2P-GO",
    "P2P-device",
    "Outside context of a BSS",
    "NAN",
};


struct state* init_state()
{
    struct state* state = malloc(sizeof(struct state));
    if (state)
    {
        state->socket = NULL;
        state->callback = NULL;
        state->messages = NULL;
        state->devices = NULL;
    }
    return state;
}

void cleanup(struct state* state)
{
    if (state->socket)
    {
        nl_socket_free(state->socket);
        state->socket = NULL;
    }
    if (state->callback)
    {
        nl_cb_put(state->callback);
        state->callback = NULL;

    }
    if (state->messages)
    {
        for (struct message* message = state->messages; message;)
        {
            nlmsg_free(message->msg);
            struct message* msg = message;
            message = message->next;
            free(msg);
        }
        state->messages = NULL;
    }
    if (state->devices)
    {
        for (struct dev_list* current = state->devices; current;)
        {
            struct dev_list* dev = current;
            current = current->next;
            free(dev);
        }
    }

}

struct nl_msg* alloc_message(struct state* state)
{
    struct message** current = &state->messages;
    while(*current)
    {
        *current = (*current)->next;
    }
    *current = malloc(sizeof(struct message));

    if (!*current)
    {
        return NULL;
    }

    (*current)->next = NULL;
    (*current)->msg = nlmsg_alloc();
    if (!(*current)->msg)
    {
        free(*current);
        return NULL;
    }

    return (*current)->msg;
}


int retrieve_iftypes_callback(struct nl_msg* msg, void* arg)
{
    struct dev_list** devices = arg;
    struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
    struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];

    int result = 0;
    if ((result = nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
              genlmsg_attrlen(gnlh, 0), NULL) < 0))
    {
        fprintf(stderr, "Cannot parse attributes due to error: %d", result);
        return NL_STOP;
    }

    uint32_t phy_id = 0;
    if (tb_msg[NL80211_ATTR_WIPHY])
    {
        phy_id = nla_get_u32(tb_msg[NL80211_ATTR_WIPHY]);
    }

    struct dev_list* current = *devices;
    while(current && current->dev.phy_id != phy_id)
    {
        current = current->next;
    }

    if (!current)
    {
        current = malloc(sizeof(struct dev_list));
        if (!current)
        {
            return NL_STOP;
        }
        current->dev.phy_id = phy_id;
        current->next = NULL;
        current->dev.supported_iftypes = 0;
        if (!(*devices))
        {
            *devices = current;
        }
        else
        {
            struct dev_list* dev = *devices;
            while(dev->next)
            {
                dev = dev->next;
            }
            dev->next = current;
        }
    }

    if (tb_msg[NL80211_ATTR_IFNAME]) {
        strncpy(current->dev.name, nla_get_string(tb_msg[NL80211_ATTR_IFNAME]),
                IFNAMSIZ);
    }

    if (tb_msg[NL80211_ATTR_SUPPORTED_IFTYPES]) {
        struct nlattr* nested_attr = NULL;
        int remaining = 0;
        nla_for_each_nested(nested_attr, tb_msg[NL80211_ATTR_SUPPORTED_IFTYPES], remaining)
        {
            current->dev.supported_iftypes |= 1 << (nla_type(nested_attr));
        }
    }

    return NL_OK;
}

int finish_callback(struct nl_msg* msg, void* arg) {
    UNUSED(msg);

    bool* finished = arg;
    *finished = true;

    return NL_SKIP;
}

int main()
{
    struct state* state = init_state();
    if (!state)
    {
        fprintf(stderr, "Cannot allocate state");
        return EXIT_FAILURE;
    }

    state->socket = nl_socket_alloc();
    if (!state->socket)
    {
        fprintf(stderr, "Cannot create socket");
        cleanup(state);
        return EXIT_FAILURE;
    }

    if (genl_connect(state->socket) < 0)
    {
        fprintf(stderr, "Failed to connect to netlink socket.\n");
        cleanup(state);
        return EXIT_FAILURE;
    }

    int family_id = genl_ctrl_resolve(state->socket, "nl80211");
    if (family_id < 0) {
        fprintf(stderr, "Nl80211 interface not found.\n");
        cleanup(state);
        return EXIT_FAILURE;
    }

    state->callback = nl_cb_alloc(NL_CB_DEFAULT);
    if (!state->callback) {
     fprintf(stderr, "Failed to allocate netlink callback.\n");
     cleanup(state);
     return EXIT_FAILURE;
    }

    bool message_received = false;
    nl_cb_set(state->callback, NL_CB_VALID , NL_CB_CUSTOM,
              retrieve_iftypes_callback, &state->devices);
    nl_cb_set(state->callback, NL_CB_FINISH, NL_CB_CUSTOM,
              finish_callback, &message_received);

    uint8_t commands[] = { NL80211_CMD_GET_INTERFACE, NL80211_CMD_GET_WIPHY };
    for (unsigned cmd = 0; cmd < sizeof(commands) / sizeof(uint8_t); cmd++)
    {
        struct nl_msg* msg = alloc_message(state);
        if (!msg)
        {
            fprintf(stderr, "Cannot allocate netlink message");
            cleanup(state);
            return EXIT_FAILURE;
        }
        genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, family_id, 0,
                    NLM_F_DUMP, commands[cmd], 0);
        nl_send_auto(state->socket, msg);
        message_received = false;
        while (!message_received)
        {
            int result = 0;
            if ((result = nl_recvmsgs(state->socket, state->callback)) < 0)
            {
                fprintf(stderr, "Error occurred while receiving message: %d", result);
                cleanup(state);
                return EXIT_FAILURE;
            }
        }
    }

    printf("List of wireless interfaces:\n\n");

    for(struct dev_list* current = state->devices; current; current = current->next)
    {
        printf("Interface %s | List of supported modes:\n", current->dev.name);
        for (enum nl80211_iftype iftype = NL80211_IFTYPE_UNSPECIFIED;
             iftype <= NUM_NL80211_IFTYPES; iftype++)
        {
            if (current->dev.supported_iftypes & (1 << iftype))
            {
                printf("%s\n",  iftype_names[iftype]);
            }
        }
        printf("\n");
    }

    cleanup(state);

    return EXIT_SUCCESS;
}
