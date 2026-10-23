/***********************************************************************************************************************
 * File Name    : rs_i2c_if_cfg_data_fpb_ra2e3.h
 * Description  : Contains Macros and function declarations.
 **********************************************************************************************************************/
/***********************************************************************************************************************
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************************************************************/
#if RS_I2C_IF_CFG_PRESET_FPB_RA2E3 // Use pre-set configuration for RS I2C IF
#ifndef _RS_I2C_IF_CFG_FPB_RA2E3_H_
#define _RS_I2C_IF_CFG_FPB_RA2E3_H_
#include <stdint.h>
#include "bsp_api.h"
#include "r_iic_master.h"
#include <rs_i2c_if/config/rs_i2c_if_cfg_data.h>
FSP_HEADER

/* I2C Master on IIC Instance. */
extern const i2c_master_instance_t g_i2c_preset;

/** Access the I2C Master instance using these structures when calling API functions directly (::p_api is not used). */
extern iic_master_instance_ctrl_t g_i2c_ctrl_preset;
extern const i2c_master_cfg_t g_i2c_cfg_preset;

FSP_FOOTER
#endif /* _RS_I2C_IF_CFG_FPB_RA2E3_H_ */
#endif //Use pre-set configuration for RS I2C IF
