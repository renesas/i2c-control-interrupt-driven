/***********************************************************************************************************************
 * File Name    : rs_i2c_if_ra.c
 * Description  : Contains function definition.
 **********************************************************************************************************************/
/***********************************************************************************************************************
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************************************************************/
#include "hal_data.h"
#include <rs_i2c_if/rs_i2c_if.h>
#include <stdlib.h>

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @addtogroup rs_i2c_if
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
static fsp_err_t rs_i2c_if_bus_reconfigure(rs_i2c_if_ctrl_t * const p_api_ctrl);

#if BSP_CFG_RTOS
static fsp_err_t rs_i2c_if_os_semaphore_acquire(rs_i2c_if_semaphore_t const * p_semaphore,
                                                   uint32_t const                   timeout);
static fsp_err_t rs_i2c_if_os_semaphore_release_from_ISR(rs_i2c_if_semaphore_t const * p_semaphore);

#endif

/**********************************************************************************************************************
 * Extern functions
 **********************************************************************************************************************/

/**********************************************************************************************************************
 * Extern variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables
 **********************************************************************************************************************/
fsp_err_t rs_i2c_if_bus_status_check(rs_i2c_if_ctrl_t * const p_api_ctrl);
fsp_err_t rs_i2c_if_bus_read(rs_i2c_if_ctrl_t * const p_api_ctrl, uint8_t * const p_dest, uint32_t const bytes);
fsp_err_t rs_i2c_if_bus_write(rs_i2c_if_ctrl_t * const p_api_ctrl, uint8_t * const p_src, uint32_t const bytes);
fsp_err_t rs_i2c_if_bus_write_read(rs_i2c_if_ctrl_t * const            p_api_ctrl,
                                      rs_i2c_if_write_read_params_t const write_read_params);
void rs_i2c_if_process_in_callback(rs_i2c_if_ctrl_t * const p_api_ctrl, rs_i2c_if_callback_args_t * const p_args);

#if BSP_CFG_RTOS
fsp_err_t rs_i2c_if_os_recursive_mutex_acquire(rs_i2c_if_mutex_t const * p_mutex, uint32_t const timeout);
fsp_err_t rs_i2c_if_os_recursive_mutex_release(rs_i2c_if_mutex_t const * p_mutex);
#endif

/***********************************************************************************************************************
 * Global variables
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/


/*******************************************************************************************************************//**
 * @brief Checks if the I2C driver module is open.
 *
 * @retval FSP_SUCCESS                  successfully configured.
 * @retval FSP_ERR_COMMS_BUS_NOT_OPEN   I2C driver is not open.
 **********************************************************************************************************************/
fsp_err_t rs_i2c_if_bus_status_check (rs_i2c_if_ctrl_t * const p_api_ctrl)
{
    fsp_err_t err;
    i2c_master_status_t status;

    FSP_ERROR_RETURN(NULL != p_api_ctrl, FSP_ERR_ASSERTION);

    rs_i2c_if_instance_ctrl_t * p_ctrl = (rs_i2c_if_instance_ctrl_t *) p_api_ctrl;

    FSP_ERROR_RETURN(NULL != p_ctrl->p_bus, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_ctrl->p_bus->p_driver_instance, FSP_ERR_ASSERTION);

    i2c_master_instance_t * p_driver_instance =
        (i2c_master_instance_t *) p_ctrl->p_bus->p_driver_instance;

    FSP_ERROR_RETURN(NULL != p_driver_instance->p_api, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_driver_instance->p_api->statusGet, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_driver_instance->p_ctrl, FSP_ERR_ASSERTION);

    err = p_driver_instance->p_api->statusGet(p_driver_instance->p_ctrl, &status);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);
    FSP_ERROR_RETURN(true == status.open, FSP_ERR_COMMS_BUS_NOT_OPEN);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief Read data from the device using the I2C driver module.
 *
 * @retval FSP_SUCCESS              successfully configured.
 **********************************************************************************************************************/
