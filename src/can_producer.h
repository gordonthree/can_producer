#pragma once

#include "canbus_project.h" /**< Required for nodeInfo_t, subModule_t */
// #include "node_state.h"     /**< Required for node and trackers */
#include "can_platform.h"   /**< Required for can_msg_t */

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

/* ============================================================================
 *  GLOBALS
 * ========================================================================== */

/** Producer tick counter */
extern uint32_t lastProducerTick[]; /**< Array of producer tick counters */
extern bool g_producerSaveRequested;
extern bool g_producerLoadRequested;


/* ============================================================================
 *  PRODUCER CONFIG API
 * ========================================================================== */

void handleProducerCfg(const can_msg_t *msg);
void handleProducerPurge(const can_msg_t *msg);
void handleProducerDefaults(const can_msg_t *msg);
void handleProducerApply(void);
void handleReqProducerCfg(const can_msg_t *msg);

/* ============================================================================
 *  NVS LOAD/SAVE wrappers - use flags for signaling
 * ========================================================================== */

void loadProducerCfgFromNVS(void);
void saveProducerCfgToNVS(void);

/* ============================================================================
 *  END C LINKAGE
 * ========================================================================== */

#ifdef __cplusplus
}
#endif