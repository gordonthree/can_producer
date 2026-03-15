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
    rt->period_ms       = ((msg->data[MSG_DATA_5] << BYTE_SHIFT) & BYTE_MASK) |
                           (msg->data[MSG_DATA_6] & BYTE_MASK); 
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
    sub->submod_flags |= SUBMOD_FLAG_DIRTY;

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

/* ============================================================================
 *  PLATFORM-AGNOSTIC INGESTION API
 * ============================================================================
 *
 * These functions update the runtime snapshot for each sub-module without
 * requiring knowledge of hardware details or producer configuration. They are
 * called by platform-specific code whenever a new value is sampled or written.
 *
 * Each function:
 *   - Validates the submodule index
 *   - Retrieves the runTime_t via producerGetRuntime()
 *   - Updates the appropriate runtime field
 *   - Updates the timestamp (last_change_ms)
 *
 * No CAN messages are sent here. No producer logic is executed here.
 * ProducerTick() handles publishing based on the updated snapshot.
 */

/**
 * @brief Ingest a digital input state into the runtime snapshot.
 *
 * @param node Pointer to the nodeInfo_t (unused but kept for symmetry)
 * @param idx  Sub-module index
 * @param state Logical digital state (0 or 1)
 * @param ts   Timestamp of update in milliseconds
 */
void nodeSetDigitalState(nodeInfo_t *node,
                         const uint8_t idx,
                         const uint8_t state,
                         const uint32_t ts)
{
    runTime_t *rt = producerGetRuntime(idx);
    if (!rt) return;

    rt->state          = state;  /**< Logical digital input state */
    rt->last_change_ms = ts;     /**< Timestamp of last update */
}


/**
 * @brief Ingest an ADC sample into the runtime snapshot.
 *
 * @param node Pointer to the nodeInfo_t (unused but kept for symmetry)
 * @param idx  Sub-module index
 * @param value Latest ADC sample value
 * @param ts   Timestamp of update in milliseconds
 */
void nodeSetAdcValue(nodeInfo_t *node,
                     const uint8_t idx,
                     const uint32_t value,
                     const uint32_t ts)
{
    runTime_t *rt = producerGetRuntime(idx);
    if (!rt) return;

    rt->adc_value      = value;  /**< Last sampled ADC value */
    rt->last_change_ms = ts;     /**< Timestamp of last update */
}


/**
 * @brief Ingest a hardware output state into the runtime snapshot.
 *
 * @param node Pointer to the nodeInfo_t (unused but kept for symmetry)
 * @param idx  Sub-module index
 * @param value Last value written to the hardware output pin
 * @param ts   Timestamp of update in milliseconds
 */
void nodeSetOutputState(nodeInfo_t *node,
                        const uint8_t idx,
                        const uint8_t value,
                        const uint32_t ts)
{
    runTime_t *rt = producerGetRuntime(idx);
    if (!rt) return;

    rt->last_hardware_output = value; /**< Last written hardware output */
    rt->last_change_ms       = ts;    /**< Timestamp of last update */
}


/**
 * @brief Generic ingestion for arbitrary producer values.
 *
 * Used when the producer kind does not map to a specific hardware field
 * (e.g., counters, timers, virtual values, or custom producer kinds).
 *
 * @param node Pointer to the nodeInfo_t (unused but kept for symmetry)
 * @param idx  Sub-module index
 * @param value Arbitrary value to store
 * @param ts   Timestamp of update in milliseconds
 */
void nodeIngestValue(nodeInfo_t *node,
                     const uint8_t idx,
                     const uint32_t value,
                     const uint32_t ts)
{
    runTime_t *rt = producerGetRuntime(idx);
    if (!rt) return;

    rt->last_published_value = value; /**< Store generic value */
    rt->last_change_ms       = ts;    /**< Timestamp of last update */
}


/**
 * @brief Periodic producer execution. Called by firmware with a timestamp.
 *
 * @param ts Current timestamp in milliseconds (firmware-provided)
 */
producer_event_t producerTick(const uint32_t ts)
{
    producer_event_t evt = {0}; /**< Zero out the return event */

    const subMods = nodeGetSubmoduleCount();

    for (uint8_t i = 0; i < subMods; i++) {

        subModule_t *sub = nodeGetSubmodule(i);
        if (!sub) continue;

        runTime_t *rt = producerGetRuntime(i);
        if (!rt) continue;


        /* Producer disabled? */
        if (!(sub->producer_flags & PRODUCER_FLAG_ENABLED))
            continue;

        /* Period disabled? */
        if (rt->period_ms == PRODUCER_PUBLISH_DISABLED)
            continue;

        /* Time to publish? */
        if (ts - lastProducerTick[i] < rt->period_ms)
            continue;


        /* Select value based on valueSource */
        uint32_t value = 0;
        switch (rt->valueSource) {
            case VALUE_SRC_STATE:            value = rt->state; break;
            case VALUE_SRC_ADC:              value = rt->adc_value; break;
            case VALUE_SRC_HW_OUTPUT:        value = rt->last_hardware_output; break;
            default:                         continue;
        }

        /* 
         * Change-only logic 
         * If the CHANGE_ONLY flag is set, and the current value equals the last published value, do not publish 
         */
        if ((sub->producer_flags & PRODUCER_FLAG_CHANGE_ONLY) &&
            (value == rt->last_published_value))
        {
            continue;
        }

        /* Update last tick */
        lastProducerTick[i] = ts;

        /* Setup event to return */
        evt.ready                = true;   /**< Ready to publish */
        evt.sub_idx              = i;      /**< Sub-module index */
        evt.value                = value;  /**< Value to publish */
        rt->last_published_value = value;  /**< Update last published */

        /** return the value to publish */
        return evt;
    } /* end for */

    /** return not ready */ 
    return evt; 

}

#ifdef __cplusplus
}
#endif