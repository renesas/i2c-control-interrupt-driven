/***********************************************************************************************************************
 * File Name    : rs_i2c_if.h
 * Description  : Contains Macros and function declarations.
 **********************************************************************************************************************/
/***********************************************************************************************************************
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************************************************************/
#ifndef RS_I2C_IF_H_
#define RS_I2C_IF_H_
#include "r_i2c_master_api.h"
#if BSP_CFG_RTOS == 2            // FreeRTOS
#include "FreeRTOS.h"
#include "semphr.h"
#endif
/* Macros definitions */
#define RESET_VALUE                 (0)
#define WRITE_BUFFER_MAX            (64U)

/* Version: MAJOR.MINOR.PATCH */
#define RS_I2C_IF_VERSION_MAJOR    (1U)
#define RS_I2C_IF_VERSION_MINOR    (0U)
#define RS_I2C_IF_VERSION_PATCH    (0U)
#define RS_I2C_IF_VERSION_STRING   "1.0.0"

/** Struct to pack params for writeRead */
typedef struct st_rs_i2c_if_write_read_params
{
    uint8_t * p_src;
    uint8_t * p_dest;
    uint8_t   src_bytes;
    uint8_t   dest_bytes;
} rs_i2c_if_write_read_params_t;

/** Event in the callback function */
typedef enum e_rs_i2c_if_event
{
    RS_I2C_IF_EVENT_OPERATION_COMPLETE = 0,
    RS_I2C_IF_EVENT_TX_OPERATION_COMPLETE,
    RS_I2C_IF_EVENT_RX_OPERATION_COMPLETE,
    RS_I2C_IF_EVENT_ERROR,
} rs_i2c_if_event_t;

#if BSP_CFG_RTOS
/* Mutex structure */
typedef struct st_rs_i2c_if_mutex
{
 #if BSP_CFG_RTOS == 2               // FreeRTOS
    SemaphoreHandle_t * p_mutex_handle;
    StaticSemaphore_t * p_mutex_memory;
 #endif
} rs_i2c_if_mutex_t;

/* Semaphore Structure */
typedef struct st_rs_i2c_ifc_semaphore
{
 #if BSP_CFG_RTOS == 2               // FreeRTOS
    SemaphoreHandle_t * p_semaphore_handle;
    StaticSemaphore_t * p_semaphore_memory;
 #endif
} rs_i2c_if_semaphore_t;
#endif

/** Communications middleware callback parameter definition */
typedef struct rs_i2c_if_callback_args
{
    void             * p_context;
    void             * p_instance_args;
    rs_i2c_if_event_t  event;
} rs_i2c_if_callback_args_t;

/** Communications middleware configuration block */
typedef struct st_rs_i2c_if_cfg
{
    uint32_t     semaphore_timeout;                             ///< Timeout for read/write.

    void const * p_extend;                                  ///< Pointer to extended configuration by instance of interface.
    void const * p_lower_level_cfg;                         ///< Pointer to lower level driver configuration structure.

    void      * p_context;                                       ///< Pointer to the user-provided context
    void     (* p_callback)(rs_i2c_if_callback_args_t * p_args); ///< Pointer to callback function, mostly used if using non-blocking functionality.
} rs_i2c_if_cfg_t;

/* I2C bus extended configuration */
typedef struct rs_i2c_if_bus_extended_cfg
{
#if BSP_CFG_RTOS
    rs_i2c_if_mutex_t const     * p_bus_recursive_mutex;    ///< The mutex for the bus. If this is NULL then operations will not lock the bus while it is in use.
    rs_i2c_if_semaphore_t const * p_blocking_semaphore;     ///< The semaphore for blocking operations. If this is NULL then operations will be non-blocking and require a callback.
#endif
    uint32_t          bus_timeout;                          ///< Possible in ticks.
    rs_i2c_if_cfg_t * p_current_ctrl;                       ///< Current device using the bus (by switching the address)
    void const      * p_driver_instance;                    ///< Pointer to I2C HAL interface to be used in the framework

    void const * p_elc;                                     ///< Pointer to ELC instance.
    void const * p_timer;                                   ///< Pointer to GPT instance.
} rs_i2c_if_bus_extended_cfg_t;

/** Communications middleware control structure. */
typedef struct st_rs_i2c_if_instance_ctrl
{
    rs_i2c_if_cfg_t const            * p_cfg;                         ///< middleware configuration.
    rs_i2c_if_bus_extended_cfg_t     * p_bus;                         ///< Bus using this device;
    void                             * p_lower_level_cfg;             ///< Used to reconfigure I2C driver
    uint32_t                           open;                          ///< Open flag.
    uint32_t                           transfer_data_bytes;           ///< Size of transfer data.
    uint8_t                          * p_transfer_data;               ///< Pointer to transfer data buffer.

    /* Specific control variable for SMBus */
    bool smbus_operation;             ///< SMBus operation on I2C bus

    /* Pointer to callback and optional working memory */
    void (* p_callback)(rs_i2c_if_callback_args_t * p_args);

    void * p_context;                 ///< Pointer to the user-provided context
} rs_i2c_if_instance_ctrl_t;

/** Communications control block.  Allocate an instance specific control block to pass into the Communications API calls.
 */
typedef void rs_i2c_if_ctrl_t;

/****************************************************************************************/
/* Module public functions */
typedef struct st_rs_i2c_if_function
{
    fsp_err_t (* open)(rs_i2c_if_ctrl_t * const p_api_ctrl, rs_i2c_if_cfg_t const * const p_cfg);
    fsp_err_t (* close)(rs_i2c_if_ctrl_t * const p_api_ctrl);
    fsp_err_t (* write)(rs_i2c_if_ctrl_t * const p_api_ctrl, uint8_t * const p_src, uint32_t const bytes);
    fsp_err_t (* read)(rs_i2c_if_ctrl_t * const p_api_ctrl, uint8_t * const p_dest, uint32_t const bytes);
    fsp_err_t (* writeRead)(rs_i2c_if_ctrl_t * const p_api_ctrl, rs_i2c_if_write_read_params_t const write_read_params);
}rs_i2c_if_function_t;

/** This structure encompasses everything that is needed to use an instance of this interface. */
typedef struct st_rs_i2c_if_instance
{
    rs_i2c_if_ctrl_t           * p_ctrl;
    rs_i2c_if_cfg_t      const * p_cfg;
    rs_i2c_if_function_t const * p_api;
} rs_i2c_if_instance_t;

/* Exported functions block */
extern rs_i2c_if_function_t const g_comms_on_rs_i2c_if;

/* Function prototypes */
fsp_err_t rs_i2c_if_ctrl_Open(rs_i2c_if_ctrl_t * const p_api_ctrl, rs_i2c_if_cfg_t const * const p_cfg);
fsp_err_t rs_i2c_if_ctrl_Close(rs_i2c_if_ctrl_t * const p_api_ctrl);
fsp_err_t rs_i2c_if_ctrl_Read(rs_i2c_if_ctrl_t * const p_api_ctrl, uint8_t * const p_dest, uint32_t const bytes);
fsp_err_t rs_i2c_if_ctrl_Write(rs_i2c_if_ctrl_t * const p_api_ctrl, uint8_t * const p_src, uint32_t const bytes);
fsp_err_t rs_i2c_if_ctrl_WriteRead(rs_i2c_if_ctrl_t * const p_api_ctrl, rs_i2c_if_write_read_params_t const write_read_params);

#endif /* RS_I2C_IF_H_ */
