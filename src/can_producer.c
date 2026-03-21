#include "can_producer.h"
#include <freertos/task.h> /* for vTaskDelay */

#ifdef __cplusplus
extern "C" {
#endif

/* ===== GLOBALS ===== */

static const producerCallbacks_t *g_node = NULL;

/** Grant access to the producer callbacks from the main firmware */
void producerInit(const producerCallbacks_t *cb)
{
    g_node = cb;
}

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
    if (msg->data_length_code < CFG_PRODUCER_CFG_DLC)
        return;

    const uint8_t sub_cnt = g_node->getSubModuleCount();
    uint8_t idx = msg->data[MSG_DATA_4]; /* data byte 4 is the sub-mod index value */
    if (idx >= MAX_SUB_MODULES || idx >= sub_cnt)
        return;
    
    /** Load data from the main firmware state */
    runTime_t   *rt  = g_node->getRuntime(idx);
    if (!rt) 
        return;

    subModule_t *sub = g_node->getSubModule(idx);
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
    const uint8_t sub_cnt = g_node->getSubModuleCount();

    for (uint8_t i = 0; i < sub_cnt; i++) {
        producerPurgeSingle(i);
    }
}

void producerPurgeSingle(const uint8_t sub_idx)
{
    if (sub_idx >= MAX_SUB_MODULES)
        return;
    subModule_t *sub = g_node->getSubModule(sub_idx);
    runTime_t   *rt  = g_node->getRuntime(sub_idx);

    memset(rt, 0, sizeof(runTime_t)); /* clear the producer runtime state */
    sub->submod_flags  |= SUBMOD_FLAG_DIRTY; /* mark the sub-module as dirty so main saves it to NVS */
}

/** Reset single producer to default */
void producerDefaultSingle(const uint8_t sub_idx)
{
    subModule_t *sub = g_node->getSubModule(sub_idx);
    runTime_t   *rt  = g_node->getRuntime(sub_idx);

    memset(rt, 0, sizeof(runTime_t)); /* clear the producer runtime state */
    rt->period_ms       = PRODUCER_PUBLISH_DISABLED;
    rt->kind            = PRODUCER_KIND_NONE;
    sub->producer_flags = PRODUCER_FLAG_NONE;
    sub->submod_flags  |= SUBMOD_FLAG_DIRTY; /* mark the sub-module as dirty so main saves it to NVS */
}

/** Reset all producers to default state */
void producerDefaultAll(void)
{
    const uint8_t sub_cnt = g_node->getSubModuleCount();

    for (uint8_t i = 0; i < sub_cnt; i++) {
        producerDefaultSingle(i);
    }
}


void producerEnable(const uint8_t sub_idx)
{
    if (sub_idx >= MAX_SUB_MODULES)
        return;

    subModule_t *sub = g_node->getSubModule(sub_idx);
    sub->producer_flags |= PRODUCER_FLAG_ENABLED;
    sub->submod_flags |= SUBMOD_FLAG_DIRTY;
}

/**
 * @brief Disable a producer from publishing data.
 *
 * Clear the PRODUCER_FLAG_ENABLED bit in the producer flags of the sub-module at idx.
 * This will prevent the producer from publishing data according to its configuration.
 *
 * @param sub_idx Index of the sub-module to disable.
 */
void producerDisable(const uint8_t sub_idx)
{
    if (sub_idx >= MAX_SUB_MODULES)
        return;
    
    subModule_t *sub = g_node->getSubModule(sub_idx);
    sub->producer_flags &= ~PRODUCER_FLAG_ENABLED;
    sub->submod_flags |= SUBMOD_FLAG_DIRTY;

}

/**
 * @brief Toggle the enabled state of a producer.
 *
 * Toggle the PRODUCER_FLAG_ENABLED bit in the producer flags of the sub-module at idx.
 * This will enable or disable the producer from publishing data according to its configuration.
 *
 * @param sub_idx Index of the sub-module to toggle.
 */
void producerToggle(const uint8_t sub_idx)
{
    if (sub_idx >= MAX_SUB_MODULES)
        return;

    subModule_t *sub = g_node->getSubModule(sub_idx);
    sub->producer_flags ^= PRODUCER_FLAG_ENABLED;
    sub->submod_flags |= SUBMOD_FLAG_DIRTY;

}

/**
 * @brief Request the main loop to load the producer configuration.
 *
 * This function sets a flag which will be checked in the main loop.
 * If the flag is set, the main loop will load the producer configuration
 * from non-volatile storage and apply it to the runtime.
 */
void requestProducerLoad(void)
{
    /** set a flag and the main loop will handle the producer load request */
    g_producerLoadRequested = true; 
}

