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

    const uint8_t sub_cnt = nodeGetSubModuleCnt();
    uint8_t idx = msg->data[MSG_DATA_4]; /* data byte 4 is the sub-mod index value */
    if (idx >= MAX_SUB_MODULES || idx >= sub_cnt)
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

    memset(rt, 0, sizeof(runTime_t)); /* clear the producer runtime state */
    sub->submod_flags  |= SUBMOD_FLAG_DIRTY; /* mark the sub-module as dirty so main saves it to NVS */
}

/** Reset single producer to default */
void producerDefaultSingle(const uint8_t sub_idx)
{
    subModule_t *sub = nodeGetSubModule(sub_idx);
    runTime_t   *rt  = nodeGetRuntime(sub_idx);

    memset(rt, 0, sizeof(runTime_t)); /* clear the producer runtime state */
    rt->period_ms       = PRODUCER_RATEMS_1HZ;
    rt->kind            = PRODUCER_KIND_NONE;
    rt->valueSource     = VALUE_SRC_NONE;
    sub->producer_flags = PRODUCER_FLAG_NONE;
    sub->submod_flags  |= SUBMOD_FLAG_DIRTY; /* mark the sub-module as dirty so main saves it to NVS */
}

/** Reset all producers to default state */
void producerDefaultAll(void)
{
    const uint8_t sub_cnt = nodeGetSubModuleCnt();

    for (uint8_t i = 0; i < sub_cnt; i++) {
        producerDefaultSingle(i);
    }
}


void producerEnable(const uint8_t sub_idx)
{
    if (sub_idx >= MAX_SUB_MODULES)
        return;

    subModule_t *sub = nodeGetSubModule(sub_idx);
    sub->producer_flags |= PRODUCER_FLAG_ENABLED;
    sub->submod_flags |= SUBMOD_FLAG_DIRTY;
}

void producerDisable(const uint8_t sub_idx)
{
    if (sub_idx >= MAX_SUB_MODULES)
        return;
    
    subModule_t *sub = nodeGetSubModule(sub_idx);
    sub->producer_flags &= ~PRODUCER_FLAG_ENABLED;
    sub->submod_flags |= SUBMOD_FLAG_DIRTY;

}

void producerToggle(const uint8_t sub_idx)
{
    if (sub_idx >= MAX_SUB_MODULES)
        return;

    subModule_t *sub = nodeGetSubModule(sub_idx);
    sub->producer_flags ^= PRODUCER_FLAG_ENABLED;
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