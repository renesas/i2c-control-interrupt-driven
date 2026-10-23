/***********************************************************************************************************************
 * File Name    : demo_entry.c
 * Description  : Contains motion sensor demo.
 **********************************************************************************************************************/
/***********************************************************************************************************************
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
***********************************************************************************************************************/
#include <i2c_thread.h>
#include <rs_i2c_if/sensor_ctrl/sensor_ctrl.h>
#include <rs_i2c_if/rs_i2c_if.h>
#include "rm_icm42670/rm_icm42670_hal_data.h"
#include "rm_icm42670/rm_icm42670.h"
#include "rm_icm42670/rm_icm42670_api.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SENSOR_READ_TIMEOUT_MS    (100U)

#define BANNER      "\r\n*****************************************************"\
                    "\r\n   RS I2C IF - Motion Sensor Demo"\
                    "\r\n   Module Version "RS_I2C_IF_VERSION_STRING""\
                    "\r\n*****************************************************"\
                    "\r\n"
/**
 * @brief Print accelerometer data in compact format
 *
 * Prints accelerometer data on a single line with minimal formatting.
 *
 * @param[in] p_accel  Pointer to accelerometer data structure
 *
 * @example
 *     rm_icm42670_accel_data_t accel_data;
 *     rs_i2c_if_sensor_data_print(&sensor data);
 *     Example output: Accel [X=-0.095, Y=-2.208, Z=9.979] m/s^2
 */
static fsp_err_t rs_i2c_if_sensor_data_print(rm_icm42670_accel_data_t const * p_accel)
{
    int result;

    if (NULL == p_accel)
    {
        return FSP_ERR_INVALID_POINTER;
    }

    result = printf("Accel [X=%.3f, Y=%.3f, Z=%.3f] m/s^2\r\n",
                    (double) p_accel->accel_x,
                    (double) p_accel->accel_y,
                    (double) p_accel->accel_z);

    if (result < 0)
    {
        return FSP_ERR_INTERNAL;
    }

    return FSP_SUCCESS;
}

/* Controls/configurations needed for sensor control */
extern rs_i2c_if_instance_t const g_rs_i2c_if_device;
extern external_irq_instance_t const g_external_irq;

/* Sensor control structure */
rs_i2c_if_sensor_ctrl_t g_sensor_sram = {0};

/* Thread entry function */
/* pvParameters contains TaskHandle_t */
void demo_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED (pvParameters);
    fsp_err_t err = FSP_SUCCESS;

    /* Generate configuration for sensor in SRAM */
    err = rs_i2c_if_SensorConfigGen(&g_rs_i2c_if_device, &g_icm42670_sensor0, &g_external_irq, &g_sensor_sram);
    if(err != FSP_SUCCESS)
    {
        __BKPT(0);
    }

    /* Verify the configuration */
    err = rs_i2c_if_SensorConfigCheck(&g_rs_i2c_if_device, &g_icm42670_sensor0, &g_external_irq, &g_sensor_sram);
    if(err != FSP_SUCCESS)
    {
        __BKPT(1);
    }


    /* Print module info */
    printf(BANNER);
    fflush(stdout);

    while (1)
    {

        /*  Read sensor data */
        err = rs_i2c_if_SensorDataRead(&g_sensor_sram.sensor_context,
                                       &g_sensor_sram.sensor_instance,
                                       &g_icm42670_interrupt_cfg,
                                       &g_sensor_sram.irq_instance,
                                       &g_sensor_sram.sensor_data,
                                        SENSOR_READ_TIMEOUT_MS);

        /* Print sensor data on terminal and handle errors */
        if (FSP_SUCCESS == err)
        {
            err = rs_i2c_if_sensor_data_print(&g_sensor_sram.sensor_data);
            if(err != FSP_SUCCESS)
            {
                __BKPT(0);
            }
        }
        else if (FSP_ERR_TIMEOUT == err)
        {
            printf("Sensor Data-Ready Timeout\r\n");
        }
        else
        {
            printf("Sensor Read Error: %d\r\n", (int) err);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