/**
 * @brief Request the main loop to save the producer configuration.
 *
 * This function sets a flag which will be checked in the main loop.
 * If the flag is set, the main loop will save the producer configuration
 * to non-volatile storage.
 */
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
 *   - Retrieves the runTime_t via nodeGetRuntime()
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
    runTime_t *rt = g_node->getRuntime(idx);
    if (!rt) return;

    rt->valueU32       = state;  /**< Logical digital input state */
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
    runTime_t *rt = g_node->getRuntime(idx);
    if (!rt) return;

    rt->valueU32       = value;  /**< Last sampled ADC value */
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
    runTime_t *rt = g_node->getRuntime(idx);
    if (!rt) return;

    rt->valueU32       = value; /**< Last written hardware output */
    rt->last_change_ms = ts;    /**< Timestamp of last update */
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
    runTime_t *rt = g_node->getRuntime(idx);
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
    evt.ready = false;


    if (!g_node ||
        !g_node->getSubModuleCount ||
        !g_node->getSubModule ||
        !g_node->getRuntime) 
        {
        evt.ready = false;
        evt.error = true;
        return evt;  // or return evt with ready = false
    }

    const uint8_t subMods = g_node->getSubModuleCount();
    static uint8_t prod_debug_flag = 0;

    for (uint8_t i = 0; i < subMods; i++) {

        subModule_t *sub = g_node->getSubModule(i);
        if (!sub) continue;

        runTime_t *rt = &sub->runTime;
        if (!rt) continue;

        // printf("[PRODLOOP] i=%u sub=%p flags=0x%02X period=%u\n",
        //     i,
        //     (void*)sub,
        //     sub ? sub->producer_flags : 0,
        //     sub ? sub->runTime.period_ms : 0);

        // vTaskDelay(100 / portTICK_PERIOD_MS);

        /* Producer disabled? */
        if (!(sub->producer_flags & PRODUCER_FLAG_ENABLED))
            continue;

        /* Period disabled? */
        if (rt->period_ms == PRODUCER_PUBLISH_DISABLED)
            continue;

        /* Time to publish? */

        static uint32_t lastDebugTs = 0;




        /* Retrieve value */
        uint32_t value = rt->valueU32; 

        /* 
         * Change-only logic 
         * If the CHANGE_ONLY flag is set, and the current value equals the last published value, do not publish 
         */
        // if ((sub->producer_flags & PRODUCER_FLAG_CHANGE_ONLY) &&
        //     (value == rt->last_published_value))
        // {
        //     continue;
        // }

        /** Determine if the input is treated as momentary */
        bool isMomentary = (sub->config.gpioInput.flags 
                            & INPUT_FLAG_MASK_MODE) 
                            == INPUT_FLAG_MODE_MOMENTARY;

        // uint8_t flags    = sub->config.gpioInput.flags;
        // uint8_t modeBits = flags & INPUT_FLAG_MASK_MODE;

        // if ((ts - lastDebugTs) > 100) {
        //     lastDebugTs = ts;
        //     printf("[PRODDBG] sub=%u ts=%u inputFlags=0x%02X isMomentary=%u period=%u flags=0x%02X valueU32=%u lastPub=%u\n",
        //         i,
        //         ts,
        //         sub->config.gpioInput.flags,
        //         isMomentary,
        //         rt->period_ms,
        //         sub->producer_flags,
        //         rt->valueU32,
        //         rt->last_published_value);

        //     printf("[PRODDBG] flags=0x%02X modeBits=0x%02X MOM=0x%02X\n",
        //         flags,
        //         modeBits,
        //         INPUT_MODE_MOMENTARY);

        // }   

        /* Check if time to publish */
        if (ts - lastProducerTick[i] < rt->period_ms)
            continue;

        if (isMomentary) {

            /* Skip if not active or released (skip startup value)*/
            if (value != MOMENTARY_ACTIVE_VALUE && value != MOMENTARY_RELEASE_VALUE) {
                continue;
            }

            bool isReleased = (value == MOMENTARY_RELEASE_VALUE);

            /* If released AND unchanged → skip */
            if (isReleased && value == rt->last_published_value) {
                continue;
            }

            /* If released AND changed → publish once (release event) */
            /* If pressed (active) → always publish */
        }
        else {
            /* Normal CHANGE_ONLY behavior */
            if ((sub->producer_flags & PRODUCER_FLAG_CHANGE_ONLY) &&
                (value == rt->last_published_value))
            {
                continue;
            }
        }


        /* Update last tick */
        lastProducerTick[i]      = ts;

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