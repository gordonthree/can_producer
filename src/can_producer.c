#include "can_producer.h"

/* ===== GLOBALS ===== */

/** Producer tick counter */
uint32_t lastProducerTick[MAX_SUB_MODULES] = {0}; 