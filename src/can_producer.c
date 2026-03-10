#include "can_producer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== GLOBALS ===== */

/** Producer tick counter array */
uint32_t lastProducerTick[MAX_SUB_MODULES] = {0}; 
extern bool g_producerSaveRequested = false;
extern bool g_producerLoadRequested = false;

/* ===== FUNCTIONS ===== */

/* ============================================================================
 *  PRODUCER CONFIG HANDLERS
 * ========================================================================== */

void handleProducerCfg(const can_msg_t *msg, const subModule_t *sub)
{
    if (msg->data_length_code < CFG_PRODUCER_CFG_DLC)
        return;

    uint8_t idx = msg->data[4];
    if (idx >= MAX_SUB_MODULES)
        return;

    sub->producer_cfg.rate_hz  = msg->data[5]; // g_producerCfg[idx].rate_hz  = msg->data[5];
    sub->producer_cfg.flags    = msg->data[6]; // g_producerCfg[idx].flags    = msg->data[6];
    sub->producer_cfg.reserved = msg->data[7]; // g_producerCfg[idx].reserved = msg->data[7];

    printf("ProducerCfg: sub %u rate %u flags 0x%02X\n",
           idx,
           sub.producer_cfg.rate_hz,
           sub.producer_cfg.flags);
}

void handleProducerPurge(const can_msg_t *msg, const subModule_t *sub)
{
    (void)msg;
    sub->producer.kind = PRODUCER_KIND_NONE; /* reset producer kind */
}

void handleProducerDefaults(const can_msg_t *msg, const subModule_t *sub)
{
    (void)msg;
    // TODO: Read "kind" from message or nvs, reset producer kind?
    // memset(g_producerCfg, 0, sizeof(g_producerCfg));
}

void handleProducerApply(void)
{
    /* No-op: config is live immediately */
}

void handleReqProducerCfg(const can_msg_t *msg, const subModule_t *sub)
{
    (void)msg;
    /* The caller (ESP32) will send RESP_PRODUCER_CFG_ID frames.
       This module is platform-agnostic, so we do nothing here. */
}

void handleProducerWriteNVS(void)
{
    // TODO: create function in main.cpp
    node_saveProducerCfgNvs();
}

#ifdef __cplusplus
}
#endif