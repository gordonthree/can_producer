#pragma once

#include <string.h> // for memset
#include <stddef.h> // for NULL

#include "canbus_project.h" /**< Required for nodeInfo_t, subModule_t */
// #include "node_state.h"     /**< Required for node and trackers */
#include "can_platform.h"    /**< Required for can_msg_t */
#include "submodule_types.h" /**< Sub-module type definitions */

#ifdef __cplusplus
extern "C"
{
#endif

    /* ============================================================================
     *  CONSTANTS
     * ========================================================================== */

    /* Forward declarations */
    struct nodeInfo_t; // forward declaration
    typedef struct nodeInfo_t nodeInfo_t;

    /* ============================================================================
     *  GLOBALS
     * ========================================================================== */

    /** Producer tick counter */
    extern uint32_t lastProducerTick[]; /**< Array of producer tick counters */
    extern bool g_producerSaveRequested;
    extern bool g_producerLoadRequested;
    extern bool g_producerMessageWaiting;

    extern bool gpioProdFired;

    /* ============================================================================
     *  TYPEDEFS
     * ========================================================================== */

    typedef struct
    {
        bool error;         /**< True if an error occurred */
        bool ready;         /**< True if a message should be published */
        uint8_t sub_idx;    /**< Which submodule produced the value */
        uint32_t value;     /**< The value to publish */
        uint32_t timestamp; /**< Timestamp of the event */
    } producer_event_t;

    /**
     * @brief Value source selector for producerReadValue().
     */
    typedef enum
    {
        VALUE_SRC_NONE = 0,  /**< Disabled / invalid value source */
        VALUE_SRC_STATE,     /**< Use logical digital state */
        VALUE_SRC_ADC,       /**< Use ADC sample value */
        VALUE_SRC_CONFIG,    /**< Use configuration byte */
        VALUE_SRC_HW_OUTPUT, /**< Use last hardware output value */
        VALUE_SRC_NUMERIC,   /**< Use numeric value */
        VALUE_SRC_RESERVED4, /**< Reserved for future expansion */
        VALUE_SRC_RESERVED5, /**< Reserved for future expansion */
        VALUE_SRC_RESERVED6, /**< Reserved for future expansion */
        VALUE_SRC_RESERVED7  /**< Reserved for future expansion */

    } valueSource_t;

    /* ============================================================================
     *  PRODUCER CONFIG API (CAN message handlers)
     * ========================================================================== */

    /* producer task handlers */
    producer_event_t producerHandleGpioEvent(uint8_t subIdx, uint32_t ts);

    /* config management */
    void handleProducerCfg(const can_msg_t *msg);
    void producerPurgeSingle(const uint8_t sub_idx);
    void producerPurgeAll(void);
    void producerDefaultSingle(const uint8_t sub_idx);
    void producerDefaultAll(void);

    /* enable/disable */
    void producerEnable(const uint8_t sub_idx);
    void producerDisable(const uint8_t sub_idx);
    void producerToggle(const uint8_t sub_idx);

    /* producer save/load */
    void requestProducerSave(void);
    void requestProducerLoad(void);

    /* ============================================================================
     *  PLATFORM-AGNOSTIC INGESTION API
     * ========================================================================== */

    void nodeSetDigitalState(nodeInfo_t *node,
                             const uint8_t idx,
                             const uint8_t state,
                             const uint32_t ts);

    void nodeSetAdcValue(nodeInfo_t *node,
                         const uint8_t idx,
                         const uint32_t value,
                         const uint32_t ts);

    void nodeSetOutputState(nodeInfo_t *node,
                            const uint8_t idx,
                            const uint8_t value,
                            const uint32_t ts);

    void nodeIngestValue(nodeInfo_t *node,
                         const uint8_t idx,
                         const uint32_t value,
                         const uint32_t ts);

    /* ============================================================================
     *  PRODUCER TICK (publishing logic)
     * ========================================================================== */

    producer_event_t producerTick(const uint32_t ts); /**< Publish producer values based on tick */

    /* ============================================================================
     *  Callback interface for data from the main firmware
     * ========================================================================== */

    typedef struct
    {
        uint8_t (*getSubModuleCount)(void);
        subModule_t *(*getSubModule)(uint8_t idx);
        runTime_t *(*getRuntime)(uint8_t idx);
    } producerCallbacks_t;

    void producerInit(const producerCallbacks_t *cb);

    /* ============================================================================
     *  END C LINKAGE
     * ========================================================================== */

#ifdef __cplusplus
}
#endif
