/***********************************************************************************************************************
 * File Name    : rs_i2c_if_cfg_data.h
 * Description  : Contains Macros and function declarations.
 **********************************************************************************************************************/
/***********************************************************************************************************************
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************************************************************/
#ifndef RS_I2C_IF_CFG_DATA_H_
#define RS_I2C_IF_CFG_DATA_H_
#include <rs_i2c_if/rs_i2c_if.h>
#include <stdint.h>
#include "bsp_api.h"
#include "common_data.h"
#include "hal_data.h"

/* I2C Communication Device */
extern const rs_i2c_if_instance_t g_rs_i2c_if_device;
extern rs_i2c_if_instance_ctrl_t g_rs_i2c_if_device_ctrl;
extern const rs_i2c_if_cfg_t g_rs_i2c_if_device_cfg;
extern const i2c_master_cfg_t g_rs_i2c_if_device_lower_level_cfg;
extern rs_i2c_if_bus_extended_cfg_t g_rs_i2c_if_bus_extended_cfg;

#ifndef rm_icm42670_comms_i2c_callback
void rm_icm42670_comms_i2c_callback(rs_i2c_if_callback_args_t *p_args);
#endif

#ifndef rs_i2c_if_callback
void rs_i2c_if_callback(i2c_master_callback_args_t *p_args);
#endif

#endif /* RS_I2C_IF_CFG_DATA_H_ */
