/* generated HAL header file - do not edit */
#ifndef HAL_DATA_H_
#define HAL_DATA_H_
#include <stdint.h>
#include "bsp_api.h"
#include "common_data.h"
#include "r_sci_uart.h"
            #include "r_uart_api.h"
#include "r_iic_master.h"
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
extern iic_master_instance_ctrl_t g_i2c_ctrl;
extern const i2c_master_cfg_t g_i2c_cfg;

#ifndef rs_i2c_if_callback
void rs_i2c_if_callback(i2c_master_callback_args_t * p_args);
#endif
void hal_entry(void);
void g_hal_init(void);
FSP_FOOTER
#endif /* HAL_DATA_H_ */
