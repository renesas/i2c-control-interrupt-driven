/***********************************************************************************************************************
 * File Name    : rm_icm42670_ra_driver.c
 * Description  : Contains function definition.
 **********************************************************************************************************************/
/***********************************************************************************************************************
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************************************************************/
/**********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 *********************************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include "rm_icm42670.h"
#include "rm_icm42670_hal_data.h"
#include "r_icu.h"

/**********************************************************************************************************************
 * Macro definitions
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Local Typedef definitions
 *********************************************************************************************************************/

/**********************************************************************************************************************
 * Exported global variables
 *********************************************************************************************************************/
fsp_err_t rm_icm42670_delay_ms(rm_icm42670_ctrl_t * const p_ctrl, uint32_t const delay_ms);
fsp_err_t rm_icm42670_irq_open(rm_icm42670_ctrl_t * const p_api_ctrl);
fsp_err_t rm_icm42670_irq_close(rm_icm42670_ctrl_t * const p_api_ctrl);
void      rm_icm42670_irq_callback(external_irq_callback_args_t * p_args);

#if BSP_CFG_RTOS
fsp_err_t rm_icm42670_os_semaphore_create(rm_icm42670_semaphore_t const * p_semaphore);
fsp_err_t rm_icm42670_os_semaphore_delete(rm_icm42670_semaphore_t const * p_semaphore);
fsp_err_t rm_icm42670_os_semaphore_acquire(rm_icm42670_semaphore_t const * p_semaphore, uint32_t const timeout);
fsp_err_t rm_icm42670_os_semaphore_release_from_ISR(rm_icm42670_semaphore_t const * p_semaphore);

#endif
extern fsp_err_t rm_icm42670_all_interrupt_bits_clear(rm_icm42670_ctrl_t * const p_api_ctrl);
/**********************************************************************************************************************
 * Private (static) variables and functions
 *********************************************************************************************************************/

/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @brief Delay some milliseconds.
 *
 * @retval FSP_SUCCESS              successfully configured.
 **********************************************************************************************************************/
