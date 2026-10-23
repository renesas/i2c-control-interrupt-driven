/***********************************************************************************************************************
 * File Name    : rs_i2c_if.c
 * Description  : Contains function definition.
 **********************************************************************************************************************/
/***********************************************************************************************************************
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************************************************************/
/* Generic headers */
#include "hal_data.h"
#include <rs_i2c_if/rs_i2c_if.h>
#include <stdlib.h>

/*******************************************************************************************************************//**
 * @addtogroup rs_i2c_if
 * @{
 **********************************************************************************************************************/
/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
/* Definitions of Open flag */
#define RS_I2C_IF_OPEN    (0x1) // Open state

/**********************************************************************************************************************
 * Extern functions
 **********************************************************************************************************************/
extern fsp_err_t rs_i2c_if_bus_status_check(rs_i2c_if_ctrl_t * const p_api_ctrl);
extern fsp_err_t rs_i2c_if_bus_read(rs_i2c_if_ctrl_t * const p_api_ctrl, uint8_t * const p_dest,
                                       uint32_t const bytes);
extern fsp_err_t rs_i2c_if_bus_write(rs_i2c_if_ctrl_t * const p_api_ctrl, uint8_t * const p_src,
                                        uint32_t const bytes);
extern fsp_err_t rs_i2c_if_bus_write_read(rs_i2c_if_ctrl_t * const            p_api_ctrl,
                                        rs_i2c_if_write_read_params_t const write_read_params);

#if BSP_CFG_RTOS == 2
static fsp_err_t rs_i2c_if_bus_os_init(rs_i2c_if_bus_extended_cfg_t * const p_bus)
{
    FSP_ERROR_RETURN(NULL != p_bus, FSP_ERR_ASSERTION);
    if (NULL != p_bus->p_bus_recursive_mutex)
    {
        FSP_ERROR_RETURN(NULL != p_bus->p_bus_recursive_mutex->p_mutex_handle, FSP_ERR_ASSERTION);
        FSP_ERROR_RETURN(NULL != p_bus->p_bus_recursive_mutex->p_mutex_memory, FSP_ERR_ASSERTION);
        if (NULL == *(p_bus->p_bus_recursive_mutex->p_mutex_handle))
        {
            *(p_bus->p_bus_recursive_mutex->p_mutex_handle) = xSemaphoreCreateRecursiveMutexStatic(p_bus->p_bus_recursive_mutex->p_mutex_memory);
            FSP_ERROR_RETURN(NULL != *(p_bus->p_bus_recursive_mutex->p_mutex_handle), FSP_ERR_OUT_OF_MEMORY);
        }
    }
    if (NULL != p_bus->p_blocking_semaphore)
    {
        FSP_ERROR_RETURN(NULL != p_bus->p_blocking_semaphore->p_semaphore_handle, FSP_ERR_ASSERTION);
        FSP_ERROR_RETURN(NULL != p_bus->p_blocking_semaphore->p_semaphore_memory, FSP_ERR_ASSERTION);
        if (NULL == *(p_bus->p_blocking_semaphore->p_semaphore_handle))
        {
            *(p_bus->p_blocking_semaphore->p_semaphore_handle) = xSemaphoreCreateBinaryStatic(p_bus->p_blocking_semaphore->p_semaphore_memory);
            FSP_ERROR_RETURN(NULL != *(p_bus->p_blocking_semaphore->p_semaphore_handle), FSP_ERR_OUT_OF_MEMORY);
        }
    }
    return FSP_SUCCESS;
}
#endif

/* RS I2C Interrupt Driven module's public functions */
const rs_i2c_if_function_t g_comms_on_rs_i2c_if =
{
    .open                  = rs_i2c_if_ctrl_Open,
    .close                 = rs_i2c_if_ctrl_Close,
    .write                 = rs_i2c_if_ctrl_Write,
    .read                  = rs_i2c_if_ctrl_Read,
    .writeRead             = rs_i2c_if_ctrl_WriteRead,
};

/*******************************************************************************************************************//**
 * @brief Reads data from the I2C device.
 *
 * Implements @ref rs_i2c_if_api_t::read.
 *
 * @param[in]  p_api_ctrl    Pointer to the I2C interface control instance.
 * @param[out] p_dest        Destination buffer for received data.
 * @param[in]  bytes         Number of bytes to read.
 *
 * @retval FSP_SUCCESS           Data read successfully.
 * @retval FSP_ERR_ASSERTION     Invalid control or destination pointer.
 * @retval FSP_ERR_INVALID_SIZE  Read size is zero.
 * @retval FSP_ERR_NOT_OPEN      Module is not open.
 *
 * @return Error returned by the underlying I2C bus read operation.
 **********************************************************************************************************************/
