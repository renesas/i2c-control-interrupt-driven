#include "i2c_thread.h"
/* I2C Thread entry function */
/* pvParameters contains TaskHandle_t */
void i2c_thread_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED (pvParameters);
    /* Call the demo  entry */
    extern void demo_entry(void);
    demo_entry();
}
