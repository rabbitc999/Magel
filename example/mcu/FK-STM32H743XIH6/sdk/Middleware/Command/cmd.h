#ifndef __CMD_H
#define __CMD_H

#include "cmd_port.h"
#include "cmd_hex.h"
#include "cmd_string.h"
#include "cmd_user_hex.h"
#include "cmd_user_string.h"

#include "cmd_config.h"
#include "cmd_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

void CMD_Init(void);
uint8_t CMD_ProcessDFA(uint8_t comX, uint8_t *rxData, uint16_t rxDataLength, uint8_t strictMode);
extern CmdRuntimeStats_T g_CMD_RuntimeStats;

#ifdef __cplusplus
}
#endif

#endif