fsp_err_t rs_i2c_if_ctrl_Read (rs_i2c_if_ctrl_t * const p_api_ctrl,
                                    uint8_t * const          p_dest,
                                    uint32_t const           bytes)
{
    FSP_ERROR_RETURN(NULL != p_api_ctrl, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_dest, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(0U != bytes, FSP_ERR_INVALID_SIZE);

    rs_i2c_if_instance_ctrl_t * p_ctrl = (rs_i2c_if_instance_ctrl_t *) p_api_ctrl;

    FSP_ERROR_RETURN(RS_I2C_IF_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);

    return rs_i2c_if_bus_read(p_api_ctrl, p_dest, bytes);
}

/*******************************************************************************************************************//**
 * @brief Writes data to the I2C device.
 *
 * Implements @ref rs_i2c_if_api_t::write.
 *
 * @param[in] p_api_ctrl    Pointer to the I2C interface control instance.
 * @param[in] p_src         Source buffer containing data to write.
 * @param[in] bytes         Number of bytes to write.
 *
 * @retval FSP_SUCCESS           Data written successfully.
 * @retval FSP_ERR_ASSERTION     Invalid control or source buffer pointer.
 * @retval FSP_ERR_INVALID_SIZE  Write size is zero.
 * @retval FSP_ERR_NOT_OPEN      Module is not open.
 *
 * @return Error returned by the underlying I2C bus write operation.
 **********************************************************************************************************************/
fsp_err_t rs_i2c_if_ctrl_Write (rs_i2c_if_ctrl_t * const p_api_ctrl,
                                     uint8_t * const          p_src,
                                     uint32_t const           bytes)
{
    FSP_ERROR_RETURN(NULL != p_api_ctrl, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_src, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(0U != bytes, FSP_ERR_INVALID_SIZE);

    rs_i2c_if_instance_ctrl_t * p_ctrl = (rs_i2c_if_instance_ctrl_t *) p_api_ctrl;

    FSP_ERROR_RETURN(RS_I2C_IF_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);

    return rs_i2c_if_bus_write(p_api_ctrl, p_src, bytes);
}

/*******************************************************************************************************************//**
 * @brief Writes data to, then reads data from, the I2C device.
 *
 * Implements @ref rs_i2c_if_api_t::writeRead.
 *
 * @param[in] p_api_ctrl         Pointer to the I2C interface control instance.
 * @param[in] write_read_params  Parameters for the write-read transaction.
 *
 * @retval FSP_SUCCESS           Write-read operation completed successfully.
 * @retval FSP_ERR_ASSERTION     Invalid control, source, or destination pointer.
 * @retval FSP_ERR_INVALID_SIZE  Write or read transfer size is zero.
 * @retval FSP_ERR_NOT_OPEN      Module is not open.
 *
 * @return Error returned by the underlying I2C bus write-read operation.
 **********************************************************************************************************************/
fsp_err_t rs_i2c_if_ctrl_WriteRead (rs_i2c_if_ctrl_t * const                  p_api_ctrl,
                                         rs_i2c_if_write_read_params_t const write_read_params)
{
    FSP_ERROR_RETURN(NULL != p_api_ctrl, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != write_read_params.p_src, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != write_read_params.p_dest, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(0U != write_read_params.src_bytes, FSP_ERR_INVALID_SIZE);
    FSP_ERROR_RETURN(0U != write_read_params.dest_bytes, FSP_ERR_INVALID_SIZE);

    rs_i2c_if_instance_ctrl_t * p_ctrl = (rs_i2c_if_instance_ctrl_t *) p_api_ctrl;

    FSP_ERROR_RETURN(RS_I2C_IF_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);

    return rs_i2c_if_bus_write_read(p_api_ctrl, write_read_params);
}

/*******************************************************************************************************************//**
 * @brief Opens and initializes the I2C interface instance.
 *
 * Implements @ref rs_i2c_if_api_t::open.
 *
 * @param[in,out] p_api_ctrl  Pointer to the I2C interface control instance.
 * @param[in]     p_cfg       Pointer to the I2C interface configuration.
 *
 * @retval FSP_SUCCESS           I2C interface opened successfully.
 * @retval FSP_ERR_ASSERTION     Invalid control pointer or configuration.
 * @retval FSP_ERR_ALREADY_OPEN  I2C interface instance is already open.
 *
 * @return Error returned by RTOS initialization or the underlying I2C bus status check.
 **********************************************************************************************************************/
fsp_err_t rs_i2c_if_ctrl_Open (rs_i2c_if_ctrl_t * const       p_api_ctrl,
                                    rs_i2c_if_cfg_t const * const p_cfg)
{
    FSP_ERROR_RETURN(NULL != p_api_ctrl, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_cfg, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_cfg->p_extend, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_cfg->p_lower_level_cfg, FSP_ERR_ASSERTION);

    rs_i2c_if_instance_ctrl_t * p_ctrl = (rs_i2c_if_instance_ctrl_t *) p_api_ctrl;
    rs_i2c_if_bus_extended_cfg_t * p_bus =
        (rs_i2c_if_bus_extended_cfg_t *) p_cfg->p_extend;

    FSP_ERROR_RETURN(RS_I2C_IF_OPEN != p_ctrl->open, FSP_ERR_ALREADY_OPEN);
    FSP_ERROR_RETURN(NULL != p_bus->p_driver_instance, FSP_ERR_ASSERTION);

#if BSP_CFG_RTOS
    /* Blocking transfers require both synchronization objects. */
    FSP_ERROR_RETURN(((NULL == p_bus->p_blocking_semaphore) &&
                      (NULL == p_bus->p_bus_recursive_mutex)) ||
                     ((NULL != p_bus->p_blocking_semaphore) &&
                      (NULL != p_bus->p_bus_recursive_mutex)), FSP_ERR_ASSERTION);
#endif

    /* Initialize the control block before checking the lower-level bus. */
    p_ctrl->p_cfg                 = p_cfg;
    p_ctrl->p_bus                 = p_bus;
    p_ctrl->p_lower_level_cfg     = (void *) p_cfg->p_lower_level_cfg;
    p_ctrl->p_callback            = p_cfg->p_callback;
    p_ctrl->p_context             = p_cfg->p_context;
    p_ctrl->p_transfer_data       = NULL;
    p_ctrl->transfer_data_bytes   = 0U;
    p_ctrl->smbus_operation       = false;

    fsp_err_t err = FSP_SUCCESS;
#if BSP_CFG_RTOS == 2
    err = rs_i2c_if_bus_os_init(p_bus);
    if (FSP_SUCCESS != err)
    {
        p_ctrl->p_cfg = NULL; p_ctrl->p_bus = NULL; p_ctrl->p_lower_level_cfg = NULL;
        p_ctrl->p_callback = NULL; p_ctrl->p_context = NULL;
        return err;
    }
#endif
    err = rs_i2c_if_bus_status_check(p_api_ctrl);
    if (FSP_SUCCESS != err)
    {
        p_ctrl->p_cfg               = NULL;
        p_ctrl->p_bus               = NULL;
        p_ctrl->p_lower_level_cfg   = NULL;
        p_ctrl->p_callback          = NULL;
        p_ctrl->p_context           = NULL;
        return err;
    }

    /* Force address and callback configuration on the first transfer. */
    if (p_bus->p_current_ctrl == p_api_ctrl)
    {
        p_bus->p_current_ctrl = NULL;
    }

    p_ctrl->open = RS_I2C_IF_OPEN;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief Closes the I2C interface instance.
 *
 * Implements @ref rs_i2c_if_api_t::close.
 *
 * @param[in,out] p_api_ctrl  Pointer to the I2C interface control instance.
 *
 * @retval FSP_SUCCESS        I2C interface closed successfully.
 * @retval FSP_ERR_ASSERTION  Invalid control or bus pointer.
 * @retval FSP_ERR_NOT_OPEN   I2C interface instance is not open.
 **********************************************************************************************************************/
fsp_err_t rs_i2c_if_ctrl_Close (rs_i2c_if_ctrl_t * const p_api_ctrl)
{
    FSP_ERROR_RETURN(NULL != p_api_ctrl, FSP_ERR_ASSERTION);

    rs_i2c_if_instance_ctrl_t * p_ctrl = (rs_i2c_if_instance_ctrl_t *) p_api_ctrl;

    FSP_ERROR_RETURN(RS_I2C_IF_OPEN == p_ctrl->open, FSP_ERR_NOT_OPEN);
    FSP_ERROR_RETURN(NULL != p_ctrl->p_bus, FSP_ERR_ASSERTION);

    /* Clear the shared-bus selection only when this instance owns it. */
    if (p_ctrl->p_bus->p_current_ctrl == p_api_ctrl)
    {
        p_ctrl->p_bus->p_current_ctrl = NULL;
    }

    p_ctrl->open                = 0U;
    p_ctrl->p_cfg               = NULL;
    p_ctrl->p_bus               = NULL;
    p_ctrl->p_lower_level_cfg   = NULL;
    p_ctrl->p_callback          = NULL;
    p_ctrl->p_context           = NULL;
    p_ctrl->p_transfer_data     = NULL;
    p_ctrl->transfer_data_bytes = 0U;
    p_ctrl->smbus_operation     = false;

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @} (end addtogroup rs_i2c_if)
 **********************************************************************************************************************/
