/*
 * ESP32-C3 Featured FreeRTOS IoT Integration V202204.00
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#ifndef OTA_CONFIG_H_
#define OTA_CONFIG_H_

/* ESP-IDF sdkconfig include. */
#include <sdkconfig.h>
#include "vinit.h"

/**
 * @brief The thing name of the device.
 */
#define otademoconfigCLIENT_IDENTIFIER        ( device_id )

/**
 * @brief The maximum size of the file paths used in the demo.
 */
#define otademoconfigMAX_FILE_PATH_SIZE       ( 260 )

/**
 * @brief The maximum size of the stream name required for downloading update file
 * from streaming service.
 */
#define otademoconfigMAX_STREAM_NAME_SIZE     ( 128 )

/**
 * @brief The delay used in the OTA demo task to periodically output the OTA
 * statistics like number of packets received, dropped, processed and queued per connection.
 */
#define otademoconfigTASK_DELAY_MS            ( 30000 )

/**
 * @brief The maximum time for which OTA demo waits for an MQTT operation to be complete.
 * This involves receiving an acknowledgment for broker for SUBSCRIBE, UNSUBSCRIBE and non
 * QOS0 publishes.
 */
#define otademoconfigMQTT_TIMEOUT_MS          ( 5000 )

/**
 * @brief The task priority of OTA agent task.
 */
#define otademoconfigAGENT_TASK_PRIORITY      ( 4 )

/**
 * @brief The stack size of OTA agent task.
 */
#define otademoconfigAGENT_TASK_STACK_SIZE    ( 4096 )

/**
 * @brief The task priority of the OTA demo task.
 */
#define otademoconfigDEMO_TASK_PRIORITY       ( 1 )

/**
 * @brief The task stack size of the OTA demo task.
 */
#define otademoconfigDEMO_TASK_STACK_SIZE     ( 3072 )

/**
 * @brief The version for the firmware which is running. OTA agent uses this
 * version number to perform anti-rollback validation. The firmware version for the
 * download image should be higher than the current version, otherwise the new image is
 * rejected in self test phase.
 */
#define APP_VERSION_MAJOR                     ( 0 )
#define APP_VERSION_MINOR                     ( 0 )
#define APP_VERSION_BUILD                     ( 0 )

#endif /* OTA_CONFIG_H_ */