fsp_err_t rm_icm42670_delay_ms (rm_icm42670_ctrl_t * const p_ctrl, uint32_t const delay_ms)
{
    FSP_PARAMETER_NOT_USED(p_ctrl);

#if BSP_CFG_RTOS == 1
    /* ThreadX: yield the CPU while waiting. Convert milliseconds to ticks, rounding up. */
    ULONG ticks = (ULONG) (((uint64_t) delay_ms * (uint64_t) TX_TIMER_TICKS_PER_SECOND + 999ULL) / 1000ULL);
    if ((0U == ticks) && (0U != delay_ms))
    {
        ticks = 1U;
    }
    tx_thread_sleep(ticks);
#elif BSP_CFG_RTOS == 2
    /* FreeRTOS: use an RTOS delay rather than busy-waiting inside a task. */
    TickType_t ticks = pdMS_TO_TICKS(delay_ms);
    if ((0U == ticks) && (0U != delay_ms))
    {
        ticks = 1U;
    }
    vTaskDelay(ticks);
#else
    R_BSP_SoftwareDelay(delay_ms, BSP_DELAY_UNITS_MILLISECONDS);
#endif

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief Open IRQ driver and set callback function.
 *
 * @retval FSP_SUCCESS              successfully configured.
 **********************************************************************************************************************/
fsp_err_t rm_icm42670_irq_open (rm_icm42670_ctrl_t * const p_api_ctrl)
{
    fsp_err_t err = FSP_SUCCESS;
    rm_icm42670_instance_ctrl_t * p_ctrl         = (rm_icm42670_instance_ctrl_t *) p_api_ctrl;
    external_irq_instance_t   * p_irq_instance;

    FSP_ASSERT(NULL != p_ctrl);
    FSP_ASSERT(NULL != p_ctrl->p_irq_instance);
    p_irq_instance = (external_irq_instance_t *) p_ctrl->p_irq_instance;
    FSP_ASSERT(NULL != p_irq_instance->p_api);

    /* Open IRQ driver */
    err = p_irq_instance->p_api->open(p_irq_instance->p_ctrl, p_irq_instance->p_cfg);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* Set callback function and current control block */
    err = p_irq_instance->p_api->callbackSet(p_irq_instance->p_ctrl, rm_icm42670_irq_callback, p_ctrl, NULL);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    /* Enable IRQ */
    err = p_irq_instance->p_api->enable(p_irq_instance->p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief Close IRQ driver.
 *
 * @retval FSP_SUCCESS              successfully configured.
 **********************************************************************************************************************/
fsp_err_t rm_icm42670_irq_close (rm_icm42670_ctrl_t * const p_api_ctrl)
{
    fsp_err_t err;
    rm_icm42670_instance_ctrl_t * p_ctrl = (rm_icm42670_instance_ctrl_t *) p_api_ctrl;
    external_irq_instance_t * p_irq_instance;

    FSP_ASSERT(NULL != p_ctrl);
    FSP_ASSERT(NULL != p_ctrl->p_irq_instance);
    p_irq_instance = (external_irq_instance_t *) p_ctrl->p_irq_instance;
    FSP_ASSERT(NULL != p_irq_instance->p_api);

    /* Close IRQ driver and propagate the driver's result. */
    err = p_irq_instance->p_api->close(p_irq_instance->p_ctrl);
    FSP_ERROR_RETURN(FSP_SUCCESS == err, err);

    return FSP_SUCCESS;
}

void rm_icm42670_irq_callback (external_irq_callback_args_t * p_args)
{
    rm_icm42670_instance_ctrl_t * p_ctrl;
    rm_icm42670_callback_args_t icm42670_callback_args = {0};

    if ((NULL == p_args) || (NULL == p_args->p_context))
    {
        return;
    }

    p_ctrl = (rm_icm42670_instance_ctrl_t *) p_args->p_context;
    if (NULL == p_ctrl->p_mode)
    {
        return;
    }

    /* Set context */
    icm42670_callback_args.p_context = p_ctrl->p_context;

    /* Set event */
    switch (p_ctrl->p_mode->mode_irq)
    {
        case RM_ICM42670_OPERATION_MODE_ACCEL_LP:
        {
            icm42670_callback_args.event = RM_ICM42670_EVENT_ACCEL_LPMODE_COMPLETE;
            break;
        }

        case RM_ICM42670_OPERATION_MODE_ACCEL_LN:
        case RM_ICM42670_OPERATION_MODE_GYRO_LN:
        case RM_ICM42670_OPERATION_MODE_6AXIS:
        {
            icm42670_callback_args.event = RM_ICM42670_EVENT_MEASUREMENT_COMPLETE;
            break;
        }

        default:
        {
            icm42670_callback_args.event = RM_ICM42670_EVENT_ERROR;
            break;
        }
    }


    if (NULL != p_ctrl->p_irq_callback)
    {
        /* Call callback function */
        p_ctrl->p_irq_callback(&icm42670_callback_args);
    }
}

#if BSP_CFG_RTOS
/**********************************************************************************************************************
 * @brief Create the binary/counting semaphore used for I2C transaction completion.
 *********************************************************************************************************************/
fsp_err_t rm_icm42670_os_semaphore_create (rm_icm42670_semaphore_t const * p_semaphore)
{
    FSP_ASSERT(NULL != p_semaphore);
    FSP_ASSERT(NULL != p_semaphore->p_semaphore_handle);

 #if BSP_CFG_RTOS == 1
    UINT status = tx_semaphore_create(p_semaphore->p_semaphore_handle,
                                      p_semaphore->p_semaphore_name,
                                      (ULONG) 0);
    FSP_ERROR_RETURN(TX_SUCCESS == status, FSP_ERR_INTERNAL);
 #elif BSP_CFG_RTOS == 2
    FSP_ASSERT(NULL != p_semaphore->p_semaphore_memory);
    *(p_semaphore->p_semaphore_handle) =
        xSemaphoreCreateCountingStatic((UBaseType_t) 1,
                                       (UBaseType_t) 0,
                                       p_semaphore->p_semaphore_memory);
    FSP_ERROR_RETURN(NULL != *(p_semaphore->p_semaphore_handle), FSP_ERR_INTERNAL);
 #endif

    return FSP_SUCCESS;
}

/**********************************************************************************************************************
 * @brief Delete the I2C transaction-completion semaphore.
 *********************************************************************************************************************/
fsp_err_t rm_icm42670_os_semaphore_delete (rm_icm42670_semaphore_t const * p_semaphore)
{
    FSP_ASSERT(NULL != p_semaphore);
    FSP_ASSERT(NULL != p_semaphore->p_semaphore_handle);

 #if BSP_CFG_RTOS == 1
    UINT status = tx_semaphore_delete(p_semaphore->p_semaphore_handle);
    FSP_ERROR_RETURN(TX_SUCCESS == status, FSP_ERR_INTERNAL);
 #elif BSP_CFG_RTOS == 2
    FSP_ERROR_RETURN(NULL != *(p_semaphore->p_semaphore_handle), FSP_ERR_NOT_OPEN);
    vSemaphoreDelete(*(p_semaphore->p_semaphore_handle));
    *(p_semaphore->p_semaphore_handle) = NULL;
 #endif

    return FSP_SUCCESS;
}

/**********************************************************************************************************************
 * @brief Wait for I2C transaction completion.
 *
 * @param[in] timeout Timeout in RTOS ticks, not milliseconds.
 * @retval FSP_SUCCESS      Semaphore acquired.
 * @retval FSP_ERR_TIMEOUT  Timeout expired.
 *********************************************************************************************************************/
fsp_err_t rm_icm42670_os_semaphore_acquire (rm_icm42670_semaphore_t const * p_semaphore, uint32_t const timeout)
{
    FSP_ASSERT(NULL != p_semaphore);
    FSP_ASSERT(NULL != p_semaphore->p_semaphore_handle);

 #if BSP_CFG_RTOS == 1
    UINT status = tx_semaphore_get(p_semaphore->p_semaphore_handle, (ULONG) timeout);
    if (TX_NO_INSTANCE == status)
    {
        return FSP_ERR_TIMEOUT;
    }
    FSP_ERROR_RETURN(TX_SUCCESS == status, FSP_ERR_INTERNAL);
 #elif BSP_CFG_RTOS == 2
    FSP_ERROR_RETURN(NULL != *(p_semaphore->p_semaphore_handle), FSP_ERR_NOT_OPEN);
    BaseType_t status = xSemaphoreTake(*(p_semaphore->p_semaphore_handle), (TickType_t) timeout);
    if (pdTRUE != status)
    {
        return FSP_ERR_TIMEOUT;
    }
 #endif

    return FSP_SUCCESS;
}

/**********************************************************************************************************************
 * @brief Signal I2C transaction completion from ISR context.
 *********************************************************************************************************************/
fsp_err_t rm_icm42670_os_semaphore_release_from_ISR (rm_icm42670_semaphore_t const * p_semaphore)
{
    FSP_ASSERT(NULL != p_semaphore);
    FSP_ASSERT(NULL != p_semaphore->p_semaphore_handle);

 #if BSP_CFG_RTOS == 1
    /* ThreadX semaphore put is ISR-callable. */
    UINT status = tx_semaphore_put(p_semaphore->p_semaphore_handle);
    FSP_ERROR_RETURN(TX_SUCCESS == status, FSP_ERR_INTERNAL);
 #elif BSP_CFG_RTOS == 2
    FSP_ERROR_RETURN(NULL != *(p_semaphore->p_semaphore_handle), FSP_ERR_NOT_OPEN);
    BaseType_t higher_priority_task_woken = pdFALSE;
    BaseType_t status = xSemaphoreGiveFromISR(*(p_semaphore->p_semaphore_handle),
                                              &higher_priority_task_woken);
    FSP_ERROR_RETURN(pdTRUE == status, FSP_ERR_INTERNAL);
    portYIELD_FROM_ISR(higher_priority_task_woken);
 #endif

    return FSP_SUCCESS;
}
#endif
/**********************************************************************************************************************
 * user irq callback

 *********************************************************************************************************************/
void icm42670_irq_callback (rm_icm42670_callback_args_t * p_args)
{
    if ((NULL != p_args) && (RM_ICM42670_EVENT_ERROR != p_args->event))
    {
        g_irq_flag = 1;
    }

}
