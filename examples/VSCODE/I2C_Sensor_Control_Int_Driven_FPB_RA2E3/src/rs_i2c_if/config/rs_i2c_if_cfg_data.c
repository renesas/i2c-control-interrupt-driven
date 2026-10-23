/***********************************************************************************************************************
 * File Name    : rs_i2c_if_cfg_data.c
 * Description  : Contains function definition.
 **********************************************************************************************************************/
/***********************************************************************************************************************
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************************************************************/
/* Validate preset configuration selection. */
#if RS_I2C_IF_CFG_PRESET_FPB_RA4E2_FPB_RA6E2 && \
    RS_I2C_IF_CFG_PRESET_FPB_RA2E3
#error "Only one RS I2C IF preset configuration can be enabled."
#endif

/* Include the selected preset configuration. */
#if RS_I2C_IF_CFG_PRESET_FPB_RA4E2_FPB_RA6E2
#include <rs_i2c_if/config/rs_i2c_if_cfg_data_fpb_ra4e2_fpb_ra6e2.h>
#elif RS_I2C_IF_CFG_PRESET_FPB_RA2E3
#include <rs_i2c_if/config/rs_i2c_if_cfg_data_fpb_ra2e3.h>
#endif

#include <rs_i2c_if/config/rs_i2c_if_cfg_data.h>
#if BSP_CFG_RTOS
#include "i2c_thread.h"
#endif

#if (BSP_CFG_RTOS != 0) && (BSP_CFG_RTOS != 2)
 #error "This configuration supports FreeRTOS (BSP_CFG_RTOS == 2) and bare metal (0) only."
#endif

/* Waits are in RTOS ticks. 0xFFFFFFFF is portMAX_DELAY: one lost interrupt would block the task forever while it
 * holds the bus mutex, hanging every task that uses the bus. Use the same value for rs_i2c_if_cfg_t::semaphore_timeout
 * of every device on this bus. */
#if BSP_CFG_RTOS == 2
 #define RS_I2C_IF_BUS_TIMEOUT_TICKS      (pdMS_TO_TICKS(100))
#else
 #define RS_I2C_IF_BUS_TIMEOUT_TICKS      (0U)
#endif

#if BSP_CFG_RTOS == 2
/* Storage only. rs_i2c_if_ctrl_Open() creates the objects once (counting semaphore max 1 / recursive mutex).
 * Do not create them anywhere else, and not after the first Open(). */
SemaphoreHandle_t g_rs_i2c_if_bus_recursive_mutex_handle;       /* NULL until created */
StaticSemaphore_t g_rs_i2c_if_bus_recursive_mutex_memory;
SemaphoreHandle_t g_rs_i2c_if_bus_blocking_semaphore_handle;    /* NULL until created */
StaticSemaphore_t g_rs_i2c_if_bus_blocking_semaphore_memory;

/* Recursive mutex for the I2C bus */
rs_i2c_if_mutex_t g_rs_i2c_if_bus_recursive_mutex =
{
    .p_mutex_handle = &g_rs_i2c_if_bus_recursive_mutex_handle,
    .p_mutex_memory = &g_rs_i2c_if_bus_recursive_mutex_memory,
};

/* Semaphore the I2C interrupt gives when a transfer has finished */
rs_i2c_if_semaphore_t g_rs_i2c_if_bus_blocking_semaphore =
{
    .p_semaphore_handle = &g_rs_i2c_if_bus_blocking_semaphore_handle,
    .p_semaphore_memory = &g_rs_i2c_if_bus_blocking_semaphore_memory,
};
#endif

/* Shared I2C bus */
rs_i2c_if_bus_extended_cfg_t g_rs_i2c_if_bus_extended_cfg =
{
#if RS_I2C_IF_CFG_PRESET_USE
    .p_driver_instance     = (void *) &g_i2c_preset,
#else
    .p_driver_instance     = (void *) &g_i2c,
#endif
    .p_current_ctrl        = NULL,
    .bus_timeout           = RS_I2C_IF_BUS_TIMEOUT_TICKS,
#if BSP_CFG_RTOS == 2
    .p_blocking_semaphore  = &g_rs_i2c_if_bus_blocking_semaphore,
    .p_bus_recursive_mutex = &g_rs_i2c_if_bus_recursive_mutex,
#endif
    .p_elc                 = NULL,
    .p_timer               = NULL,
};

/* I2C communication device */
rs_i2c_if_instance_ctrl_t g_rs_i2c_if_device_ctrl;

/* Lower level driver configuration */
const i2c_master_cfg_t g_rs_i2c_if_device_lower_level_cfg =
{ .slave = 0x68, .addr_mode = I2C_MASTER_ADDR_MODE_7BIT, .p_callback = rs_i2c_if_callback, };

const rs_i2c_if_cfg_t g_rs_i2c_if_device_cfg =
{ .semaphore_timeout = 0xFFFFFFFF, .p_lower_level_cfg = (void*) &g_rs_i2c_if_device_lower_level_cfg, .p_extend =
          (void*) &g_rs_i2c_if_bus_extended_cfg,
  .p_callback = rm_icm42670_comms_i2c_callback,
#if defined(NULL)
    .p_context          = NULL,
#else
  .p_context = (void*) &NULL,
#endif
        };

const rs_i2c_if_instance_t g_rs_i2c_if_device =
{ .p_ctrl = &g_rs_i2c_if_device_ctrl, .p_cfg = &g_rs_i2c_if_device_cfg, .p_api = &g_comms_on_rs_i2c_if, };

