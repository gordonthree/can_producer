#include "can_producer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ===== GLOBALS ===== */

/** Producer tick counter array */
uint32_t lastProducerTick[MAX_SUB_MODULES] = {0}; 
bool g_producerSaveRequested = false;
bool g_producerLoadRequested = false;

/* ===== FUNCTIONS ===== */

/* ============================================================================
 *  PRODUCER CONFIG HANDLERS
 * ========================================================================== */

void handleProducerCfg(const can_msg_t *msg)
{
    if (msg->data_length_code < CFG_PRODUCER_CFG_DLC) /* malformed message, bail out*/
        return;

    uint8_t idx = msg->data[MSG_DATA_4]; /* data byte 4 is the sub-mod index value */
    if (idx >= MAX_SUB_MODULES || idx >= nodeGetSubModuleCnt())
        return;
    
    /** Load data from the main firmware state */
    runTime_t   *rt  = nodeGetRuntime(idx);
    if (!rt) 
        return;

    subModule_t *sub = nodeGetSubModule(idx);
    if (!sub)
        return;

    /** Pack two bytes for the 16-bit period */
    rt->period_ms       = ((msg->data[MSG_DATA_5] << 8) & 0xFF) |
                           (msg->data[MSG_DATA_6] & 0xFF); 
    sub->producer_flags = msg->data[MSG_DATA_7];

    /** Request the firmware save data to NVS */
    sub->submod_flags  |= SUBMOD_FLAG_DIRTY; /* mark the sub-module as dirty so main saves it to NVS */
}

void producerPurgeAll(void)
{
    const uint8_t sub_cnt = nodeGetSubModuleCnt();

    for (uint8_t i = 0; i < sub_cnt; i++) {
        producerPurgeSingle(i);
    }
}

void producerPurgeSingle(const uint8_t sub_idx)
{
    if (sub_idx >= MAX_SUB_MODULES)
        return;
    subModule_t *sub = nodeGetSubModule(sub_idx);
    runTime_t   *rt  = nodeGetRuntime(sub_idx);

    memset(rt, 0, sizeof(runTime_t)); /* clear the producer runtime state 
    sub->submod_flags  |= SUBMOD_FLAG_DIRTY; /* mark the sub-module as dirty so main saves it to NVS */}
}

void producerSetDefault(const uint8_t sub_idx)
{
    subModule_t *sub = nodeGetSubModule(sub_idx);
    runTime_t   *rt  = nodeGetRuntime(sub_idx);

    memset(rt, 0, sizeof(runTime_t)); /* clear the producer runtime state */
    rt->period_ms = PRODUCER_RATEMS_1HZ;
    rt->kind      = PRODUCER_KIND_NONE

        cfg->flags   = 0;
        cfg->reserved = 0;
    }
    g_producerSaveRequested = true;
}

void producerDelete(const uint8_t sub_idx)
{
    if (sub_idx >= MAX_SUB_MODULES)
        return;

    // Clear producer personality
    producer_t *p = producerGetState(sub_idx);
    memset(p, 0, sizeof(producer_t));

    // Clear producer configuration
    producer_cfg_t *cfg = producerGetConfig(sub_idx);
    memset(cfg, 0, sizeof(producer_cfg_t));

    // Clear submodule flags related to producer behavior
    producerSetFlags(sub_idx, 0);

    // Ask firmware to persist the change
    g_producerSaveRequested = true;
}

void producerEnable(const uint8_t sub_idx)
{
    if (sub_idx >= MAX_SUB_MODULES)
        return;

    producer_cfg_t *cfg = producerGetConfig(sub_idx);
    cfg->flags |= PRODUCER_FLAG_ENABLED;

    g_producerSaveRequested = true;
}

void producerDisable(const uint8_t sub_idx)
{
    if (sub_idx >= MAX_SUB_MODULES)
        return;

    producer_cfg_t *cfg = producerGetConfig(sub_idx);
    cfg->flags &= ~PRODUCER_FLAG_ENABLED;

    g_producerSaveRequested = true;
}

void producerToggle(const uint8_t sub_idx)
{
    if (sub_idx >= MAX_SUB_MODULES)
        return;

    producer_cfg_t *cfg = producerGetConfig(sub_idx);
    cfg->flags ^= PRODUCER_FLAG_ENABLED;

    g_producerSaveRequested = true;
}

void requestProducerLoad(void)
{
    /** set a flag and the main loop will handle the producer load request */
    g_producerLoadRequested = true; 
}

void requestProducerSave(void)
{
    /** set a flag and the main loop will handle the producer save request */
    g_producerSaveRequested = true;
}

#ifdef __cplusplus
}
#endif