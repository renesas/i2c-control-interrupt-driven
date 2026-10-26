/***********************************************************************************************************************
 * File Name    : sensor_ctrl.c
 * Description  : Contains function definition.
 **********************************************************************************************************************/
/***********************************************************************************************************************
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************************************************************/
#include <rs_i2c_if/rs_i2c_if.h>
#include "rm_icm42670/rm_icm42670_hal_data.h"
#include <rs_i2c_if/sensor_ctrl/sensor_ctrl.h>

/*******************************************************************************************************************//**
 * @brief Resets the sensor read context to its default uninitialized state.
 *
 * The function clears the initialization state and resets the stored sensor control, configuration, and external IRQ
 * pointers. When FreeRTOS is enabled, it deletes the IRQ semaphore if one exists and clears the semaphore handle. In a
 * bare-metal configuration, it clears the interrupt flag.
 *
 * @param[in,out] p_context  Pointer to the sensor read context to reset.
 *
 * @note When FreeRTOS is enabled, any existing IRQ semaphore associated with the context is deleted before its handle
 *       is set to NULL.
 **********************************************************************************************************************/
static void rs_i2c_if_sensor_context_reset(rs_i2c_if_sensor_read_context_t * const p_context)
{
    p_context->initialized = false;
    p_context->p_ctrl = NULL;
#if BSP_CFG_RTOS == 2
    if (NULL != p_context->irq_semaphore)
    {
        vSemaphoreDelete(p_context->irq_semaphore);
        p_context->irq_semaphore = NULL;
    }
#else
    p_context->irq_flag = false;
#endif
}

/*******************************************************************************************************************//**
 * @brief Cleans up sensor resources after an initialization failure.
 *
 * The function marks the read context as uninitialized, disables and closes the external IRQ instance if it was
 * successfully opened, closes the sensor if it was successfully opened, and resets the read context to its
 * default state. The @p sensor_opened and @p irq_opened flags ensure that cleanup is performed only for resources that
 * were opened during initialization.
 *
 * @param[in,out] p_context      Pointer to the sensor read context to reset.
 * @param[in,out] p_ctrl         Pointer to the sensor driver control structure.
 * @param[in]     p_irq          Pointer to the external IRQ instance.
 * @param[in]     sensor_opened  Indicates whether the sensor sensor was successfully opened.
 * @param[in]     irq_opened     Indicates whether the external IRQ instance was successfully opened.
 *
 * @note Errors returned while disabling or closing the IRQ or sensor are intentionally ignored because this function
 *       is used to clean up resources after an initialization failure.
 **********************************************************************************************************************/
static void rs_i2c_if_sensor_init_cleanup(
    rs_i2c_if_sensor_read_context_t * const p_context,
    rm_icm42670_instance_ctrl_t * const p_ctrl,
    external_irq_instance_t const * const p_irq,
    bool sensor_opened,
    bool irq_opened)
{
    p_context->initialized = false;
    if (true == irq_opened)
    {
        (void) p_irq->p_api->disable(p_irq->p_ctrl);
        (void) p_irq->p_api->close(p_irq->p_ctrl);
    }
    if (true == sensor_opened)
    {
        (void) RM_ICM42670_Close(p_ctrl);
    }
    rs_i2c_if_sensor_context_reset(p_context);
}

#if BSP_CFG_RTOS == 2
/*******************************************************************************************************************//**
 * @brief Converts a timeout value in milliseconds to FreeRTOS ticks.
 *
 * The function converts @p timeout_ms to the corresponding FreeRTOS tick count. If a non-zero timeout converts to
 * zero ticks because it is shorter than one RTOS tick period, the timeout is rounded up to one tick.
 *
 * @param[in] timeout_ms  Timeout value in milliseconds.
 *
 * @return Timeout value converted to FreeRTOS ticks.
 **********************************************************************************************************************/
