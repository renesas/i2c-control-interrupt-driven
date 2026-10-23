/***********************************************************************************************************************
 * File Name    : rm_icm42670_hal_data.h
 * Description  : Contains Macros and function declarations.
 **********************************************************************************************************************/
/***********************************************************************************************************************
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************************************************************/
#ifndef RM_ICM42670_HAL_DATA_H_
#define RM_ICM42670_HAL_DATA_H_
#include "bsp_api.h"
#include "hal_data.h"
#include "common_data.h"
#include "rm_icm42670.h"
#include "rm_icm42670_api.h"
#include <rs_i2c_if/config/rs_i2c_if_cfg_data.h>
#include <rs_i2c_if/rs_i2c_if.h>

FSP_HEADER
/* I2C Communication Device */

#ifndef rm_icm42670_comms_i2c_callback
void rm_icm42670_comms_i2c_callback(rs_i2c_if_callback_args_t *p_args);
#endif
/* ICM42670 Light Proximity mode */
extern rm_icm42670_mode_extended_cfg_t g_icm42670_sensor0_extended_cfg;
/* ICM42670 Sensor */
extern const rm_icm42670_instance_t g_icm42670_sensor0;
extern rm_icm42670_instance_ctrl_t g_icm42670_sensor0_ctrl;
extern const rm_icm42670_cfg_t g_icm42670_sensor0_cfg;
extern rm_icm42670_device_interrupt_cfg_t g_icm42670_interrupt_cfg;
#ifndef icm42670_comms_i2c_callback
void icm42670_comms_i2c_callback(rm_icm42670_callback_args_t *p_args);
#endif
#ifndef rm_icm42670_irq_callback
void rm_icm42670_irq_callback(external_irq_callback_args_t *p_args);
#endif
#ifndef icm42670_irq_callback
void icm42670_irq_callback(rm_icm42670_callback_args_t *p_args);
#endif

FSP_FOOTER
#endif /* HAL_DATA_H_ */
