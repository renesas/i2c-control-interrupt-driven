/* generated thread header file - do not edit */
#ifndef I2C_THREAD_H_
#define I2C_THREAD_H_
#include "bsp_api.h"
                #include "FreeRTOS.h"
                #include "task.h"
                #include "semphr.h"
                #include "hal_data.h"
                #ifdef __cplusplus
                extern "C" void i2c_thread_entry(void * pvParameters);
                #else
                extern void i2c_thread_entry(void * pvParameters);
                #endif
#include "r_sci_uart.h"
            #include "r_uart_api.h"
#include "r_iic_b_master.h"
#include "r_i2c_master_api.h"
FSP_HEADER
/** UART on SCI Instance. */
            extern const uart_instance_t      g_uart_jlob_vcom;

            /** Access the UART instance using these structures when calling API functions directly (::p_api is not used). */
            extern sci_uart_instance_ctrl_t     g_uart_jlob_vcom_ctrl;
            extern const uart_cfg_t g_uart_jlob_vcom_cfg;
            extern const sci_uart_extended_cfg_t g_uart_jlob_vcom_cfg_extend;

            #ifndef uart_jlob_vcom_callback
            void uart_jlob_vcom_callback(uart_callback_args_t * p_args);
            #endif
/* I2C Master on IIC Instance. */
extern const i2c_master_instance_t g_i2c;

/** Access the I2C Master instance using these structures when calling API functions directly (::p_api is not used). */
extern iic_b_master_instance_ctrl_t g_i2c_ctrl;
extern const i2c_master_cfg_t g_i2c_cfg;

#ifndef rs_i2c_if_callback
void rs_i2c_if_callback(i2c_master_callback_args_t * p_args);
#endif
FSP_FOOTER
#endif /* I2C_THREAD_H_ */