static TickType_t rs_i2c_if_timeout_to_ticks(uint32_t timeout_ms)
{
    TickType_t ticks = pdMS_TO_TICKS(timeout_ms);
    if ((timeout_ms > 0U) && (ticks == 0U))
    {
        ticks = (TickType_t) 1;
    }
    return ticks;
}

/*******************************************************************************************************************//**
 * @brief Removes any pending tokens from a sensor interrupt semaphore.
 *
 * The function repeatedly attempts to take @p semaphore without blocking until no semaphore token remains. This
 * clears any previously signaled interrupt event before starting a new wait operation.
 *
 * @param[in] semaphore  FreeRTOS semaphore handle to drain.
 *
 * @note No action is performed if @p semaphore is NULL.
 **********************************************************************************************************************/
static void rs_i2c_if_semaphore_drain(SemaphoreHandle_t semaphore)
{
    if (NULL != semaphore)
    {
        while (pdTRUE == xSemaphoreTake(semaphore, (TickType_t) 0))
        {
        }
    }
}
#endif

/*******************************************************************************************************************//**
 * @brief Generates the writable SRAM object hierarchy required by rs_i2c_if_SensorDataRead().
 *
 * @param[in]  p_src_comms_instance  Source RS I2C communications instance, normally FSP/generated const data.
 * @param[in]  p_src_sensor_instance Source ICM42670 instance, normally FSP/generated const data.
 * @param[in]  p_src_irq_instance    Source external IRQ instance, normally FSP/generated const data.
 * @param[out] p_dst                 Complete destination hierarchy in writable SRAM.
 *
 * @retval FSP_SUCCESS              The SRAM hierarchy was generated successfully.
 * @retval FSP_ERR_ASSERTION        A required pointer or nested source pointer is NULL.
 * @retval FSP_ERR_INVALID_ARGUMENT A source instance aliases storage inside the destination object.
 *
 * @note This function performs shallow copies, then redirects all mutable links needed by
 *       rs_i2c_if_SensorDataRead() to objects in @p p_dst. Read-only API tables remain in Flash.
 **********************************************************************************************************************/
