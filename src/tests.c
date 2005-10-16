#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "proto.h"
#include "privkey.h"
#include "message.h"

#define ALICE "oneeyedian"
#define BOB "otr4ian"
#define PROTO "prpl-oscar"

static OtrlPolicy ALICEPOLICY = OTRL_POLICY_DEFAULT &~ OTRL_POLICY_ALLOW_V1;
static OtrlPolicy BOBPOLICY = OTRL_POLICY_DEFAULT;

void receiving(const char *from, const char *to, const char *msg);

typedef struct s_node {
    struct s_node *next;
    char *from, *to, *msg;
} MsgNode;

static MsgNode *noderoot = NULL;
static MsgNode **nodeend = &noderoot;

static void dispatch(void)
{
    while(noderoot) {
	MsgNode *node = noderoot;

	receiving(node->from, node->to, node->msg);
	free(node->from);
	free(node->to);
	free(node->msg);
	noderoot = node->next;
	free(node);
	if (noderoot == NULL) nodeend = &noderoot;
    }
}

static void inject(const char *from, const char *to, const char *msg)
{
    MsgNode *node = malloc(sizeof(*node));
    node->from = strdup(from);
    node->to = strdup(to);
    node->msg = strdup(msg);
    node->next = NULL;
    *nodeend = node;
    nodeend = &(node->next);
    printf("[%s->%s: %s]\n\n", from, to, msg);
}

static OtrlPolicy op_policy(void *opdata, ConnContext *context)
{
    if (!strcmp(context->accountname, ALICE)) return ALICEPOLICY;
    if (!strcmp(context->accountname, BOB)) return BOBPOLICY;
    return OTRL_POLICY_DEFAULT;
}

static void op_inject(void *opdata, const char *accountname,
	const char *protocol, const char *recipient, const char *message)
{
    inject(accountname, recipient, message);
}

static void op_notify(void *opdata, OtrlNotifyLevel level,
	    const char *accountname, const char *protocol,
	    const char *username, const char *title,
	    const char *primary, const char *secondary)
{
}

static int op_display_otr_message(void *opdata, const char *accountname,
	    const char *protocol, const char *username, const char *msg)
{
    return -1;
}

static void op_gone_secure(void *opdata, ConnContext *context,
	    int protocol_version)
{
    printf("SECURE (%d): %s / %s\n\n", protocol_version, context->accountname,
	    context->username);
}

static void op_gone_insecure(void *opdata, ConnContext *context)
{
    printf("INSECURE: %s / %s\n\n", context->accountname, context->username);
}

static void op_still_secure(void *opdata, ConnContext *context, int is_reply,
	    int protocol_version)
{
    printf("REFRESH (%d/%d): %s / %s\n\n", is_reply, protocol_version,
	    context->accountname, context->username);
}

static OtrlMessageAppOps ops = {
    op_policy,
    NULL,
    NULL,
    op_inject,
    op_notify,
    op_display_otr_message,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    op_gone_secure,
    op_gone_insecure,
    op_still_secure,
    NULL
};

static OtrlUserState us;

void receiving(const char *from, const char *to, const char *msg)
{
    int ignore;
    char *newmsg;
    OtrlTLV *tlvs;

    ignore = otrl_message_receiving(us, &ops, NULL, to, PROTO, from, msg,
	    &newmsg, &tlvs, NULL, NULL);

    if (!ignore) {
	printf("%s> %s\n\n", from, newmsg ? newmsg : msg);
    }

    otrl_message_free(newmsg);
    otrl_tlv_free(tlvs);
}

static void sending(const char *from, const char *to, const char *msg)
{
    gcry_error_t err;
    OtrlTLV *tlvs = NULL;
    char *newmsg;

    err = otrl_message_sending(us, &ops, NULL, from, PROTO, to, msg,
	    tlvs, &newmsg, NULL, NULL);

    if (!err) {
	inject(from, to, newmsg ? newmsg : msg);
    }

    otrl_message_free(newmsg);
    otrl_tlv_free(tlvs);
}

void test(int vers, int both)
{
    printf("\n\n*** Testing version %d, %s ***\n\n", vers,
	    both ? "simultaneous start" : "Alice start");

    otrl_context_forget_all(us);
    ALICEPOLICY = vers == 1 ? (OTRL_POLICY_DEFAULT &~ OTRL_POLICY_ALLOW_V2) :
	OTRL_POLICY_DEFAULT;
    sending(ALICE, BOB, "?OTR?");
    if (both) {
	sending(BOB, ALICE, "?OTR?");
    }
    dispatch();
    sending(ALICE, BOB, "Hi there");
    dispatch();
}

static void test_unreadable(void)
{
    ConnContext *bobcontext;

    printf("\n\n*** Testing Bob receiving unreadable messages from "
	    "Alice ***\n\n");

    bobcontext = otrl_context_find(us, ALICE, BOB, PROTO, 0, NULL, NULL, NULL);
    otrl_context_force_plaintext(bobcontext);
    sending(ALICE, BOB, "unreadable text");
    dispatch();

}

int main(int argc, char **argv)
{
    OTRL_INIT;
    us = otrl_userstate_create();

    otrl_privkey_read(us, "/home/iang/.gaim/otr.private_key");

    test(1,0);
    test(2,0);
    test(1,1);
    test(2,1);
    test_unreadable();


    otrl_userstate_free(us);

    return 0;
}