fsp_err_t rs_i2c_if_bus_read (rs_i2c_if_ctrl_t * const p_api_ctrl,
                                    uint8_t * const          p_dest,
                                    uint32_t const           bytes)
{
    fsp_err_t err = FSP_SUCCESS;
#if BSP_CFG_RTOS
    bool mutex_acquired = false;
#endif

    FSP_ERROR_RETURN(NULL != p_api_ctrl, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_dest, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(0U != bytes, FSP_ERR_INVALID_SIZE);

    rs_i2c_if_instance_ctrl_t * p_ctrl = (rs_i2c_if_instance_ctrl_t *) p_api_ctrl;

    FSP_ERROR_RETURN(NULL != p_ctrl->p_bus, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_ctrl->p_cfg, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_ctrl->p_bus->p_driver_instance, FSP_ERR_ASSERTION);

    i2c_master_instance_t * p_driver_instance =
        (i2c_master_instance_t *) p_ctrl->p_bus->p_driver_instance;

    FSP_ERROR_RETURN(NULL != p_driver_instance->p_api, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_driver_instance->p_api->read, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_driver_instance->p_ctrl, FSP_ERR_ASSERTION);

#if BSP_CFG_RTOS
    if (NULL != p_ctrl->p_bus->p_bus_recursive_mutex)
    {
        err = rs_i2c_if_os_recursive_mutex_acquire(p_ctrl->p_bus->p_bus_recursive_mutex,
                                                    p_ctrl->p_bus->bus_timeout);
        if (FSP_SUCCESS != err)
        {
            return err;
        }
        mutex_acquired = true;
    }
#endif

    if (FSP_SUCCESS == err)
    {
        err = rs_i2c_if_bus_reconfigure(p_api_ctrl);
    }

#if BSP_CFG_RTOS
    /* Remove a stale completion token from a previous timed-out transfer. */
    if ((FSP_SUCCESS == err) && (NULL != p_ctrl->p_bus->p_blocking_semaphore))
    {
        fsp_err_t drain_err = rs_i2c_if_os_semaphore_acquire(p_ctrl->p_bus->p_blocking_semaphore, 0U);
        if ((FSP_SUCCESS != drain_err) && (FSP_ERR_TIMEOUT != drain_err))
        {
            err = drain_err;
        }
    }
#endif

    if (FSP_SUCCESS == err)
    {
        if (!p_ctrl->smbus_operation)
        {
            p_ctrl->p_transfer_data      = NULL;
            p_ctrl->transfer_data_bytes = 0U;
        }
        else
        {
            p_ctrl->p_transfer_data      = p_dest;
            p_ctrl->transfer_data_bytes = bytes;
        }

        p_ctrl->transfer_err = false;   /* clear before the ISR can set it */
        err = p_driver_instance->p_api->read(p_driver_instance->p_ctrl, p_dest, bytes, false);
    }

#if BSP_CFG_RTOS
    if ((FSP_SUCCESS == err) && (NULL != p_ctrl->p_bus->p_blocking_semaphore))
    {
        err = rs_i2c_if_os_semaphore_acquire(p_ctrl->p_bus->p_blocking_semaphore,
                                              p_ctrl->p_cfg->semaphore_timeout);
        if (FSP_SUCCESS == err)
        {
            /* The callback gives the semaphore for errors too; report them. */
            if (p_ctrl->transfer_err)
            {
                err = FSP_ERR_ABORTED;
            }
        }
        else if ((FSP_ERR_TIMEOUT == err) && (NULL != p_driver_instance->p_api->abort))
        {
            (void) p_driver_instance->p_api->abort(p_driver_instance->p_ctrl);
        }
    }

    if (mutex_acquired)
    {
        fsp_err_t mutex_err =
            rs_i2c_if_os_recursive_mutex_release(p_ctrl->p_bus->p_bus_recursive_mutex);
        if ((FSP_SUCCESS == err) && (FSP_SUCCESS != mutex_err))
        {
            err = mutex_err;
        }
    }
#endif

    return err;
}

/*******************************************************************************************************************//**
 * @brief Write data to the device using the I2C driver module.
 *
 * @retval FSP_SUCCESS              successfully configured.
 **********************************************************************************************************************/
fsp_err_t rs_i2c_if_bus_write (rs_i2c_if_ctrl_t * const p_api_ctrl,
                                     uint8_t * const          p_src,
                                     uint32_t const           bytes)
{
    fsp_err_t err = FSP_SUCCESS;
#if BSP_CFG_RTOS
    bool mutex_acquired = false;
#endif

    FSP_ERROR_RETURN(NULL != p_api_ctrl, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_src, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(0U != bytes, FSP_ERR_INVALID_SIZE);

    rs_i2c_if_instance_ctrl_t * p_ctrl = (rs_i2c_if_instance_ctrl_t *) p_api_ctrl;

    FSP_ERROR_RETURN(NULL != p_ctrl->p_bus, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_ctrl->p_cfg, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_ctrl->p_bus->p_driver_instance, FSP_ERR_ASSERTION);

    i2c_master_instance_t * p_driver_instance =
        (i2c_master_instance_t *) p_ctrl->p_bus->p_driver_instance;

    FSP_ERROR_RETURN(NULL != p_driver_instance->p_api, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_driver_instance->p_api->write, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_driver_instance->p_ctrl, FSP_ERR_ASSERTION);

#if BSP_CFG_RTOS
    if (NULL != p_ctrl->p_bus->p_bus_recursive_mutex)
    {
        err = rs_i2c_if_os_recursive_mutex_acquire(p_ctrl->p_bus->p_bus_recursive_mutex,
                                                    p_ctrl->p_bus->bus_timeout);
        if (FSP_SUCCESS != err)
        {
            return err;
        }
        mutex_acquired = true;
    }
#endif

    if (FSP_SUCCESS == err)
    {
        err = rs_i2c_if_bus_reconfigure(p_api_ctrl);
    }

#if BSP_CFG_RTOS
    if ((FSP_SUCCESS == err) && (NULL != p_ctrl->p_bus->p_blocking_semaphore))
    {
        fsp_err_t drain_err = rs_i2c_if_os_semaphore_acquire(p_ctrl->p_bus->p_blocking_semaphore, 0U);
        if ((FSP_SUCCESS != drain_err) && (FSP_ERR_TIMEOUT != drain_err))
        {
            err = drain_err;
        }
    }
#endif

    if (FSP_SUCCESS == err)
    {
        p_ctrl->p_transfer_data      = NULL;
        p_ctrl->transfer_data_bytes = 0U;
        p_ctrl->transfer_err = false;   /* clear before the ISR can set it */
        err = p_driver_instance->p_api->write(p_driver_instance->p_ctrl, p_src, bytes, false);
    }

#if BSP_CFG_RTOS
    if ((FSP_SUCCESS == err) && (NULL != p_ctrl->p_bus->p_blocking_semaphore))
    {
        err = rs_i2c_if_os_semaphore_acquire(p_ctrl->p_bus->p_blocking_semaphore,
                                              p_ctrl->p_cfg->semaphore_timeout);
        if (FSP_SUCCESS == err)
        {
            /* The callback gives the semaphore for errors too; report them. */
            if (p_ctrl->transfer_err)
            {
                err = FSP_ERR_ABORTED;
            }
        }
        else if ((FSP_ERR_TIMEOUT == err) && (NULL != p_driver_instance->p_api->abort))
        {
            (void) p_driver_instance->p_api->abort(p_driver_instance->p_ctrl);
        }
    }

    if (mutex_acquired)
    {
        fsp_err_t mutex_err =
            rs_i2c_if_os_recursive_mutex_release(p_ctrl->p_bus->p_bus_recursive_mutex);
        if ((FSP_SUCCESS == err) && (FSP_SUCCESS != mutex_err))
        {
            err = mutex_err;
        }
    }
#endif

    return err;
}

/*******************************************************************************************************************//**
 * @brief  Writes to I2C bus, then reads with restart.
 *
 * @retval FSP_SUCCESS              successfully configured.
 **********************************************************************************************************************/
fsp_err_t rs_i2c_if_bus_write_read (rs_i2c_if_ctrl_t * const                  p_api_ctrl,
                                          rs_i2c_if_write_read_params_t const write_read_params)
{
    fsp_err_t err = FSP_SUCCESS;
#if BSP_CFG_RTOS
    bool mutex_acquired = false;
#endif

    FSP_ERROR_RETURN(NULL != p_api_ctrl, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != write_read_params.p_src, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != write_read_params.p_dest, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(0U != write_read_params.src_bytes, FSP_ERR_INVALID_SIZE);
    FSP_ERROR_RETURN(0U != write_read_params.dest_bytes, FSP_ERR_INVALID_SIZE);

    rs_i2c_if_instance_ctrl_t * p_ctrl = (rs_i2c_if_instance_ctrl_t *) p_api_ctrl;

    FSP_ERROR_RETURN(NULL != p_ctrl->p_bus, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_ctrl->p_cfg, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_ctrl->p_bus->p_driver_instance, FSP_ERR_ASSERTION);

    i2c_master_instance_t * p_driver_instance =
        (i2c_master_instance_t *) p_ctrl->p_bus->p_driver_instance;

    FSP_ERROR_RETURN(NULL != p_driver_instance->p_api, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_driver_instance->p_api->write, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_driver_instance->p_ctrl, FSP_ERR_ASSERTION);

#if BSP_CFG_RTOS
    if (NULL != p_ctrl->p_bus->p_bus_recursive_mutex)
    {
        err = rs_i2c_if_os_recursive_mutex_acquire(p_ctrl->p_bus->p_bus_recursive_mutex,
                                                    p_ctrl->p_bus->bus_timeout);
        if (FSP_SUCCESS != err)
        {
            return err;
        }
        mutex_acquired = true;
    }
#endif

    if (FSP_SUCCESS == err)
    {
        err = rs_i2c_if_bus_reconfigure(p_api_ctrl);
    }

#if BSP_CFG_RTOS
    if ((FSP_SUCCESS == err) && (NULL != p_ctrl->p_bus->p_blocking_semaphore))
    {
        fsp_err_t drain_err = rs_i2c_if_os_semaphore_acquire(p_ctrl->p_bus->p_blocking_semaphore, 0U);
        if ((FSP_SUCCESS != drain_err) && (FSP_ERR_TIMEOUT != drain_err))
        {
            err = drain_err;
        }
    }
#endif

    if (FSP_SUCCESS == err)
    {
        p_ctrl->transfer_err         = false;   /* clear before the ISR can set it */
        p_ctrl->p_transfer_data      = write_read_params.p_dest;
        p_ctrl->transfer_data_bytes = write_read_params.dest_bytes;

        err = p_driver_instance->p_api->write(p_driver_instance->p_ctrl,
                                               write_read_params.p_src,
                                               write_read_params.src_bytes,
                                               true);
    }

#if BSP_CFG_RTOS
    if ((FSP_SUCCESS == err) && (NULL != p_ctrl->p_bus->p_blocking_semaphore))
    {
        err = rs_i2c_if_os_semaphore_acquire(p_ctrl->p_bus->p_blocking_semaphore,
                                              p_ctrl->p_cfg->semaphore_timeout);
        if (FSP_SUCCESS == err)
        {
            /* The callback gives the semaphore for errors too; report them. */
            if (p_ctrl->transfer_err)
            {
                err = FSP_ERR_ABORTED;
            }
        }
        else if ((FSP_ERR_TIMEOUT == err) && (NULL != p_driver_instance->p_api->abort))
        {
            (void) p_driver_instance->p_api->abort(p_driver_instance->p_ctrl);
        }
    }

    if (mutex_acquired)
    {
        fsp_err_t mutex_err =
            rs_i2c_if_os_recursive_mutex_release(p_ctrl->p_bus->p_bus_recursive_mutex);
        if ((FSP_SUCCESS == err) && (FSP_SUCCESS != mutex_err))
        {
            err = mutex_err;
        }
    }
#endif

    if (FSP_SUCCESS != err)
    {
        p_ctrl->p_transfer_data      = NULL;
        p_ctrl->transfer_data_bytes = 0U;
    }

    return err;
}

/*******************************************************************************************************************//**
 * @brief Common callback function called in the I2C driver callback function.
 **********************************************************************************************************************/
void rs_i2c_if_callback (i2c_master_callback_args_t * p_args)
{
    fsp_err_t err = FSP_SUCCESS;
    rs_i2c_if_instance_ctrl_t * p_ctrl             = (rs_i2c_if_instance_ctrl_t *) p_args->p_context;
    i2c_master_instance_t      * p_driver_instance = (i2c_master_instance_t *) p_ctrl->p_bus->p_driver_instance;
    rs_i2c_if_callback_args_t  comms_i2c_args;

    /* Set context */
    comms_i2c_args.p_context = p_ctrl->p_context;

    /* Set event */
    switch (p_args->event)
    {
        case I2C_MASTER_EVENT_RX_COMPLETE:
        {
            comms_i2c_args.event = RS_I2C_IF_EVENT_OPERATION_COMPLETE;
            break;
        }

        case I2C_MASTER_EVENT_TX_COMPLETE:
        {
            comms_i2c_args.event = RS_I2C_IF_EVENT_OPERATION_COMPLETE;
            break;
        }

        case I2C_MASTER_EVENT_ABORTED:
        default:
        {
            /* Do not call abort() here: if the driver re-enters this callback from
             * abort(), the recursion would overflow the ISR stack. Record the
             * failure and let the waiting task handle recovery. */
            comms_i2c_args.event        = RS_I2C_IF_EVENT_ERROR;
            p_ctrl->p_transfer_data     = NULL;     /* drop any pending restart-read */
            p_ctrl->transfer_data_bytes = 0U;
            p_ctrl->transfer_err        = true;     /* checked by the task after the semaphore */
            break;
        }
    }

    if ((NULL != p_ctrl->p_transfer_data) &&
        (RS_I2C_IF_EVENT_OPERATION_COMPLETE == comms_i2c_args.event))
    {
        /* Read data with restart */
        err = p_driver_instance->p_api->read(p_driver_instance->p_ctrl,
                                             p_ctrl->p_transfer_data,
                                             p_ctrl->transfer_data_bytes,
                                             false);

        /* Clear transfer data */
        p_ctrl->p_transfer_data     = NULL;
        p_ctrl->transfer_data_bytes = 0;

        if (FSP_SUCCESS != err)
        {
            /* Set event */
            comms_i2c_args.event = RS_I2C_IF_EVENT_ERROR;

            /* Release semaphore in OS and call user callback function */
            rs_i2c_if_process_in_callback(p_ctrl, &comms_i2c_args);
        }
    }
    else
    {
        /* Release semaphore in OS and call user callback function */
        rs_i2c_if_process_in_callback(p_ctrl, &comms_i2c_args);
    }
}

/***********************************************************************************************************************
 * Private Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * Internal Communications Middleware I2C driver private function.
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief Process in callback function. Release semaphores in RTOS and call user callback.
 **********************************************************************************************************************/
void rs_i2c_if_process_in_callback (rs_i2c_if_ctrl_t * const p_api_ctrl, rs_i2c_if_callback_args_t * const p_args)
{
    rs_i2c_if_instance_ctrl_t * p_ctrl = (rs_i2c_if_instance_ctrl_t *) p_api_ctrl;

    /* Runs in ISR context: never dereference unchecked pointers. */
    if ((NULL == p_ctrl) || (NULL == p_args))
    {
        return;
    }

    /* Latch the result first, so the waiting task (and the user callback) see a consistent state. */
    p_ctrl->transfer_err = (RS_I2C_IF_EVENT_ERROR == p_args->event);

    if (p_ctrl->transfer_err)
    {
        /* Never leave a restart-read pending after a failed transfer. */
        p_ctrl->p_transfer_data     = NULL;
        p_ctrl->transfer_data_bytes = 0U;
    }

    /* Call the user callback before waking the task. */
    if (NULL != p_ctrl->p_callback)
    {
        p_ctrl->p_callback(p_args);
    }

#if BSP_CFG_RTOS
    /* Release the blocking semaphore last. The give may fail if the binary
     * semaphore is already given; that is harmless, so the result is ignored. */
    if ((NULL != p_ctrl->p_bus) && (NULL != p_ctrl->p_bus->p_blocking_semaphore))
    {
        (void) rs_i2c_if_os_semaphore_release_from_ISR(p_ctrl->p_bus->p_blocking_semaphore);
    }
#endif
}

/*******************************************************************************************************************//**
 * @brief Reconfigure I2C bus.
 *
 * @retval FSP_SUCCESS              successfully configured.
 **********************************************************************************************************************/
static fsp_err_t rs_i2c_if_bus_reconfigure (rs_i2c_if_ctrl_t * const p_api_ctrl)
{
    fsp_err_t err;

    FSP_ERROR_RETURN(NULL != p_api_ctrl, FSP_ERR_ASSERTION);

    rs_i2c_if_instance_ctrl_t * p_ctrl = (rs_i2c_if_instance_ctrl_t *) p_api_ctrl;

    FSP_ERROR_RETURN(NULL != p_ctrl->p_bus, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_ctrl->p_lower_level_cfg, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_ctrl->p_bus->p_driver_instance, FSP_ERR_ASSERTION);

    i2c_master_instance_t * p_driver_instance =
        (i2c_master_instance_t *) p_ctrl->p_bus->p_driver_instance;
    i2c_master_cfg_t * p_lower_level_cfg =
        (i2c_master_cfg_t *) p_ctrl->p_lower_level_cfg;

    FSP_ERROR_RETURN(NULL != p_driver_instance->p_api, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_driver_instance->p_api->slaveAddressSet, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_driver_instance->p_api->callbackSet, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_driver_instance->p_ctrl, FSP_ERR_ASSERTION);

    /* The caller owns the bus mutex for the complete transaction. */
    if (p_ctrl->p_bus->p_current_ctrl != (rs_i2c_if_ctrl_t *) p_ctrl)
    {
        err = p_driver_instance->p_api->slaveAddressSet(p_driver_instance->p_ctrl,
                                                         p_lower_level_cfg->slave,
                                                         p_lower_level_cfg->addr_mode);
        if (FSP_SUCCESS != err)
        {
            return err;
        }

        err = p_driver_instance->p_api->callbackSet(p_driver_instance->p_ctrl,
                                                     p_lower_level_cfg->p_callback,
                                                     p_ctrl,
                                                     NULL);
        if (FSP_SUCCESS != err)
        {
            return err;
        }

        p_ctrl->p_bus->p_current_ctrl = (rs_i2c_if_ctrl_t *) p_ctrl;
    }

    return FSP_SUCCESS;
}


#if BSP_CFG_RTOS

/**********************************************************************************************************************
 * @brief Acquire a recursive mutex.
 *
 * @retval FSP_SUCCESS              successfully configured.
 * @retval FSP_ERR_INTERNAL         RTOS internal error.
 *********************************************************************************************************************/
fsp_err_t rs_i2c_if_os_recursive_mutex_acquire (rs_i2c_if_mutex_t const * p_mutex,
                                                       uint32_t const            timeout)
{
    FSP_ERROR_RETURN(NULL != p_mutex, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_mutex->p_mutex_handle, FSP_ERR_ASSERTION);

 #if BSP_CFG_RTOS == 1                 /* ThreadX */
    UINT status = tx_mutex_get(p_mutex->p_mutex_handle, (ULONG) timeout);
    if (TX_SUCCESS != status)
    {
        return FSP_ERR_TIMEOUT;
    }
 #elif BSP_CFG_RTOS == 2               /* FreeRTOS */
    FSP_ERROR_RETURN(NULL != *(p_mutex->p_mutex_handle), FSP_ERR_ASSERTION);
    BaseType_t sem_err = xSemaphoreTakeRecursive(*(p_mutex->p_mutex_handle), (TickType_t) timeout);
    if (pdTRUE != sem_err)
    {
        return FSP_ERR_TIMEOUT;
    }
 #endif

    return FSP_SUCCESS;
}

/**********************************************************************************************************************
 * @brief Release a recursive mutex.
 *
 * @retval FSP_SUCCESS              successfully configured.
 * @retval FSP_ERR_INTERNAL         RTOS internal error.
 *********************************************************************************************************************/
fsp_err_t rs_i2c_if_os_recursive_mutex_release (rs_i2c_if_mutex_t const * p_mutex)
{
    FSP_ERROR_RETURN(NULL != p_mutex, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_mutex->p_mutex_handle, FSP_ERR_ASSERTION);

 #if BSP_CFG_RTOS == 1                 /* ThreadX */
    UINT status = tx_mutex_put(p_mutex->p_mutex_handle);
    FSP_ERROR_RETURN(TX_SUCCESS == status, FSP_ERR_INTERNAL);
 #elif BSP_CFG_RTOS == 2               /* FreeRTOS */
    FSP_ERROR_RETURN(NULL != *(p_mutex->p_mutex_handle), FSP_ERR_ASSERTION);
    BaseType_t sem_err = xSemaphoreGiveRecursive(*(p_mutex->p_mutex_handle));
    FSP_ERROR_RETURN(pdTRUE == sem_err, FSP_ERR_INTERNAL);
 #endif

    return FSP_SUCCESS;
}

/**********************************************************************************************************************
 * @brief Acquire a semaphore.
 *
 * @retval FSP_SUCCESS              successfully configured.
 * @retval FSP_ERR_INTERNAL         RTOS internal error.
 *********************************************************************************************************************/
static fsp_err_t rs_i2c_if_os_semaphore_acquire (rs_i2c_if_semaphore_t const * p_semaphore,
                                                      uint32_t const                   timeout)
{
    FSP_ERROR_RETURN(NULL != p_semaphore, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_semaphore->p_semaphore_handle, FSP_ERR_ASSERTION);

 #if BSP_CFG_RTOS == 1                 /* ThreadX */
    UINT status = tx_semaphore_get(p_semaphore->p_semaphore_handle, (ULONG) timeout);
    if (TX_SUCCESS != status)
    {
        return FSP_ERR_TIMEOUT;
    }
 #elif BSP_CFG_RTOS == 2               /* FreeRTOS */
    FSP_ERROR_RETURN(NULL != *(p_semaphore->p_semaphore_handle), FSP_ERR_ASSERTION);
    BaseType_t sem_err = xSemaphoreTake(*(p_semaphore->p_semaphore_handle), (TickType_t) timeout);
    if (pdTRUE != sem_err)
    {
        return FSP_ERR_TIMEOUT;
    }
 #endif

    return FSP_SUCCESS;
}

/**********************************************************************************************************************
 * @brief Release a semaphore from an interrupt.
 *
 * @retval FSP_SUCCESS              successfully configured.
 * @retval FSP_ERR_INTERNAL         RTOS internal error.
 *********************************************************************************************************************/
static fsp_err_t rs_i2c_if_os_semaphore_release_from_ISR (rs_i2c_if_semaphore_t const * p_semaphore)
{
    if ((NULL == p_semaphore) || (NULL == p_semaphore->p_semaphore_handle))
    {
        return FSP_ERR_ASSERTION;
    }

 #if BSP_CFG_RTOS == 1                 /* ThreadX */
    UINT status = tx_semaphore_put(p_semaphore->p_semaphore_handle);
    return (TX_SUCCESS == status) ? FSP_SUCCESS : FSP_ERR_INTERNAL;
 #elif BSP_CFG_RTOS == 2               /* FreeRTOS */
    if (NULL == *(p_semaphore->p_semaphore_handle))
    {
        return FSP_ERR_ASSERTION;
    }

    BaseType_t higher_priority_task_woken = pdFALSE;
    BaseType_t sem_err = xSemaphoreGiveFromISR(*(p_semaphore->p_semaphore_handle),
                                               &higher_priority_task_woken);
    if (pdTRUE != sem_err)
    {
        return FSP_ERR_INTERNAL;
    }

    portYIELD_FROM_ISR(higher_priority_task_woken);
    return FSP_SUCCESS;
 #endif
}

#endif
/*******************************************************************************************************************//**
 * @} (end addtogroup rs_i2c_if)
 **********************************************************************************************************************/