fsp_err_t rs_i2c_if_SensorConfigGen (
    rs_i2c_if_instance_t const    * const p_src_comms_instance,
    rm_icm42670_instance_t const  * const p_src_sensor_instance,
    external_irq_instance_t const * const p_src_irq_instance,
    rs_i2c_if_sensor_ctrl_t  * const p_dst)
{
    rs_i2c_if_cfg_t const              * p_src_comms_cfg;
    i2c_master_cfg_t const             * p_src_lower_cfg;
    rs_i2c_if_bus_extended_cfg_t const * p_src_bus_cfg;
    rm_icm42670_cfg_t const            * p_src_sensor_cfg;
    external_irq_cfg_t const           * p_src_irq_cfg;

    FSP_ERROR_RETURN(NULL != p_src_comms_instance,  FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_src_sensor_instance, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_src_irq_instance,    FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_dst,                 FSP_ERR_ASSERTION);

    FSP_ERROR_RETURN((void const *) p_src_comms_instance  != (void const *) &p_dst->i2c_instance, FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN((void const *) p_src_sensor_instance != (void const *) &p_dst->sensor_instance, FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN((void const *) p_src_irq_instance    != (void const *) &p_dst->irq_instance, FSP_ERR_INVALID_ARGUMENT);

    FSP_ERROR_RETURN(NULL != p_src_comms_instance->p_cfg, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_src_comms_instance->p_api, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_src_sensor_instance->p_cfg, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_src_sensor_instance->p_api, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_src_irq_instance->p_cfg, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_src_irq_instance->p_api, FSP_ERR_ASSERTION);

    p_src_comms_cfg  = p_src_comms_instance->p_cfg;
    p_src_lower_cfg  = (i2c_master_cfg_t const *) p_src_comms_cfg->p_lower_level_cfg;
    p_src_bus_cfg    = (rs_i2c_if_bus_extended_cfg_t const *) p_src_comms_cfg->p_extend;
    p_src_sensor_cfg = p_src_sensor_instance->p_cfg;
    p_src_irq_cfg    = p_src_irq_instance->p_cfg;

    FSP_ERROR_RETURN(NULL != p_src_lower_cfg, FSP_ERR_ASSERTION);
    FSP_ERROR_RETURN(NULL != p_src_bus_cfg,   FSP_ERR_ASSERTION);

    /* Clear every runtime object before reconnecting the copied configuration hierarchy. */
    memset(p_dst, 0, sizeof(*p_dst));

    memcpy(&p_dst->i2c_instance,  p_src_comms_instance,  sizeof(p_dst->i2c_instance));
    memcpy(&p_dst->i2c_cfg,       p_src_comms_cfg,       sizeof(p_dst->i2c_cfg));
    memcpy(&p_dst->i2c_hal_cfg,       p_src_lower_cfg,     sizeof(p_dst->i2c_hal_cfg));
    memcpy(&p_dst->i2c_bus_cfg,         p_src_bus_cfg,     sizeof(p_dst->i2c_bus_cfg));
    memcpy(&p_dst->sensor_instance, p_src_sensor_instance, sizeof(p_dst->sensor_instance));
    memcpy(&p_dst->sensor_cfg,      p_src_sensor_cfg,      sizeof(p_dst->sensor_cfg));
    memcpy(&p_dst->irq_instance,    p_src_irq_instance,    sizeof(p_dst->irq_instance));
    memcpy(&p_dst->irq_cfg,         p_src_irq_cfg,         sizeof(p_dst->irq_cfg));

    /* Reconnect the writable RS I2C communications hierarchy. */
    p_dst->i2c_bus_cfg.p_current_ctrl       = NULL;
    p_dst->i2c_cfg.p_lower_level_cfg  = (void *) &p_dst->i2c_hal_cfg;
    p_dst->i2c_cfg.p_extend           = (void *) &p_dst->i2c_bus_cfg;
    p_dst->i2c_instance.p_ctrl        = (rs_i2c_if_ctrl_t *) &p_dst->i2c_ctrl;
    p_dst->i2c_instance.p_cfg         = &p_dst->i2c_cfg;

    /* Reconnect the writable sensor hierarchy. */
    p_dst->sensor_cfg.p_comms_instance  = &p_dst->i2c_instance;
    p_dst->sensor_instance.p_ctrl       = (rm_icm42670_ctrl_t *) &p_dst->sensor_ctrl;
    p_dst->sensor_instance.p_cfg        = &p_dst->sensor_cfg;

    /* Reconnect the writable external IRQ hierarchy. */
    p_dst->irq_instance.p_ctrl          = (external_irq_ctrl_t *) &p_dst->irq_ctrl;
    p_dst->irq_instance.p_cfg           = &p_dst->irq_cfg;

    return FSP_SUCCESS;
}
/*******************************************************************************************************************//**
 * @brief Validates the SRAM arguments and pointer topology required by rs_i2c_if_SensorDataRead().
 *
 * @param[in] p_src_comms_instance  Original RS I2C source instance.
 * @param[in] p_src_sensor_instance Original sensor source instance.
 * @param[in] p_src_irq_instance    Original external IRQ source instance.
 * @param[in] p_dst                 Generated SRAM hierarchy to validate.
 *
 * @retval FSP_SUCCESS              Every required argument and link is valid.
 * @retval FSP_ERR_INVALID_POINTER  A required pointer is NULL.
 * @retval FSP_ERR_INVALID_ARGUMENT A pointer link, API table, or copied key configuration value is invalid.
 **********************************************************************************************************************/
fsp_err_t rs_i2c_if_SensorConfigCheck (
    rs_i2c_if_instance_t const          * const p_src_comms_instance,
    rm_icm42670_instance_t const        * const p_src_sensor_instance,
    external_irq_instance_t const       * const p_src_irq_instance,
    rs_i2c_if_sensor_ctrl_t const  * const p_dst)
{
    i2c_master_cfg_t const * p_src_lower_cfg;
    external_irq_cfg_t const * p_src_irq_cfg;

    FSP_ERROR_RETURN(NULL != p_src_comms_instance,  FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN(NULL != p_src_sensor_instance, FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN(NULL != p_src_irq_instance,    FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN(NULL != p_dst,                 FSP_ERR_INVALID_POINTER);

    FSP_ERROR_RETURN(NULL != p_src_comms_instance->p_cfg,  FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN(NULL != p_src_sensor_instance->p_cfg, FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN(NULL != p_src_irq_instance->p_cfg,    FSP_ERR_INVALID_POINTER);

    p_src_lower_cfg = (i2c_master_cfg_t const *) p_src_comms_instance->p_cfg->p_lower_level_cfg;
    p_src_irq_cfg   = p_src_irq_instance->p_cfg;

    FSP_ERROR_RETURN(NULL != p_src_lower_cfg, FSP_ERR_INVALID_POINTER);

    /* Validate objects passed directly to rs_i2c_if_SensorDataRead(). */
    FSP_ERROR_RETURN(NULL != p_dst->sensor_instance.p_ctrl, FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN(NULL != p_dst->sensor_instance.p_cfg,  FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN(NULL != p_dst->sensor_instance.p_api,  FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN(NULL != p_dst->irq_instance.p_ctrl,    FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN(NULL != p_dst->irq_instance.p_cfg,     FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN(NULL != p_dst->irq_instance.p_api,     FSP_ERR_INVALID_POINTER);

    /* Validate sensor instance links. */
    FSP_ERROR_RETURN(p_dst->sensor_instance.p_ctrl == (rm_icm42670_ctrl_t *) &p_dst->sensor_ctrl, FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN(p_dst->sensor_instance.p_cfg == &p_dst->sensor_cfg, FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN(p_dst->sensor_instance.p_api == p_src_sensor_instance->p_api, FSP_ERR_INVALID_ARGUMENT);

    /* Validate sensor -> communications link. */
    FSP_ERROR_RETURN(p_dst->sensor_cfg.p_comms_instance == &p_dst->i2c_instance, FSP_ERR_INVALID_ARGUMENT);

    /* Validate communications instance and nested configuration links. */
    FSP_ERROR_RETURN(p_dst->i2c_instance.p_ctrl == (rs_i2c_if_ctrl_t *) &p_dst->i2c_ctrl, FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN(p_dst->i2c_instance.p_cfg == &p_dst->i2c_cfg, FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN(p_dst->i2c_instance.p_api == p_src_comms_instance->p_api, FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN(p_dst->i2c_cfg.p_lower_level_cfg == (void const *) &p_dst->i2c_hal_cfg, FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN(p_dst->i2c_cfg.p_extend == (void const *) &p_dst->i2c_bus_cfg, FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN(NULL == p_dst->i2c_bus_cfg.p_current_ctrl, FSP_ERR_INVALID_ARGUMENT);

    /* Validate key copied I2C values. */
    FSP_ERROR_RETURN(p_dst->i2c_hal_cfg.channel   == p_src_lower_cfg->channel,   FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN(p_dst->i2c_hal_cfg.rate      == p_src_lower_cfg->rate,      FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN(p_dst->i2c_hal_cfg.addr_mode == p_src_lower_cfg->addr_mode, FSP_ERR_INVALID_ARGUMENT);

    /* Validate external IRQ instance and key copied IRQ values. */
    FSP_ERROR_RETURN(p_dst->irq_instance.p_ctrl == (external_irq_ctrl_t *) &p_dst->irq_ctrl, FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN(p_dst->irq_instance.p_cfg == &p_dst->irq_cfg,                 FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN(p_dst->irq_instance.p_api == p_src_irq_instance->p_api,       FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN(p_dst->irq_cfg.channel       == p_src_irq_cfg->channel,       FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN(p_dst->irq_cfg.trigger       == p_src_irq_cfg->trigger,       FSP_ERR_INVALID_ARGUMENT);
    FSP_ERROR_RETURN(p_dst->irq_cfg.filter_enable == p_src_irq_cfg->filter_enable, FSP_ERR_INVALID_ARGUMENT);

    /* Validate API entries dereferenced by rs_i2c_if_SensorDataRead(). */
    FSP_ERROR_RETURN(NULL != p_dst->sensor_instance.p_api->open,                  FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN(NULL != p_dst->sensor_instance.p_api->deviceInterruptCfgSet, FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN(NULL != p_dst->sensor_instance.p_api->measurementStart,      FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN(NULL != p_dst->sensor_instance.p_api->accelRead,             FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN(NULL != p_dst->sensor_instance.p_api->accelDataCalculate,    FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN(NULL != p_dst->irq_instance.p_api->open,                     FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN(NULL != p_dst->irq_instance.p_api->callbackSet,              FSP_ERR_INVALID_POINTER);
    FSP_ERROR_RETURN(NULL != p_dst->irq_instance.p_api->enable,                   FSP_ERR_INVALID_POINTER);

    return FSP_SUCCESS;
}

/*******************************************************************************************************************//**
 * @brief Initializes the sensor interface when required, waits for data-ready,
 *        and reads converted accelerometer data.
 *
 * On the first call, this function validates and initializes the I2C driver,
 * sensor driver, external IRQ, synchronization object, sensor interrupt, and
 * measurement operation. Subsequent calls reuse the initialized context and
 * execute only the data-ready wait, raw-data read, and data conversion.
 *
 * @param[in,out] p_context          Persistent sensor-read context.
 * @param[in]     p_sensor_instance  Pointer to the sensor instance.
 * @param[in]     p_interrupt_cfg    Pointer to the sensor interrupt configuration.
 * @param[in]     p_irq              External data-ready IRQ instance.
 * @param[out]    p_sensor_data      Destination for converted sensor data.
 * @param[in]     timeout_ms         Data-ready timeout in milliseconds.
 *
 * @retval FSP_SUCCESS            Data was read and converted successfully.
 * @retval FSP_ERR_ASSERTION      A required pointer or configuration is invalid.
 * @retval FSP_ERR_OUT_OF_MEMORY  The static semaphore could not be created.
 * @retval FSP_ERR_TIMEOUT        The data-ready event was not received in time.
 * @return                        Error returned by an underlying driver.
 *
 * @note The sensor instance, interrupt configuration, and IRQ instance are used
 *       only during the first call for a context.
 * @note The context must be initialized to zero before its first use.
 * @note A context must not be shared concurrently by multiple tasks.
 **********************************************************************************************************************/
 fsp_err_t rs_i2c_if_SensorDataRead(
     rs_i2c_if_sensor_read_context_t * const p_context,
     void const * const                       p_sensor_instance,
     void const * const                       p_interrupt_cfg,
     external_irq_instance_t const * const    p_irq,
     void * const                             p_sensor_data,
     uint32_t                                 timeout_ms)
 {
     fsp_err_t err;

     /*
      * These pointers are dereferenced on every call.
      */
     FSP_ERROR_RETURN(NULL != p_context,     FSP_ERR_ASSERTION);
     FSP_ERROR_RETURN(NULL != p_sensor_data, FSP_ERR_ASSERTION);

     /*
      * Perform configuration validation and hardware initialization once.
      */
     if (false == p_context->initialized)
     {
         bool sensor_opened = false;
         bool irq_opened    = false;

         rm_icm42670_instance_t const * p_instance;
         rm_icm42670_cfg_t const * p_sensor_cfg;
         rm_icm42670_device_interrupt_cfg_t interrupt_cfg;

         rs_i2c_if_bus_extended_cfg_t const * p_extend;
         i2c_master_instance_t const * p_driver_instance;

         /*
          * These parameters are required only during initialization.
          */
         FSP_ERROR_RETURN(NULL != p_sensor_instance, FSP_ERR_ASSERTION);
         FSP_ERROR_RETURN(NULL != p_interrupt_cfg,   FSP_ERR_ASSERTION);
         FSP_ERROR_RETURN(NULL != p_irq,             FSP_ERR_ASSERTION);

         /*
          * Convert generic initialization objects to sensor-specific types.
          */
         p_instance = (rm_icm42670_instance_t const *) p_sensor_instance;
         interrupt_cfg = *(rm_icm42670_device_interrupt_cfg_t const *) p_interrupt_cfg;

         /*
          * Cache only objects required by the repeated read path.
          *
          * The sensor configuration and IRQ instance are needed only during
          * initialization, so they are not stored in the read context.
          */
         p_context->p_ctrl = (rm_icm42670_instance_ctrl_t *) p_instance->p_ctrl;
         p_context->p_api  = p_instance->p_api;
         p_sensor_cfg      = p_instance->p_cfg;

         p_extend          = (rs_i2c_if_bus_extended_cfg_t const *) p_sensor_cfg->p_comms_instance->p_cfg->p_extend;
         p_driver_instance = (i2c_master_instance_t const *)p_extend->p_driver_instance;

         /*
          * Clear persistent runtime data before enabling any interrupts.
          */
         memset(&p_context->raw_data, 0, sizeof(p_context->raw_data));

 #if BSP_CFG_RTOS == 2

         /*
          * Create the static binary semaphore once.
          */
         p_context->irq_semaphore = xSemaphoreCreateBinaryStatic( &p_context->irq_semaphore_memory);
         if (NULL == p_context->irq_semaphore)
         {
             rs_i2c_if_sensor_context_reset(p_context);

             return FSP_ERR_OUT_OF_MEMORY;
         }

 #else

         p_context->irq_flag = false;

 #endif

         /*
          * Open the shared lower-level I2C driver.
          *
          * FSP_ERR_ALREADY_OPEN is accepted because the I2C bus may be shared
          * with another sensor instance.
          */
         err = p_driver_instance->p_api->open( p_driver_instance->p_ctrl, p_driver_instance->p_cfg);

         if ((FSP_SUCCESS != err) && (FSP_ERR_ALREADY_OPEN != err))
         {
             rs_i2c_if_sensor_context_reset(p_context);

             return err;
         }

         /*
          * Open the sensor driver.
          */
         err = p_context->p_api->open( p_context->p_ctrl, p_sensor_cfg);
         if (FSP_SUCCESS != err)
         {
             rs_i2c_if_sensor_context_reset(p_context);

             return err;
         }

         sensor_opened = true;

         /*
          * Open the external data-ready IRQ.
          */
         err = p_irq->p_api->open(p_irq->p_ctrl, p_irq->p_cfg);
         if (FSP_SUCCESS != err)
         {
             rs_i2c_if_sensor_init_cleanup(p_context, p_context->p_ctrl, p_irq, sensor_opened, irq_opened);

             return err;
         }

         irq_opened = true;

         /*
          * Register the persistent read context with the IRQ callback.
          */
         err = p_irq->p_api->callbackSet(p_irq->p_ctrl, rs_i2c_if_SensorExternalIrqCallback, p_context, NULL);

         if (FSP_SUCCESS != err)
         {
             rs_i2c_if_sensor_init_cleanup(p_context, p_context->p_ctrl, p_irq, sensor_opened, irq_opened);

             return err;
         }

         /*
          * Enable the sensor data-ready interrupt without modifying the
          * caller's interrupt configuration object.
          */
         interrupt_cfg.int_config |= 0x01U;

         err = p_context->p_api->deviceInterruptCfgSet(p_context->p_ctrl, interrupt_cfg);

         if (FSP_SUCCESS != err)
         {
             rs_i2c_if_sensor_init_cleanup(p_context, p_context->p_ctrl, p_irq, sensor_opened, irq_opened);

             return err;
         }

         /*
          * Remove any stale data-ready event before measurement starts.
          */
 #if BSP_CFG_RTOS == 2

         rs_i2c_if_semaphore_drain(p_context->irq_semaphore);

 #else

         p_context->irq_flag = false;

 #endif

         /*
          * Start sensor measurement.
          */
         err = p_context->p_api->measurementStart(
             p_context->p_ctrl);

         if (FSP_SUCCESS != err)
         {
             rs_i2c_if_sensor_init_cleanup(p_context, p_context->p_ctrl, p_irq, sensor_opened, irq_opened);

             return err;
         }

         /*
          * Mark the context initialized before enabling the external IRQ.
          *
          * This is required because the IRQ callback checks initialized before
          * signaling the semaphore or bare-metal flag. If the IRQ fires
          * immediately after enable(), the event will not be discarded.
          */
         p_context->initialized = true;

         /*
          * Enable the external IRQ last.
          */
         err = p_irq->p_api->enable(p_irq->p_ctrl);

         if (FSP_SUCCESS != err)
         {
             p_context->initialized = false;

             rs_i2c_if_sensor_init_cleanup(p_context, p_context->p_ctrl, p_irq, sensor_opened, irq_opened);

             return err;
         }
     }

     /*
      * Fast repeated-read path.
      */

 #if BSP_CFG_RTOS == 2

     if (pdTRUE != xSemaphoreTake( p_context->irq_semaphore, rs_i2c_if_timeout_to_ticks(timeout_ms)))
     {
         return FSP_ERR_TIMEOUT;
     }

 #else

     {
         uint32_t elapsed_ms = 0U;

         while (false == p_context->irq_flag)
         {
             if (elapsed_ms >= timeout_ms)
             {
                 return FSP_ERR_TIMEOUT;
             }

             R_BSP_SoftwareDelay(1U, BSP_DELAY_UNITS_MILLISECONDS);

             elapsed_ms++;
         }

         p_context->irq_flag = false;
     }

 #endif

     /*
      * Read raw sensor data into the persistent context buffer.
      */
     err = p_context->p_api->accelRead(p_context->p_ctrl, &p_context->raw_data);
     if (FSP_SUCCESS != err)
     {
         return err;
     }

     /*
      * Convert the raw sample directly into the caller's output object.
      */
     return p_context->p_api->accelDataCalculate( p_context->p_ctrl, &p_context->raw_data, (rm_icm42670_accel_data_t *) p_sensor_data);
 }

/*******************************************************************************************************************//**
 * @brief Handles the external interrupt generated by the sensor.
 *
 * The callback retrieves the sensor read context from the external IRQ callback arguments and signals that a sensor
 * interrupt has occurred. When FreeRTOS is enabled, the callback gives the IRQ semaphore from ISR context and requests
 * a context switch if a higher-priority task is unblocked. In a bare-metal configuration, it sets the interrupt flag
 * in the read context.
 *
 * @param[in] p_args  Pointer to the external IRQ callback arguments containing the sensor read context.
 *
 * @note The callback returns without taking action if @p p_args is NULL, the context is NULL, or the context has not
 *       been initialized.
 *
 * @note When FreeRTOS is enabled, the semaphore is given only if the IRQ semaphore stored in the context is valid.
 **********************************************************************************************************************/
void rs_i2c_if_SensorExternalIrqCallback(external_irq_callback_args_t * p_args)
{
    rs_i2c_if_sensor_read_context_t * p_context;
    if (NULL == p_args)
    {
        return;
    }
    p_context = (rs_i2c_if_sensor_read_context_t *) p_args->p_context;
    if ((NULL == p_context) || (false == p_context->initialized))
    {
        return;
    }
#if BSP_CFG_RTOS == 2
    if (NULL != p_context->irq_semaphore)
    {
        BaseType_t higher_priority_task_woken = pdFALSE;
        (void) xSemaphoreGiveFromISR(p_context->irq_semaphore, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
#else
    p_context->irq_flag = true;
#endif
}

