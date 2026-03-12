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
    if (msg->data_length_code < CFG_PRODUCER_CFG_DLC)
        return;

    uint8_t idx = msg->data[MSG_DATA_4];
    if (idx >= MAX_SUB_MODULES)
        return;

    subModule_t *sub = producerGetSubModule(idx);
    if (!sub) return;

    runTime_t *rt = &sub->runTime;

    /* ============================
     *  Decode producer configuration
     * ============================ */

    rt->period_ms = ((uint16_t)msg->data[MSG_DATA_5] << BYTE_SHIFT)  /**< period_ms MSB */
                   |  (uint16_t)msg->data[MSG_DATA_6];               /**< period_ms LSB */

    sub->producer_flags = msg->data[MSG_DATA_7];                     /**< producer flags */

    g_producerSaveRequested = true;
}


/**
 * @brief Restore universal safe defaults for all producers.
 *
 * Calls producerDefaultSingle() for each sub-module. This avoids duplicating
 * default logic and ensures all producers share the same baseline behavior.
 */
void producerDefaultAll(void)
{
    for (uint8_t i = 0; i < MAX_SUB_MODULES; i++) {
        producerDefaultSingle(i);
    }
}


/**
 * @brief Restore meaningful default producer configuration for a single sub-module.
 *
 * @param sub_idx Index of the sub-module to default.
 */
void producerDefaultSingle(const uint8_t sub_idx)
{
    if (sub_idx >= MAX_SUB_MODULES)
        return;

    subModule_t *sub = producerGetSubModule(sub_idx);
    if (!sub) return;

    runTime_t *rt = &sub->runTime;

    /* Clear runtime snapshot */
    memset(rt, 0, sizeof(runTime_t));

    /* Apply meaningful defaults */
    rt->kind            = PRODUCER_KIND_NONE;        /**< No kind by default */
    rt->valueSource     = VALUE_SRC_NONE;            /**< Publish nothing */
    rt->period_ms       = PRODUCER_PUBLISH_DISABLED; /**< Disabled */
    sub->producer_flags = PRODUCER_FLAG_NONE;        /**< Disabled by default */

    g_producerSaveRequested = true;
}


/**
 * @brief CAUTION: Hard wipe of all producer runtime/config fields.
 *        This clears all producer-related state for every sub-module.
 */
void producerPurgeAll(void)
{
    for (uint8_t i = 0; i < MAX_SUB_MODULES; i++) {

        subModule_t *sub = producerGetSubModule(i);
        if (!sub) continue;

        runTime_t *rt = &sub->runTime;
        memset(rt, 0, sizeof(runTime_t));              /**< Clear runtime/config */

        sub->producer_flags = PRODUCER_FLAG_NONE;      /**< Clear all producer flags */
    }

    g_producerSaveRequested = true;
}



/**
 * @brief CAUTION: Hard delete of a single producer runtime/config fields.
 *        This clears all producer-related state for a single sub-module.
 *
 * @param sub_idx The index of the sub-module to delete.
 */
void producerDelete(const uint8_t sub_idx)
{
    subModule_t *sub = producerGetSubModule(sub_idx);
    if (!sub) return;

    runTime_t *rt = &sub->runTime;
    memset(rt, 0, sizeof(runTime_t));

    sub->producer_flags = PRODUCER_FLAG_NONE;  /**< Clear flags too */

    g_producerSaveRequested = true;

}

/**
 * @brief Enable a producer to publish data.
 *
 * Set the PRODUCER_FLAG_ENABLED bit in the producer flags of the sub-module at idx.
 * This will allow the producer to publish data according to its configuration.
 *
 * @param sub_idx Index of the sub-module to enable.
 */
void producerEnable(const uint8_t sub_idx)
{
    if (sub_idx >= MAX_SUB_MODULES)
        return;

    subModule_t *sub = producerGetSubModule(sub_idx);
    sub->producer_flags |= PRODUCER_FLAG_ENABLED;

    g_producerSaveRequested = true;
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

    subModule_t *sub = producerGetSubModule(sub_idx);
    sub->producer_flags &= ~PRODUCER_FLAG_ENABLED;

    g_producerSaveRequested = true;
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

    subModule_t *sub = producerGetSubModule(sub_idx);
    sub->producer_flags ^= PRODUCER_FLAG_ENABLED;

    g_producerSaveRequested = true;
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
void producerTick(const uint32_t ts)
{
    for (uint8_t i = 0; i < MAX_SUB_MODULES; i++) {

        runTime_t *rt = producerGetRuntime(i);
        if (!rt) continue;

        subModule_t *sub = producerGetSubModule(i);
        if (!sub) continue;

        /* Producer disabled? */
        if (!(sub->producer_flags & PRODUCER_FLAG_ENABLED))
            continue;

        /* Period disabled? */
        if (rt->period_ms == 0)
            continue;

        /* Time to publish? */
        if (ts - lastProducerTick[i] < rt->period_ms)
            continue;

        lastProducerTick[i] = ts;

        /* Select value based on valueSource */
        uint32_t value = 0;
        switch (rt->valueSource) {
            case VALUE_SRC_STATE:            value = rt->state; break;
            case VALUE_SRC_ADC:              value = rt->adc_value; break;
            case VALUE_SRC_HW_OUTPUT:        value = rt->last_hardware_output; break;
            default:                         continue;
        }

        /* Change-only logic */
        if ((sub->producer_flags & PRODUCER_FLAG_CHANGE_ONLY) &&
            (value == rt->last_published_value))
        {
            continue;
        }

        /* Publish CAN message (firmware implements this) */
        sendProducerData(i, value); // TODO: implement this in main.cpp

        /* Update last published */
        rt->last_published_value = value;
    }
}


#ifdef __cplusplus
}
#endif