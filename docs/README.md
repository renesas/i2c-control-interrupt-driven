# I2C Controller with Interrupt-Driven Control

## Table of Contents
* [1. Introduction](#1-introduction)
* [2. Hardware and Tool Support](#2-hardware-and-tool-support)
* [3. MCU Resource Use](#3-mcu-resource-use)
* [4. Hardware Block Diagram](#4-hardware-block-diagram) 
* [5. System Block Diagram](#5-system-block-diagram)
* [6. rs_i2c_if Module and Sensor Driver Integration](#6-rs_i2c_if-module-and-sensor-driver-integration)
* [7. Module-Layer Code and Demonstration Code](#7-module-layer-code-and-demonstration-code)
* [8. Reference Application](#8-reference-application)
* [9. Configuration Assumptions for the Reference Board](#9-configuration-assumptions-for-the-reference-board)
* [10. Public Function - Where to Start](#10-public-functions--where-to-start)
* [11. Running the Project and Expected Output](#11-running-the-project-and-expected-output)

## 1. Introduction

This project demonstrates an interrupt-driven I2C interface between a Renesas RA MCU and the QCIOT-ICM42671POCZ motion sensor module. Upon detecting motion, the sensor asserts an interrupt on the MCU's IRQ line. The MCU interrupt handler then triggers an I2C transaction to read sensor data, providing a responsive and efficient event-driven sensing solution.

### Project Structure

```
.
├── index.htm / docs/README.md
│   └── Documentation for the I2C controller module with interrupt control   
│        
├── Examples
│   └── VSCode
│       │   
│       │── I2C_Motion_Sensor_Control_FPB_RA6E2
│       │    └── Interrupt-driven I2C motion sensor data acquisition for FPB-RA6E2
│       │── I2C_Motion_Sensor_Control_FPB_RA4E2
│       │    └── Interrupt-driven I2C motion sensor data acquisition for FPB-RA4E2
│       │── I2C_Motion_Sensor_Control_FPB_RA2E3
│       │    └── Interrupt-driven I2C motion sensor data acquisition for FPB-RA2E3
│       │
│       └── executable
│            └── Executables for the demo projects
├── src
│   │── demo
│   │    └── Demonstration projects
│   │ 
│   │── rm_icm42670
│   │    └── Driver for QCIOT-ICM42671POCZ motion sensor
│   │ 
│   │── config   
│   │    └── Module configuration settings
│   │       
│   │── sensor_ctrl
│   │    └── Sensor control functions - Single entry function
│   │ 
│   │── rs_i2c_if.c
│   │        └── I2C controller module main source code
│   │── rs_i2c_if_ra.c
│   │        └── I2C controller module private function
│   └── rs_i2c_if.h
│           └── I2C controller module header file
│   
```

## 2. Hardware and Tool Support

| Hardware Item | Description|
|---|---|
| Reference Board | FPB-RA6E2, FPB-RA4E2, FPB-RA2E3|
| Device Family | RA6E2, RA4E2, RA2E3|
| External Hardware | QCIOT-ICM42671POCZ|

| IDE Tool/RTOS/FSP| Description|
|---|---|
| IDE / Tool| VS Code/Renesas extensions for VS Code |
| FSP| v6.5.0 |
| RTOS | FreeRTOS, Bare Metal|

## 3. MCU Resource Use

| Item | Description |
|---|---|
| I2C Controller Driver | R_IIC_B_MASTER, R_IIC_MASTER|
| External Interrupt | R_ICU |
| Board I/O | R_IOPORT |

## 4. Hardware Block Diagram
```
            +--------------------------------+
            |      RA MCU                    |
            | I2C Controller + ICU controller|
            +------------+-----------+-------+
                         |           ^
                   VCC   |  VCC      |
                    |    |   |       |
                  [2.2K] | [2.2K]    |
                     |   |   |       |
                   SDA --+--SCL     IRQ
                         |           |
                         +-----+-----+
                               |                   
                               |                   
                        +------+------+    
                        |Motion Sensor|    
                        |             |           
                        +-------------+    
```
## 5. System Block Diagram

The sofware building block(SBB) provides a generic, reusable I2C sensor-access framework that simplifies application-level interaction with multiple I2C sensors, isolates sensor-specific drivers from hardware-dependent implementations, supports interrupt-driven sensor data acquisition, improves portability and scalability when adding sensors, and reduces application complexity through a single sensor-access interface.

```
+----------------------------------------+        
|       User Application / Demo          |
+-------------------+--------------------+
                    ^
                    |
                    v
+-------------------+--------------------+
|      I2C control instance              |
|   +---------------------------------+  |
|   |Sensor access function:          |  |
|   | - Single Entry Point function   |  |
|   | - Set device configuration      |  |
|   | - Device start/stop             |  |
|   | - Interrupt trigger             |  |
|   | - Read device data              |  |
|   +---------------+-----------------+  |
|                   ^                    |
|                   |                    |
|                   v                    |
|   +---------------+-----------------+  |
|   | I2C generic driver/bus level:   |  |
|   | - I2C device access functions   |  |
|   +---------------+-----------------+  |
+-------------------+--------------------+ 
                    ^             
                    |
                    v
+-------------------+--------------------+
|         I2C HAL Layer/IRQ HAL Layer    |
|            r_iic_master/r_icu          |
+----------------------------------------+
                    ^             
                    | I2C + Interrupt
                    v 
+-------------------+--------------------+
|            QCIOT-ICM42671POCZ          | 
+----------------------------------------+ 
```
## 6. `rs_i2c_if` Module and Sensor Driver Integration

### 6.1. Overview

These modules form a layered sensor-control architecture:

> **`sensor_ctrl` provides the generic sensor interface, a `specific sensor driver` implements sensor-specific behavior, and `rs_i2c_if` provides I2C communication and shared-bus management.**

The application should normally access the sensor through `sensor_ctrl`. A sensor implementation will perform register transfers through `g_rs_i2c_if_device`.

### 6.2. Module Relationship

```text
Application
    |
    v
sensor_ctrl's single entry function
    |
    v
sensor-specific driver
    |
    v
g_rs_i2c_if_device
    +-- p_ctrl --> g_rs_i2c_if_device_ctrl
    +-- p_cfg  --> g_rs_i2c_if_device_cfg
    |               +-- p_lower_level_cfg --> address and FSP callback
    |               +-- p_extend ---------> shared-bus configuration
    +-- p_api  --> g_comms_on_rs_i2c_if
                         |
                         v
                    FSP HAL I2C driver
```

### 6.3. Module Responsibilities

#### 6.3.1. `sensor_ctrl`

`sensor_ctr` is an application-facing sensor abstraction layer. It stores or references the selected sensor driver’s control, configuration, and API objects, allowing application code to initialize, configure, and read the sensor without directly depending on a sensor-specific driver implementation. It also provides a single-entry interface for sensor operations, isolating sensor-specific implementation details from the application. The `sensor_ctrl` dispatches generic sensor operations to the selected sensor-specific driver.

#### `rs_i2c_if_sensor_ctrl_t`

#### Overview

`rs_i2c_if_sensor_ctrl_t` is the SRAM-resident runtime container used by `rs_i2c_if_SensorDataRead()`. It groups the writable I2C, sensor-driver, external-interrupt, read-context, and sensor-data objects required for one sensor instance.

Read-only API tables remain in **Flash** and are referenced by the corresponding instance objects.

```c
typedef struct st_rs_i2c_if_sensor_ctrl
{
    rs_i2c_if_instance_t                   i2c_instance;
    rs_i2c_if_instance_ctrl_t              i2c_ctrl;
    rs_i2c_if_cfg_t                        i2c_cfg;
    i2c_master_cfg_t                       i2c_hal_cfg;
    rs_i2c_if_bus_extended_cfg_t           i2c_bus_cfg;

    sensor_specific_driver_instance_t      sensor_instance;
    sensor_specific_driver_instance_ctrl_t sensor_ctrl;
    sensor_specific_driver_cfg_t           sensor_cfg;

    external_irq_instance_t                irq_instance;
    icu_instance_ctrl_t                    irq_ctrl;
    external_irq_cfg_t                     irq_cfg;

    rs_i2c_if_sensor_read_context_t        sensor_context;
    sensor_specific_driver_data_t          sensor_data;
} rs_i2c_if_sensor_ctrl_t;
```

#### Object Groups

| Group | Members | Purpose |
|---|---|---|
| I2C controller | `i2c_instance`, `i2c_ctrl`, `i2c_cfg`, `i2c_hal_cfg`, `i2c_bus_cfg` | Maintains the I2C interface, runtime control, and configuration |
| Sensor-specific driver | `sensor_instance`, `sensor_ctrl`, `sensor_cfg` | Maintains the sensor-speicific driver instance, runtime state, and sensor configuration |
| External IRQ | `irq_instance`, `irq_ctrl`, `irq_cfg` | Provides the external interrupt instance, control block, and configuration |
| Sensor read | `sensor_context`, `sensor_data` | Maintains the generic read context and destination for acquired sensor data |

#### Memory Organization

- **SRAM:** Runtime control blocks, mutable configurations, read context, and sensor data.
- **Flash:** Read-only I2C, sensor, and external-IRQ API tables.

This organization keeps writable runtime state in SRAM without unnecessarily copying constant API tables from Flash.

#### Architecture Role

```text
Application
  -> Sensor Control
    -> Sensor-Specific Driver
      -> rs_i2c_if
        -> I2C HAL
          -> Sensor
```

`rs_i2c_if_sensor_ctrl_t` consolidates the runtime state required by this path into one object, supporting an application-facing, single-entry sensor interface while keeping sensor-specific and lower-level I2C implementation details below the application layer.

#### 6.3.2. `Sensor-specific driver`

The `Sensor-specific driver` implements a sensor-specific operations, including:

- Device initialization and reset;
- Register configuration;
- Interrupt configuration;
- Acceleration-data acquisition;
- Raw-data conversion; and
- Sensor-specific communication-event handling.

Its configuration should reference `g_rs_i2c_if_device` as the communication interface. It should not directly access the FSP I2C instance, mutex, semaphore, or shared-bus state.

#### 6.3.3. `rs_i2c_if`

`rs_i2c_if` provides the generic transport layer. It owns:

- The FSP I2C driver connection;
- Device addressing;
- Interrupt-driven transfer completion;
- Shared-bus locking;
- RTOS semaphore synchronization;
- Bus timeout handling; and
- Bus reconfiguration when multiple devices share one peripheral.

#### FreeRTOS synchronization objects

```c
#if BSP_CFG_RTOS == 2
SemaphoreHandle_t g_rs_i2c_if_bus_recursive_mutex_handle;
StaticSemaphore_t g_rs_i2c_if_bus_recursive_mutex_memory;
SemaphoreHandle_t g_rs_i2c_if_bus_blocking_semaphore_handle;
StaticSemaphore_t g_rs_i2c_if_bus_blocking_semaphore_memory;

rs_i2c_if_mutex_t g_rs_i2c_if_bus_recursive_mutex =
{
    .p_mutex_handle = &g_rs_i2c_if_bus_recursive_mutex_handle,
    .p_mutex_memory = &g_rs_i2c_if_bus_recursive_mutex_memory,
};

rs_i2c_if_semaphore_t g_rs_i2c_if_bus_blocking_semaphore =
{
    .p_semaphore_handle = &g_rs_i2c_if_bus_blocking_semaphore_handle,
    .p_semaphore_memory = &g_rs_i2c_if_bus_blocking_semaphore_memory,
};
#endif
```

`rs_i2c_if_ctrl_Open()` creates these static objects once. The recursive mutex serializes access to the shared bus, while the semaphore synchronizes a waiting task with the I2C completion callback. Neither `sensor_ctrl` nor `sensor-specific driver` accesses these objects directly.

#### Shared-bus configuration

```c
rs_i2c_if_bus_extended_cfg_t g_rs_i2c_if_bus_extended_cfg =
{
#if RS_I2C_IF_CFG_PRESET_USE
    .p_driver_instance       = (void *) &g_i2c_preset,
#else
    .p_driver_instance       = (void *) &g_i2c,
#endif
    .p_current_ctrl          = NULL,
    .bus_timeout             = RS_I2C_IF_BUS_TIMEOUT_TICKS,
#if BSP_CFG_RTOS == 2
    .p_blocking_semaphore    = &g_rs_i2c_if_bus_blocking_semaphore,
    .p_bus_recursive_mutex   = &g_rs_i2c_if_bus_recursive_mutex,
#endif
    .p_elc                   = NULL,
    .p_timer                 = NULL,
};
```

This structure describes resources shared by all logical devices on the physical I2C bus. Device configurations reference it through `p_extend`.

#### Device runtime control

```c
rs_i2c_if_instance_ctrl_t g_rs_i2c_if_device_ctrl;
```

This object stores mutable runtime state for the sensor communication instance. It is managed by `rs_i2c_if` and is not modified directly by `sensor_ctrl` or `sensor-specific driver`.

#### Lower-level device configuration

```c
const i2c_master_cfg_t g_rs_i2c_if_device_lower_level_cfg =
{
    .slave      = SENSOR_ADDRESS,
    .addr_mode  = I2C_MASTER_ADDR_MODE_7BIT,
    .p_callback = rs_i2c_if_callback,
};
```

This FSP-level configuration defines the sensor I2C address and routes lower-level transfer events to `rs_i2c_if_callback()`.

### Device configuration

```c
const rs_i2c_if_cfg_t g_rs_i2c_if_device_cfg =
{
    .semaphore_timeout = 0xFFFFFFFF,
    .p_lower_level_cfg = (void *) &g_rs_i2c_if_device_lower_level_cfg,
    .p_extend          = (void *) &g_rs_i2c_if_bus_extended_cfg,
    .p_callback        = sensor_driver_callback,
    .p_context         = NULL,
};
```

This object connects the sensor-specific I2C settings to the shared bus. It also defines the blocking timeout and the callback used by the sensor driver. Use `.p_context = NULL` when no callback context is required.

#### Complete communication instance

```c
const rs_i2c_if_instance_t g_rs_i2c_if_device =
{
    .p_ctrl = &g_rs_i2c_if_device_ctrl,
    .p_cfg  = &g_rs_i2c_if_device_cfg,
    .p_api  = &g_comms_on_rs_i2c_if,
};
```

This is the transport instance consumed by the `sensor-specific driver`. A sensor-driver configuration can reference it as follows:

```c

.p_comms_instance = &g_rs_i2c_if_device,

```
The `sensor_ctrl` reaches this instance indirectly through the `sensor-specific driver`.

### 6.4. Callback Roles

- `rs_i2c_if_callback()` handles lower-level FSP I2C events, records transfer status, and signals the completion semaphore.
- `sensor_driver_callback()` receives communication events at the sensor-specific driver layer.
- A `sensor_ctrl` callback, when supported, reports completed sensor operations or sensor events to the application without exposing the I2C callback details.

```text
FSP I2C ISR
     |
     v
rs_i2c_if_callback()
     |
     v
sensor_specific_driver_callback()
     |
     v
sensor_ctrl callback (when supported)
     |
     v
Application
```

### 6.5. Ownership Summary

| Object or layer | Owner and purpose |
|---|---|
| `sensor_ctrl` instance | Generic application-facing sensor interface. |
| `sensor-specific driver` control/configuration | sensor runtime state and sensor-specific settings. |
| `g_rs_i2c_if_bus_recursive_mutex` | `rs_i2c_if`; protects shared-bus access. |
| `g_rs_i2c_if_bus_blocking_semaphore` | `rs_i2c_if`; signals transfer completion. |
| `g_rs_i2c_if_bus_extended_cfg` | `rs_i2c_if`; describes shared physical-bus resources. |
| `g_rs_i2c_if_device_ctrl` | `rs_i2c_if`; stores mutable communication state. |
| `g_rs_i2c_if_device_lower_level_cfg` | FSP boundary; defines device address and callback. |
| `g_rs_i2c_if_device_cfg` | `rs_i2c_if`; connects device and bus configurations. |
| `g_rs_i2c_if_device` | Transport interface exposed to `sensor-specific driver`. |

### 6.6. Runtime Flow

1. The application calls a generic `sensor_ctrl` operation.
2. `sensor_ctrl` dispatches the request to the `sensor-specific driver` implementation.
3. `sensor-specific driver` prepares the required sensor-register transaction.
4. The driver calls the API exposed by `g_rs_i2c_if_device`.
5. `rs_i2c_if` locks the shared bus and applies the device configuration.
6. The FSP I2C driver starts the transfer.
7. `rs_i2c_if_callback()` records completion and signals the semaphore.
8. `rs_i2c_if` returns the transfer result and releases the bus mutex.
9. `sensor-specific driver` interprets the sensor data and returns it through `sensor_ctrl`.
10. The application receives sensor data through the generic interface.

This separation allows another sensor driver to replace `sensor-specific driver` while preserving the application-facing `sensor_ctrl` interface and reusing the same `rs_i2c_if` transport layer.

## 7. `module-layer` Code and Demonstration Code

The following table identifies which files are reusable `module-layer` code and which belong only to the reference or demo application layer.

| Layer | Files | Purpose | Reuse Expectation |
|---|---|---|---|
| `module-layer` | `src/rs_i2c_if/rs_i2c_if.c` | Lifecycle state machine; all public function entry points|
| `module-layer` | `src/rs_i2c_if/rs_i2c_if_ra.c` | private functions | Include directly; do not modify |
| `module-layer` | `src/rs_i2c_if/rs_i2c_if.h` | Public interface header — all public function prototypes | Include directly; do not modify |
| `module-layer` | `src/rs_i2c_if/sensor_ctrl/sensor_ctrl.h` |Standard module configurations | Include directly; modify as needed to support the lower-level driver|
| `module-layer` | `src/rs_i2c_if/sensor_ctrl/sensor_ctrl.c` | Public/private functions for sensor-specific driver support|Modify to support a specific sensor driver|
| `module-layer` | `src/rs_i2c_if/config/rs_i2c_if_cfg_data.h` |Public interface header for module configurations| Include directly; do not modify |
| `module-layer` | `src/rs_i2c_if/config/rs_i2c_if_cfg_data.c` |Standard configurations for the module|Include directly; modify to adapt to lower-level driver when needed|
| `Preset Config` | `src/rs_i2c_if/config/rs_i2c_if_cfg_data_fpb_ra4e2_fpb_ra6e2.c/.h` | Pre-built HAL instance definitions for FPB-RA4E2/FPB-RA6E2; aliased to FSP-generated instances where appropriate | Reference only — do not edit; for custom boards, use the FSP Stack Configurator instead
| `Preset Config` | `src/rs_i2c_if/config/rs_i2c_if_cfg_data_fpb_ra2e3.c/.h` | Pre-built HAL instance definitions for FPB-RA2E3; aliased to FSP-generated instances where appropriate | Reference only — do not edit; for custom boards, use the FSP Stack Configurator instead
| `Reference App` | `src/demo/demo_entry.c` | Full demonstration; CLI input/output|

Note: The CLI code for printing and receiving user input via the serial terminal is entirely reference application code and is not part of the core module. When adapting this reference code, CLI input handling stays in your application layer.

## 8. Reference Application

These files demonstrate the module but are not intended for direct reuse. They contain board-specific and demo-specific scaffolding. Use them as a starting point and replace what you don't need.

| File | Adaptation Guidance |
|---|---|
| `src/rs_i2c_if/sensor_ctrl/sensor_ctrl.c/.h` |sensor-specific control functions; replace or heavily adapt|
| `src/demo/demo_entry.c` | Starting point for your CLI commands and ISR callback; replace or heavily adapt|
| `src/i2c_thread_entry.c/; hal_entry.c` | FSP application entry point |

### 8.1. FSP-Generated Files — Do Not Edit Manually

| Directory | Notes |
|---|---|
| `ra/` | FSP-generated drivers, middleware, CMSIS, and board support; regenerate using the FSP tooling when configuration.xml changes. |
| `ra_gen/` | Auto-generated by the FSP Stack Configurator; overwritten when regenerated. |

### 8.2. Adopting a Newer Release

This module is delivered as reference source code. Once you incorporate and modify it, you are responsible for maintaining your modified copy and merging future upstream changes.

**What typically changes between releases:**
- Core source files (`src/rs_i2c_if/rs_i2c_if.c`, `src/rs_i2c_if/rs_i2c_if_ra.c`, `src/rs_i2c_if/sensor_ctrl/sensor_ctrl.c`),  may receive bug fixes, new features, or FSP version compatibility updates.
- Public headers (`src/rs_i2c_if/rs_i2c_if.h`, `src/rs_i2c_if/sensor_ctrl/sensor_ctrl.h`) may add new public functions or extend existing structures. Additions are generally backward-compatible; removals or signature changes are not.
- `src/rs_i2c_if/rs_i2c_if.h` constants may gain new `#ifndef`-guarded defaults. Your existing overrides continue to take precedence without modification.
- Preset config files may be updated for new FSP releases or corrected for hardware errata.

**Recommended merge process:**

1. Diff the updated core files against your local copies.
2. Apply upstream changes selectively, preserving any application-specific modifications.
3. Review the updated `src/rs_i2c_if/rs_i2c_if.h`, `src/rs_i2c_if/sensor_ctrl/sensor_ctrl.h` for interface changes; update call sites in your application accordingly.
4. Rebuild and retest on target hardware.

**Note:** Releases do not need to track every minor FSP release automatically. Confirm the supported FSP version in the release notes before upgrading. If the FSP version changes between your baseline and the update, rerun the FSP Stack Configurator to regenerate `ra_gen` content for the new FSP version.

## 9. Configuration Assumptions for the Reference Board

The reference implementation targets the **FPB-RA6E2**, **FPB-RA4E2** and **FPB-RA2E3** with the following board-specific assumptions. These are reference assumptions for the demo hardware, not requirements for your application.

| Resource | Reference Assumption | Notes |
|---|---|---|
| MCU | RA6E2 (Cortex-M33, 64-pin LQFN, R7FA6E2BB3CFM), RA4E2 (Cortex-M33, 64-pin LQFP, R7FA4E2B93CFM), RA2E3 (Cortex-M23, 48-pin, LFQFP, R7FA2E3073CFL) | Primary development targets |
| I2C | 1 channel | Control the motion sensor|
| IRQ | 1 channel | Motion sensor trigger interrupt|
| UART channel | FPB-RA6E2/FPBRA4E2: SCI0; FPB-RA2E3: SCI9; (USB-Serial VCOM bridge via J-Link) | Schematic page 7; FPB-RA6E2/FPB-RA4E2: P410(RX)/P411(TX); FPB-RA2E3: P110(RX)/P109(TX); |
| UART baud rate | 115200 baud, async, 8N1 | VCOM USB-Serial bridge default |

### Required Non-Default FSP Settings

The following FSP Stack Configurator settings must be configured correctly for the module to operate. These are values that differ from FSP defaults and must be set in `configuration.xml` when not using the preset configuration.
| Driver | Property | Required Value | Reason |
|---|---|---|---|
| `r_iic_b_master/I2C ` | Channel| 0 | Select I2C channel|
| `r_iic_b_master/I2C` | Rate | Fast-mode Plus/Fast-mode | Required for I2C mode setup|
| `r_iic_b_master/I2C` | Slave Address | 0x68 | Required for I2c address setup|
| `r_icu/IRQ ` | Channel| 0;7 | Select IRQ channel|
| `r_icu/IRQ` | Trigger| Rising | Required for trigger polarity|
| `r_icu/IRQ` | Callback | external_irq_callback | Required for IRQ callback function setup|
| `r_sci_uart` | Name | g_uart_jlob_vcom | Module name ; must not be changed |
| `r_sci_uart` | Channel | 0;9 | Select UART channel, channel 0/9 connected to J-Link Virtual COM port on the reference board|
| `r_sci_uart` | Callback | uart_jlob_vcom_callback | Module-owned callback; must not be changed | 
| `r_sci_uart` | Operating mode | Asynchronous | Required for non-blocking UART write |
| `r_sci_uart` | Baud rate | 115200 | Set baud rate |

## 10. Public Functions — Where to Start

All public functions use the naming pattern `rs_i2c_if_OperationName()` — a lowercase `rs_i2c_if_` prefix followed by an UpperCamelCase operation name. 

All functions return `fsp_err_t` and take a pointer to the control block as their first argument.

Build-time configuration macros and enumeration values use the all-uppercase `RS_I2C_IF_` form.

### 10.1. Single Entry Function

### Overview

The `rs_i2c_if_SensorDataRead()` provides an interrupt-driven method for acquiring data from a sensor instance. It manages one-time initialization of the I2C communication path, `sensor-specific driver`, external IRQ, sensor data-ready interrupt, and measurement operation, then waits for each data-ready event before reading and converting the sensor sample.

The function supports both **FreeRTOS** and **bare-metal** operation. Under FreeRTOS, a binary semaphore is used to synchronize the data-ready interrupt with the calling task. In a bare-metal build, the function waits on an interrupt flag with a millisecond timeout.

### Function Prototype

```c
fsp_err_t rs_i2c_if_SensorDataRead(
    rs_i2c_if_sensor_read_context_t * const  p_context,
    void const * const                       p_sensor_instance,
    void const * const                       p_interrupt_cfg,
    external_irq_instance_t const * const    p_irq,
    void * const                             p_sensor_data,
    uint32_t                                 timeout_ms);    
```
### Parameters

| Parameter | Direction | Description |
|---|---|---|
| `p_context` | In/Out | Persistent read context used to retain initialization state and interrupt synchronization information across calls|
| `p_instance` | In | Sensor instance containing the sensor control block, configuration, and functions table|
| `interrupt_cfg` | In | Sensor interrupt configuration. The function enables bit 0 of `int_config` locally for the required data-ready interrupt|
| `p_irq` | In | External IRQ instance associated with the sensor data-ready signal|
| `p_accel_data` | Out | Destination structure that receives the converted sensor data|
| `timeout_ms` | In | Maximum time, in milliseconds, to wait for a sensor data-ready interrupt|

### Functional Behavior

#### First Call

When `p_context->initialized` is `false`, the function performs the initialization sequence required for interrupt-driven acquisition:

1. Validates the sensor instance, IRQ instance, communication configuration, and required API function pointers.
2. Associates the persistent context with the sensor control block, sensor configuration, and IRQ instance.
3. Creates the static FreeRTOS binary semaphore when `BSP_CFG_RTOS == 2`; otherwise, initializes the bare-metal interrupt flag.
4. Opens the lower-level I2C driver. `FSP_ERR_ALREADY_OPEN` is accepted because the I2C bus may already be shared and initialized.
5. Opens the sensor driver and external IRQ instance.
6. Registers `rs_i2c_if_SensorExternalIrqCallback()` with the persistent context as the callback context.
7. Enables the required sensor data-ready interrupt bit in the local interrupt configuration.
8. Clears any stale data-ready event before starting measurement.
9. Starts sensor measurement and enables the external IRQ.
10. Marks the context as initialized for subsequent reads.

If initialization fails after resources have been opened, the function uses the module cleanup path before returning the originating error.

#### Subsequent Calls

After initialization, the function reuses the existing sensor and IRQ state instead of reopening the drivers. It verifies that the same context is still associated with the original sensor control block, sensor configuration, and IRQ instance. Reusing the context with different objects returns `FSP_ERR_INVALID_ARGUMENT`.

#### Waiting for Data Ready

The waiting mechanism depends on the build configuration:

- **FreeRTOS:** waits on `irq_semaphore` using the requested `timeout_ms` converted to RTOS ticks.
- **Bare metal:** polls `irq_flag` at 1 ms intervals until the interrupt occurs or `timeout_ms` expires.

A wait that exceeds the specified timeout returns `FSP_ERR_TIMEOUT`.

#### Reading and Converting Data

After receiving the data-ready event, the function:

1. Calls the sensor read function to obtain the sensor data.
2. Converts and stores the sensor data.

Errors from either sensor operation are returned directly to the caller.

### Return Values

| Return value | Meaning |
|---|---|
| `FSP_SUCCESS` | Accelerometer data was read and converted successfully|
| `FSP_ERR_ASSERTION` | A required pointer, nested configuration object, or required API function pointer is `NULL`|
| `FSP_ERR_INVALID_ARGUMENT` | An initialized context was supplied with a different sensor or IRQ configuration|
| `FSP_ERR_OUT_OF_MEMORY` | The FreeRTOS data-ready semaphore could not be created|
| `FSP_ERR_NOT_OPEN` | The expected FreeRTOS data-ready semaphore is unavailable|
| `FSP_ERR_TIMEOUT` | No data-ready interrupt was received before the specified timeout|
| Other `fsp_err_t` values | Errors returned by the lower-level I2C driver, sensor driver, external IRQ, data read, or data conversion functions|

### Usage Considerations

- The object referenced by `p_context` is persistent state and must remain valid across calls.
- Once initialized, a context must not be reused with a different sensor control block, sensor configuration, or IRQ instance.
- The sensor instance is treated as the source of the sensor control block, configuration, and API table.
- The lower-level I2C bus may be shared. Therefore, `FSP_ERR_ALREADY_OPEN` from its `open()` API is treated as an acceptable initialization result.
- The function is blocking while waiting for data-ready. The maximum blocking period is controlled by `timeout_ms`.
- The external IRQ is enabled only after the sensor configuration and measurement startup sequence has completed.

### Typical Usage

```c
fsp_err_t err;
sensor_data_t sensor_data = {0};

err = rs_i2c_if_SensorDataRead (&g_sensor_sram.sensor_context,
                                &g_sensor_sram.sensor_instance,
                                &g_sensor_interrupt_cfg,
                                &g_sensor_sram.irq_instance,
                                &g_sensor_sram.sensor_data,
                                 SENSOR_READ_TIMEOUT_MS);                               
```
### 10.2. Public Function Summary

| Function | Purpose |
|---|---|
| `rs_i2c_if_ctrl_Read` | Reads one or more bytes from the configured I2C device. |
| `rs_i2c_if_ctrl_Write` | Writes one or more bytes to the configured I2C device. |
| `rs_i2c_if_ctrl_WriteRead` | Performs a combined write followed by a read operation. |
| `rs_i2c_if_ctrl_Open` | Validates the configuration, initializes the instance and RTOS resources when applicable, and verifies the lower-level bus. |
| `rs_i2c_if_ctrl_Close` | Closes the instance, releases its shared-bus selection, and clears runtime state. |

### Common Preconditions

- `p_api_ctrl` must point to a valid `rs_i2c_if_instance_ctrl_t` control block.
- `rs_i2c_if_ctrl_Open()` must complete successfully before calling `Read`, `Write`, `WriteRead`, or `Close`.
- Transfer buffers must remain valid for the duration of the blocking transaction.
- A transfer size of zero is rejected with `FSP_ERR_INVALID_SIZE`.

### Error Propagation

In addition to the explicitly documented validation errors, the transaction functions return the status produced by `rs_i2c_if_bus_read()`, `rs_i2c_if_bus_write()`, or `rs_i2c_if_bus_write_read()`. `rs_i2c_if_ctrl_Open()` also propagates failures from RTOS-object initialization and lower-level bus-status checking.

### 10.3. `rs_i2c_if_ctrl_Read`

**Implements:** `rs_i2c_if_api_t::read`

Reads the requested number of bytes from the configured I2C device into the caller-provided destination buffer.

### Prototype
```c
fsp_err_t rs_i2c_if_ctrl_Read(
    rs_i2c_if_ctrl_t * const p_api_ctrl,
    uint8_t * const           p_dest,
    uint32_t const            bytes);
```
### Parameters

| Parameter | Description |
|---|---|
| `p_api_ctrl` | Pointer to the `rs_i2c_if` control block. |
| `p_dest` | Destination buffer that receives the data. |
| `bytes` | Number of bytes to read. Must be greater than zero. |

### Operation

1. Validates the control pointer, destination pointer, and transfer size.
2. Casts the public control pointer to `rs_i2c_if_instance_ctrl_t`.
3. Verifies that the instance open marker equals `RS_I2C_IF_OPEN`.
4. Calls `rs_i2c_if_bus_read()` and returns its status.

### Return Values

| Return value | Condition |
|---|---|
| `FSP_SUCCESS` | The read completed successfully. |
| `FSP_ERR_ASSERTION` | `p_api_ctrl` or `p_dest` is `NULL`. |
| `FSP_ERR_INVALID_SIZE` | `bytes` is zero. |
| `FSP_ERR_NOT_OPEN` | The module is not open. |
| Other `fsp_err_t` | An error returned by the lower-level bus-read helper. |

### 10.4. `rs_i2c_if_ctrl_Write`

**Implements:** `rs_i2c_if_api_t::write`

Writes the requested number of bytes from the caller-provided source buffer to the configured I2C device.

### Prototype

```c
fsp_err_t rs_i2c_if_ctrl_Write(
    rs_i2c_if_ctrl_t * const p_api_ctrl,
    uint8_t * const           p_src,
    uint32_t const            bytes);
```
### Parameters

| Parameter | Description |
|---|---|
| `p_api_ctrl` | Pointer to the `rs_i2c_if` control block. |
| `p_src` | Source buffer containing the bytes to transmit. |
| `bytes` | Number of bytes to write. Must be greater than zero. |

### Operation

1. Validates the control pointer, source pointer, and transfer size.
2. Casts the public control pointer to `rs_i2c_if_instance_ctrl_t`.
3. Verifies that the module is open.
4. Calls `rs_i2c_if_bus_write()` and returns its status.

### Return Values

| Return value | Condition |
|---|---|
| `FSP_SUCCESS` | The write completed successfully. |
| `FSP_ERR_ASSERTION` | `p_api_ctrl` or `p_src` is `NULL`. |
| `FSP_ERR_INVALID_SIZE` | `bytes` is zero. |
| `FSP_ERR_NOT_OPEN` | The module is not open. |
| Other `fsp_err_t` | An error returned by the lower-level bus-write helper. |

### 10.5. `rs_i2c_if_ctrl_WriteRead`

**Implements:** `rs_i2c_if_api_t::writeRead`

Performs a combined I2C transaction that writes a command or register address and then reads response data.

### Prototype
```c
fsp_err_t rs_i2c_if_ctrl_WriteRead(
    rs_i2c_if_ctrl_t * const            p_api_ctrl,
    rs_i2c_if_write_read_params_t const write_read_params);
```
### Parameters

| Parameter | Description |
|---|---|
| `p_api_ctrl` | Pointer to the `rs_i2c_if` control block. |
| `write_read_params.p_src` | Source buffer for the write phase. |
| `write_read_params.src_bytes` | Number of bytes in the write phase. Must be greater than zero. |
| `write_read_params.p_dest` | Destination buffer for the read phase. |
| `write_read_params.dest_bytes` | Number of bytes in the read phase. Must be greater than zero. |

### Operation

1. Validates the control pointer, both transfer buffers, and both transfer sizes.
2. Casts the public control pointer to `rs_i2c_if_instance_ctrl_t`.
3. Verifies that the module is open.
4. Passes the parameter structure to `rs_i2c_if_bus_write_read()`.
5. Returns the lower-level helper status without translation.

### Return Values

| Return value | Condition |
|---|---|
| `FSP_SUCCESS` | The combined transaction completed successfully. |
| `FSP_ERR_ASSERTION` | `p_api_ctrl`, `p_src`, or `p_dest` is `NULL`. |
| `FSP_ERR_INVALID_SIZE` | `src_bytes` or `dest_bytes` is zero. |
| `FSP_ERR_NOT_OPEN` | The module is not open. |
| Other `fsp_err_t` | An error returned by the lower-level write-read helper. |

### 10.6. `rs_i2c_if_ctrl_Open`

**Implements:** `rs_i2c_if_api_t::open`

Opens and configures an `rs_i2c_if` instance, associates it with the shared I2C bus, initializes optional RTOS resources, and verifies that the lower-level driver is available.

### Prototype

```c
fsp_err_t rs_i2c_if_ctrl_Open(
    rs_i2c_if_ctrl_t * const      p_api_ctrl,
    rs_i2c_if_cfg_t const * const p_cfg);
```
### Parameters

| Parameter | Description |
|---|---|
| `p_api_ctrl` | Pointer to the instance control block to initialize. |
| `p_cfg` | Pointer to the immutable instance configuration. `p_extend` and `p_lower_level_cfg` must be valid. |
| `p_cfg->p_extend` | Points to `rs_i2c_if_bus_extended_cfg_t`, which contains the shared-bus configuration. |
| `p_cfg->p_lower_level_cfg` | Points to the lower-level I2C driver configuration. |

### Operation

1. Validates `p_api_ctrl`, `p_cfg`, `p_cfg->p_extend`, and `p_cfg->p_lower_level_cfg`.
2. Casts `p_api_ctrl` to `rs_i2c_if_instance_ctrl_t`.
3. Casts `p_cfg->p_extend` to `rs_i2c_if_bus_extended_cfg_t`.
4. Rejects an instance whose `open` marker already equals `RS_I2C_IF_OPEN`.
5. Validates `p_bus->p_driver_instance`.
6. For RTOS builds, verifies that the blocking semaphore and recursive mutex are either both configured or both absent.
7. Initializes the control block with the configuration, bus, lower-level configuration, callback, and context.
8. Resets the transfer-data fields and SMBus operation flag.
9. When `BSP_CFG_RTOS == 2`, calls `rs_i2c_if_bus_os_init()`.
10. Calls `rs_i2c_if_bus_status_check()` to verify the lower-level bus.
11. Clears `p_bus->p_current_ctrl` if it already points to this control block, forcing address and callback configuration on the first transfer.
12. Sets `p_ctrl->open` to `RS_I2C_IF_OPEN`.

### RTOS Synchronization Validation

For an RTOS build, the following configurations are accepted:

- Both `p_blocking_semaphore` and `p_bus_recursive_mutex` are `NULL`.
- Both `p_blocking_semaphore` and `p_bus_recursive_mutex` are non-`NULL`.

A configuration with only one synchronization object is rejected with `FSP_ERR_ASSERTION`.

### Failure Cleanup

If RTOS-object initialization or bus-status checking fails, the function clears these control-block members before returning the error:

```c
p_ctrl->p_cfg             = NULL;
p_ctrl->p_bus             = NULL;
p_ctrl->p_lower_level_cfg = NULL;
p_ctrl->p_callback        = NULL;
p_ctrl->p_context         = NULL;
```
### Return Values

| Return value | Condition |
|---|---|
| `FSP_SUCCESS` | The module was configured and opened successfully. |
| `FSP_ERR_ASSERTION` | A required pointer is `NULL`, the driver instance is missing, or the RTOS synchronization-object combination is invalid. |
| `FSP_ERR_ALREADY_OPEN` | The control block is already open. |
| `FSP_ERR_COMMS_BUS_NOT_OPEN` | The lower-level I2C driver is not open, when reported by the bus-status check. |
| Other `fsp_err_t` | An error returned by RTOS-object initialization or bus-status checking. |

### Implementation Notes

- The open marker is written only after OS initialization and lower-level bus validation succeed.
- The failure path clears the initialized control-block references.
- If `rs_i2c_if_bus_os_init()` creates synchronization objects and a later bus-status check fails, their ownership and cleanup behavior should be explicitly defined. The corresponding cleanup routine should be called if the objects are owned by this instance.
---
### 10.7. `rs_i2c_if_ctrl_Close`

**Implements:** `rs_i2c_if_api_t::close`

Closes an `rs_i2c_if` instance, removes its shared-bus selection when applicable, and clears its runtime control state.

### Prototype

```c
fsp_err_t rs_i2c_if_ctrl_Close(
    rs_i2c_if_ctrl_t * const p_api_ctrl);
```
### Parameters

| Parameter | Description |
|---|---|
| `p_api_ctrl` | Pointer to the open `rs_i2c_if` control block. |

### Operation

1. Validates `p_api_ctrl`.
2. Casts the public control pointer to `rs_i2c_if_instance_ctrl_t`.
3. Verifies that the instance is open.
4. Verifies that the stored shared-bus pointer is valid.
5. Clears `p_current_ctrl` only if the closing instance currently owns the shared-bus selection.
6. Clears the open marker, configuration pointers, callback, context, transfer state, and SMBus state.
7. Returns `FSP_SUCCESS`.

### Cleared Runtime State

```c
p_ctrl->open                = 0U;
p_ctrl->p_cfg               = NULL;
p_ctrl->p_bus               = NULL;
p_ctrl->p_lower_level_cfg   = NULL;
p_ctrl->p_callback          = NULL;
p_ctrl->p_context           = NULL;
p_ctrl->p_transfer_data     = NULL;
p_ctrl->transfer_data_bytes = 0U;
p_ctrl->smbus_operation     = false;
```
### Return Values

| Return value | Condition |
|---|---|
| `FSP_SUCCESS` | The instance was closed and its control state was cleared. |
| `FSP_ERR_ASSERTION` | `p_api_ctrl` is `NULL` or the stored bus pointer is `NULL`. |
| `FSP_ERR_NOT_OPEN` | The module is not open. |

### Implementation Notes

- The function does not close the shared lower-level I2C driver. This supports a bus shared by multiple logical device instances.
- If `Open` dynamically creates RTOS synchronization objects, `Close` should call the corresponding deinitialization routine unless those objects are bus-owned and intentionally persistent.

## 11. Running the Project and Expected Output

This demonstration reads motion-sensor data and displays it on a UART terminal.

### 11.1. Hardware Connections

- Connect J8 on FPB-RA6E2/FPB-RA4E2 board or CN6 on FPB-RA2E3 to the PC for power, debugging, and J-Link virtual COM port.
- Connect a serial terminal to the J-Link virtual COM port with **115200 baud, 8N1**.

**FPB-RA6E2/FPB-RA4E2 - QCIOT-ICM42671POCZ:**

|Signal           | FPB-RA6E2/FPB-RA4E2 Arduino Uno Pin# | QCIOT-ICM42671POCZ J1/J2 Pin# |Note| 
|:----------------|------------------|----------------------------|--------------|
|VCC              | #J2-Pin 4        | #6                         |              |
|GND              | #J2-Pin 7        | #5                         |              |
|IRQ              | #J5-Pin 3        | #1                         |P105 (IRQ0)   | 
|I2C-SCL          | #J1-Pin 10       | #3                         |P100 (SCL)    |
|I2C-SDA          | #J1-Pin 9        | #4                         |P101 (SDA)    |

**Note:** Use one interrupt input (P105) since the module hardware design used only INT1 of the ICM42670 which is connected to QCIOT-ICM42671POCZ J2's IRQ pin (#pin 1).

**FPB-RA2E3 - QCIOT-ICM42671POCZ:**

|Signal           | FPB-RA2E3 PMOD2 Pin# | QCIOT-ICM42671POCZ J1/J2 Pin# |Note   | 
|:----------------|------------------|----------------------------|--------------|
|VCC              | #6               | #6                         |              |
|GND              | #5               | #5                         |              |
|IRQ              | #1               | #1                         |P408 (IRQ7)   | 
|I2C-SCL          | #3               | #3                         |P400 (SCL)    |
|I2C-SDA          | #4               | #4                         |P401 (SDA)    |

**Note:** Ensure E26, E28, E46, and E47 are connected to enable MCU UART communication through the J-Link Virtual COM Port.

### 11.2. Importing and Building Demonstration Project

For detailed instructions on importing, configuring, building, and running the demonstration project in Visual Studio Code, refer to the Renesas RA VS Code Documentation at the following link:

[creating-and-building-project.html#using-renesas-ra](https://tool-support.renesas.com/e2studio/vscode/docs/creating-and-building-project.html#using-renesas-ra)

### 11.3. Example Terminal Output

The following data show example terminal output from the demonstration projects.

```
*****************************************************
   RS I2C IF - Motion Sensor Demo
   Module Version 1.0.0
*****************************************************
Accel [X=-0.536, Y=0.291, Z=10.197] m/s^2
Accel [X=-0.544, Y=0.285, Z=10.210] m/s^2
Accel [X=-0.522, Y=0.266, Z=10.212] m/s^2
Accel [X=-0.541, Y=0.285, Z=10.203] m/s^2
Accel [X=-0.559, Y=0.285, Z=10.216] m/s^2
Accel [X=-0.528, Y=0.272, Z=10.189] m/s^2
Accel [X=-0.526, Y=0.249, Z=10.179] m/s^2
Accel [X=-0.535, Y=0.258, Z=10.189] m/s^2
Accel [X=-0.522, Y=0.272, Z=10.195] m/s^2
Accel [X=-0.529, Y=0.280, Z=10.204] m/s^2
Accel [X=-0.535, Y=0.266, Z=10.197] m/s^2
Accel [X=-0.528, Y=0.261, Z=10.184] m/s^2
Accel [X=-0.546, Y=0.278, Z=10.180] m/s^2
Accel [X=-0.535, Y=0.285, Z=10.182] m/s^2
Accel [X=-0.526, Y=0.276, Z=10.212] m/s^2
Accel [X=-0.563, Y=0.305, Z=10.188] m/s^2
Accel [X=-0.542, Y=0.257, Z=10.231] m/s^2
Accel [X=-0.544, Y=0.281, Z=10.180] m/s^2
Accel [X=-0.548, Y=0.281, Z=10.195] m/s^2

```