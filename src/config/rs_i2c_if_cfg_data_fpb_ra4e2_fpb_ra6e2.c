/***********************************************************************************************************************
 * File Name    : rs_i2c_if_cfg_data_fpb_ra4e2_fpb_ra6e2.c
 * Description  : Contains function definition.
 **********************************************************************************************************************/
/***********************************************************************************************************************
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************************************************************/
#if RS_I2C_IF_CFG_PRESET_FPB_RA4E2_FPB_RA6E2 // Use pre-set configuration for RS I2C IF
#include <rs_i2c_if/config/rs_i2c_if_cfg_data_fpb_ra4e2_fpb_ra6e2.h>

iic_b_master_instance_ctrl_t g_i2c_ctrl_preset;
const iic_b_master_extended_cfg_t g_i2c_extend_preset =
{ .timeout_mode = IIC_B_MASTER_TIMEOUT_MODE_SHORT,
  .timeout_scl_low = IIC_B_MASTER_TIMEOUT_SCL_LOW_ENABLED,
  .smbus_operation = 0,
  /* Actual calculated bitrate: 1000000. Actual calculated duty cycle: 50%. */.clock_settings.brl_value = 75,
  .clock_settings.brh_value = 75,
  .clock_settings.cks_value = 0,
  .clock_settings.sdod_value = 0,
  .clock_settings.sdodcs_value = 0,
  .iic_clock_freq = 200000000, };
const i2c_master_cfg_t g_i2c_cfg_preset =
{ .channel = 0, .rate = I2C_MASTER_RATE_FASTPLUS, .slave = 0x68, .addr_mode = I2C_MASTER_ADDR_MODE_7BIT,
#define RA_NOT_DEFINED (1)
#if (RA_NOT_DEFINED == RA_NOT_DEFINED)
  .p_transfer_tx = NULL,
#else
                .p_transfer_tx       = &RA_NOT_DEFINED,
#endif
#if (RA_NOT_DEFINED == RA_NOT_DEFINED)
  .p_transfer_rx = NULL,
#else
                .p_transfer_rx       = &RA_NOT_DEFINED,
#endif
#undef RA_NOT_DEFINED
  .p_callback = rs_i2c_if_callback,
  .p_context = NULL,
#if defined(VECTOR_NUMBER_IICB0_RXI)
    .rxi_irq             = VECTOR_NUMBER_IICB0_RXI,
#else
  .rxi_irq = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_IICB0_TXI)
    .txi_irq             = VECTOR_NUMBER_IICB0_TXI,
#else
  .txi_irq = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_IICB0_TEND)
    .tei_irq             = VECTOR_NUMBER_IICB0_TEND,
#elif defined(VECTOR_NUMBER_IICB0_TEI)
    .tei_irq             = VECTOR_NUMBER_IICB0_TEI,
#else
  .tei_irq = FSP_INVALID_VECTOR,
#endif
#if defined(VECTOR_NUMBER_IICB0_EEI)
    .eri_irq             = VECTOR_NUMBER_IICB0_EEI,
#elif defined(VECTOR_NUMBER_IICB0_ERI)
    .eri_irq             = VECTOR_NUMBER_IICB0_ERI,
#else
  .eri_irq = FSP_INVALID_VECTOR,
#endif
  .ipl = (15),
  .p_extend = &g_i2c_extend_preset, };
/* Instance structure to use this module. */
const i2c_master_instance_t g_i2c_preset =
{ .p_ctrl = &g_i2c_ctrl_preset, .p_cfg = &g_i2c_cfg_preset, .p_api = &g_i2c_master_on_iic_b };

#endif //Use pre-set configuration for RS I2C IF
