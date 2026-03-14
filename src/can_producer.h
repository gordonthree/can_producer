#pragma once

#include "canbus_project.h" /**< Required for nodeInfo_t, subModule_t */
// #include "node_state.h"     /**< Required for node and trackers */
#include "can_platform.h"   /**< Required for can_msg_t */
#include "submodule_types.h"   /**< Sub-module type definitions */

#ifdef __cplusplus
extern "C" {
#endif
/* ============================================================================
 *  CONSTANTS
 * ========================================================================== */

#define PRODUCER_FLAG_ENABLED      (0x01U)
#define PRODUCER_FLAG_CHANGE_ONLY  (0x02U)
#define PRODUCER_FLAG_RESERVED1    (0x04U)
#define PRODUCER_FLAG_RESERVED2    (0x08U)

#define PRODUCER_RATEMS_1HZ        (1000U)    /**< 1000ms 1 Hz */
#define PRODUCER_RATEMS_10HZ       (100U)     /**< 100ms 10 Hz */
#define PRODUCER_RATEMS_100HZ      (10U)      /**< 10ms 100 Hz */
/* ============================================================================
 *  GLOBALS
 * ========================================================================== */

/** Producer tick counter */
extern uint32_t lastProducerTick[]; /**< Array of producer tick counters */
extern bool g_producerSaveRequested;
extern bool g_producerLoadRequested;

typedef enum {
    PRODUCER_KIND_NONE = 0,     // Not a producer
    PRODUCER_KIND_SWITCH_STATE, // ON/OFF/MOMENTARY
    PRODUCER_KIND_LEVEL,        // Brightness, duty cycle, analog level
    PRODUCER_KIND_SENSOR,       // ADC, temperature, etc.
    PRODUCER_KIND_EVENT         // Button press, edge-triggered events
} producer_kind_t;


/* ============================================================================
 *  PRODUCER CONFIG API
 * ========================================================================== */

void handleProducerCfg(const can_msg_t *msg);
void handleProducerPurge(void);
void handleProducerDefaults(void);
void handleProducerRemove(const uint8_t sub_idx);
void handleReqProducerCfg(const can_msg_t *msg);
void producerDelete(const uint8_t sub_idx);
void producerEnable(const uint8_t sub_idx);
void producerDisable(const uint8_t sub_idx);
void producerToggle(const uint8_t sub_idx);
void requestProducerSave(void);
void requestProducerLoad(void);

/* Forward declarations, node state accessors */
subModule_t* nodeGetSubModule(const uint8_t sub_idx);
runTime_t*   nodeGetRuntime(const uint8_t sub_idx); 
uint8_t*     nodeGetProducerFlags(const uint8_t sub_idx); 
void         nodeSetProducerFlags(const uint8_t sub_idx, uint8_t flags); 



/* ============================================================================
 *  END C LINKAGE
 * ========================================================================== */

#ifdef __cplusplus
}
#endif