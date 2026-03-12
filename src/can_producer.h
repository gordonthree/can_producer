#pragma once

#include "canbus_project.h"  /**< Required for nodeInfo_t, subModule_t */
#include "can_platform.h"    /**< Required for can_msg_t */

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 *  CONSTANTS
 * ========================================================================== */

#define PRODUCER_FLAG_NONE         (0x00U)
#define PRODUCER_FLAG_ENABLED      (0x01U)
#define PRODUCER_FLAG_CHANGE_ONLY  (0x02U)
#define PRODUCER_FLAG_RESERVED1    (0x04U)
#define PRODUCER_FLAG_RESERVED2    (0x08U)

#define DEFAULT_PUBLISH_RATE        1000  /**< Default publish period in ms (1 Hz) */
#define PRODUCER_PUBLISH_DISABLED   0



/* Forward declarations */
struct nodeInfo_t;              // forward declaration
typedef struct nodeInfo_t nodeInfo_t;

/* ============================================================================
 *  GLOBALS
 * ========================================================================== */

extern uint32_t lastProducerTick[]; /**< Array of producer tick counters */
extern bool g_producerSaveRequested;
extern bool g_producerLoadRequested;
extern bool g_producerMessageWaiting;

/* ============================================================================
 *  ENUMS
 * ========================================================================== */

typedef struct {
    bool     ready;      /**< True if a message should be published */
    uint8_t  sub_idx;    /**< Which submodule produced the value */
    uint32_t value;      /**< The value to publish */
} producer_event_t;

/**
 * @brief Value source selector for producerReadValue().
 */
typedef enum
{
    VALUE_SRC_NONE      = 0, /**< Disabled / invalid value source */
    VALUE_SRC_STATE       ,  /**< Use logical digital state */
    VALUE_SRC_ADC         ,  /**< Use ADC sample value */
    VALUE_SRC_CONFIG      ,  /**< Use configuration byte */
    VALUE_SRC_HW_OUTPUT   ,  /**< Use last hardware output value */
    VALUE_SRC_RESERVED4   ,  /**< Reserved for future expansion */
    VALUE_SRC_RESERVED5   ,  /**< Reserved for future expansion */
    VALUE_SRC_RESERVED6   ,  /**< Reserved for future expansion */
    VALUE_SRC_RESERVED7      /**< Reserved for future expansion */

} valueSource_t;

/**
 * @brief Producer behavioral type.
 */
typedef enum
{
    PRODUCER_KIND_NONE     = 0,  /**< No producer / disabled */
    PRODUCER_KIND_DIGITAL  = 1,  /**< Digital producer (0/1) */
    PRODUCER_KIND_ANALOG   = 2,  /**< Analog producer (0–4095, etc.) */
    PRODUCER_KIND_COUNTER  = 3,  /**< Counter / incrementing producer */
    PRODUCER_KIND_CUSTOM   = 4   /**< User-defined or extended behavior */

} producer_kind_t;


/* ============================================================================
 *  PRODUCER CONFIG API (CAN message handlers)
 * ========================================================================== */

void handleProducerCfg(const can_msg_t *msg);
void producerPurgeAll(void);
void producerDefaultAll(void);
void producerDefaultSingle(const uint8_t sub_idx);
void handleReqProducerCfg(const can_msg_t *msg);

void producerDelete(const uint8_t sub_idx);
void producerEnable(const uint8_t sub_idx);
void producerDisable(const uint8_t sub_idx);
void producerToggle(const uint8_t sub_idx);

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
 *  END C LINKAGE
 * ========================================================================== */

#ifdef __cplusplus
}
#endif
