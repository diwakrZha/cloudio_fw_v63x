/**
 * @file shadow_pub.c
 * @brief This file contains the implementation of the shadow publishing task.
 *
 * The shadow publishing task is responsible for publishing device shadow updates to the MQTT broker.
 * It includes functions for initializing the task, handling MQTT events, subscribing to topics, and publishing messages.
 * The task uses the coreMQTT library and the coreMQTT-Agent library for MQTT communication.
 * It also includes various utility functions for handling hardware setup, UART data, and timeouts.
 *
 * @note This file assumes the presence of external declarations and utilities.
 * @note This file assumes the presence of other header files and source files for the application's core functionality.
 */
/* Includes *******************************************************************/

/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <cJSON.h>
/* FreeRTOS includes. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <inttypes.h>
/* ESP-IDF includes. */
#include "esp_log.h"
#include "esp_event.h"
#include "sdkconfig.h"
#include <esp_sleep.h>

/* coreMQTT library include. */
#include "core_mqtt.h"

/* coreMQTT-Agent include. */
#include "core_mqtt_agent.h"

/* coreMQTT-Agent network manager include. */
#include "core_mqtt_agent_manager.h"
#include "core_mqtt_agent_manager_events.h"

/* Subscription manager include. */
#include "subscription_manager.h"

/* Public functions include. */
#include "shadow_pub.h"

/* Demo task configurations include. */
#include "shadow_pub_conf.h"

/*App core includes and drivers*/
#include "buzzer.h"
#include "ldo_control.h"
#include "led.h"
#include "aws_config_handler.h"
#include "json_payload.h"
#include "ping_time.h"
#include "dev_wdt.h"
#include "uart_handler.h"
#include "vinit.h"
#include "dev_sleep.h"
#include "psm_routine.h"
#include "app_wifi.h"
#include "sensors.h"
//#include "vmesh.h"
#include "device_config.h"

/* Preprocessor definitions ***************************************************/

/* coreMQTT-Agent event group bit definitions */
#define CORE_MQTT_AGENT_CONNECTED_BIT (1 << 0)
#define CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT (1 << 1)

// reset wifi creds defines
#define VOLTAGE_THRESHOLD 50
#define RESET_COUNT 1
#define TIME_WINDOW pdMS_TO_TICKS(10000) // 10 seconds in ticks

/*Global Variables*/
char configJSONdata[PAYLOAD_STRING_BUFFER_LENGTH];
bool newConfig = false;
#define WDT_TIMEOUT_SENDER_TASK (3 * conf.publish_interval) /*Multiplication factor of publish interval for the WDT timeout*/
/* Struct definitions *********************************************************/
/**
 * @brief Defines the structure to use as the incoming publish callback context
 * when data from a subscribed topic is received.
 */
typedef struct IncomingPublishCallbackContext
{
    TaskHandle_t xTaskToNotify;
    uint32_t ulNotificationValue;
    char pcIncomingPublish[PAYLOAD_STRING_BUFFER_LENGTH];
} IncomingPublishCallbackContext_t;

/**
 * @brief Defines the structure to use as the command callback context in this
 * demo.
 */
struct MQTTAgentCommandContext
{
    MQTTStatus_t xReturnStatus;
    TaskHandle_t xTaskToNotify;
    uint32_t ulNotificationValue;
    IncomingPublishCallbackContext_t *pxIncomingPublishCallbackContext;
    void *pArgs;
};

/**
 * @brief Parameters for this task.
 */
struct DemoParams
{
    uint32_t ulTaskNumber;
};

/* Global variables ***********************************************************/
static const char *TAG = "SHADOW_PUBLISH";

/**
 * @brief Static handle used for MQTT agent context.
 */
extern MQTTAgentContext_t xGlobalMqttAgentContext;

/**
 * @brief The event group used to manage coreMQTT-Agent events.
 */
static EventGroupHandle_t xNetworkEventGroup;

/**
 * @brief The semaphore used to lock access to ulMessageID to eliminate a race
 * condition in which multiple tasks try to increment/get ulMessageID.
 */
static SemaphoreHandle_t xMessageIdSemaphore;

/**
 * @brief The message ID for the next message sent by this demo.
 */
static uint32_t ulMessageId = 0;

// Assumed external declarations and utilities
static void setupHardware();
static void teardownHardware();
static void handleUartData(bool *isValidUartData, char *pcPayload, uint32_t *ulValueToNotify);
static bool waitForValidUartData(const TickType_t retryIntervalTicks, const TickType_t timeoutTick, char *pcPayload, uint32_t *ulValueToNotify);
static void handleTimeoutScenario(char *pcPayload, uint32_t *ulValueToNotify);

/* Static function declarations ***********************************************/

/**
 * @brief ESP Event Loop library handler for coreMQTT-Agent events.
 *
 * This handles events defined in core_mqtt_agent_events.h.
 */
static void prvCoreMqttAgentEventHandler(void *pvHandlerArg,
                                         esp_event_base_t xEventBase,
                                         int32_t lEventId,
                                         void *pvEventData);

/**
 * @brief Passed into MQTTAgent_Subscribe() as the callback to execute when the
 * broker ACKs the SUBSCRIBE message.  Its implementation sends a notification
 * to the task that called MQTTAgent_Subscribe() to let the task know the
 * SUBSCRIBE operation completed.  It also sets the xReturnStatus of the
 * structure passed in as the command's context to the value of the
 * xReturnStatus parameter - which enables the task to check the status of the
 * operation.
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] pxCommandContext Context of the initial command.
 * @param[in].xReturnStatus The result of the command.
 */
static void prvSubscribeCommandCallback(MQTTAgentCommandContext_t *pxCommandContext,
                                        MQTTAgentReturnInfo_t *pxReturnInfo);

static void prvUnsubscribeCommandCallback(MQTTAgentCommandContext_t *pxCommandContext,
                                          MQTTAgentReturnInfo_t *pxReturnInfo);

/**
 * @brief Passed into MQTTAgent_Publish() as the callback to execute when the
 * broker ACKs the PUBLISH message.  Its implementation sends a notification
 * to the task that called MQTTAgent_Publish() to let the task know the
 * PUBLISH operation completed.  It also sets the xReturnStatus of the
 * structure passed in as the command's context to the value of the
 * xReturnStatus parameter - which enables the task to check the status of the
 * operation.
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] pxCommandContext Context of the initial command.
 * @param[in].xReturnStatus The result of the command.
 */
static void prvPublishCommandCallback(MQTTAgentCommandContext_t *pxCommandContext,
                                      MQTTAgentReturnInfo_t *pxReturnInfo);

/**
 * @brief Called by the task to wait for a notification from a callback function
 * after the task first executes either MQTTAgent_Publish()* or
 * MQTTAgent_Subscribe().
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] pxCommandContext Context of the initial command.
 * @param[out] pulNotifiedValue The task's notification value after it receives
 * a notification from the callback.
 *
 * @return pdTRUE if the task received a notification, otherwise pdFALSE.
 */
static BaseType_t prvWaitForNotification(uint32_t *pulNotifiedValue);

/**
 * @brief Passed into MQTTAgent_Subscribe() as the callback to execute when
 * there is an incoming publish on the topic being subscribed to.  Its
 * implementation just logs information about the incoming publish including
 * the publish messages source topic and payload.
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] pvIncomingPublishCallbackContext Context of the initial command.
 * @param[in] pxPublishInfo Deserialized publish.
 */

static void prvIncomingPublishCallback(void *pvIncomingPublishCallbackContext,
                                       MQTTPublishInfo_t *pxPublishInfo);

/**
 * @brief Subscribe to the topic the demo task will also publish to - that
 * results in all outgoing publishes being published back to the task
 * (effectively echoed back).
 *
 * @param[in] pxIncomingPublishCallbackContext The callback context used when
 * data is received from pcTopicFilter.
 * @param[in] xQoS The quality of service (QoS) to use.  Can be zero or one
 * for all MQTT brokers.  Can also be QoS2 if supported by the broker.  AWS IoT
 * does not support QoS2.
 * @param[in] pcTopicFilter Topic filter to subscribe to.
 */
static void prvSubscribeToTopic(IncomingPublishCallbackContext_t *pxIncomingPublishCallbackContext,
                                MQTTQoS_t xQoS,
                                char *pcTopicFilter);

/**
 * @brief Unsubscribe to the topic the demo task will also publish to.
 *
 * @param[in] xQoS The quality of service (QoS) to use.  Can be zero or one
 * for all MQTT brokers.  Can also be QoS2 if supported by the broker.  AWS IoT
 * does not support QoS2.
 * @param[in] pcTopicFilter Topic filter to unsubscribe from.
 */
static void prvUnsubscribeToTopic(MQTTQoS_t xQoS,
                                  char *pcTopicFilter);

/**
 * @brief The function that implements the task demonstrated by this file.
 */
static void prvSubscribePublishUnsubscribeTask(void *pvParameters);

/* Static function definitions ************************************************/

static void prvCoreMqttAgentEventHandler(void *pvHandlerArg,
                                         esp_event_base_t xEventBase,
                                         int32_t lEventId,
                                         void *pvEventData)
{
    (void)pvHandlerArg;
    (void)xEventBase;
    (void)pvEventData;

    switch (lEventId)
    {
    case CORE_MQTT_AGENT_CONNECTED_EVENT:
        ESP_LOGI(TAG,
                 "coreMQTT-Agent connected.");
        xEventGroupSetBits(xNetworkEventGroup,
                           CORE_MQTT_AGENT_CONNECTED_BIT);
        break;

    case CORE_MQTT_AGENT_DISCONNECTED_EVENT:
        ESP_LOGI(TAG,
                 "coreMQTT-Agent disconnected. Preventing coreMQTT-Agent "
                 "commands from being enqueued.");
        xEventGroupClearBits(xNetworkEventGroup,
                             CORE_MQTT_AGENT_CONNECTED_BIT);
        break;

    case CORE_MQTT_AGENT_OTA_STARTED_EVENT:
        ESP_LOGI(TAG,
                 "OTA started. Preventing coreMQTT-Agent commands from "
                 "being enqueued.");
        xEventGroupClearBits(xNetworkEventGroup,
                             CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT);
        break;

    case CORE_MQTT_AGENT_OTA_STOPPED_EVENT:
        ESP_LOGI(TAG,
                 "OTA stopped. No longer preventing coreMQTT-Agent "
                 "commands from being enqueued.");
        xEventGroupSetBits(xNetworkEventGroup,
                           CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT);
        break;

    default:
        ESP_LOGE(TAG,
                 "coreMQTT-Agent event handler received unexpected event: %" PRIu32 "",
                 lEventId);
        break;
    }
}

static void prvPublishCommandCallback(MQTTAgentCommandContext_t *pxCommandContext,
                                      MQTTAgentReturnInfo_t *pxReturnInfo)
{
    /* Store the result in the application defined context so the task that
     * initiated the publish can check the operation's status. */
    pxCommandContext->xReturnStatus = pxReturnInfo->returnCode;

    if (pxCommandContext->xTaskToNotify != NULL)
    {
        /* Send the context's ulNotificationValue as the notification value so
         * the receiving task can check the value it set in the context matches
         * the value it receives in the notification. */
        xTaskNotify(pxCommandContext->xTaskToNotify,
                    pxCommandContext->ulNotificationValue,
                    eSetValueWithOverwrite);
    }
}

static void prvSubscribeCommandCallback(MQTTAgentCommandContext_t *pxCommandContext,
                                        MQTTAgentReturnInfo_t *pxReturnInfo)
{
    bool xSubscriptionAdded = false;
    MQTTAgentSubscribeArgs_t *pxSubscribeArgs = (MQTTAgentSubscribeArgs_t *)pxCommandContext->pArgs;

    /* Store the result in the application defined context so the task that
     * initiated the subscribe can check the operation's status. */
    pxCommandContext->xReturnStatus = pxReturnInfo->returnCode;

    /* Check if the subscribe operation is a success. */
    if (pxReturnInfo->returnCode == MQTTSuccess)
    {
        /* Add subscription so that incoming publishes are routed to the application
         * callback. */
        xSubscriptionAdded = addSubscription((SubscriptionElement_t *)xGlobalMqttAgentContext.pIncomingCallbackContext,
                                             pxSubscribeArgs->pSubscribeInfo->pTopicFilter,
                                             pxSubscribeArgs->pSubscribeInfo->topicFilterLength,
                                             prvIncomingPublishCallback,
                                             (void *)(pxCommandContext->pxIncomingPublishCallbackContext));

        if (xSubscriptionAdded == false)
        {
            ESP_LOGE(TAG,
                     "Failed to register an incoming publish callback for topic %.*s.",
                     pxSubscribeArgs->pSubscribeInfo->topicFilterLength,
                     pxSubscribeArgs->pSubscribeInfo->pTopicFilter);
        }
    }

    if (pxCommandContext->xTaskToNotify != NULL)
    {
        xTaskNotify(pxCommandContext->xTaskToNotify,
                    pxCommandContext->ulNotificationValue,
                    eSetValueWithOverwrite);
    }
}

static void prvUnsubscribeCommandCallback(MQTTAgentCommandContext_t *pxCommandContext,
                                          MQTTAgentReturnInfo_t *pxReturnInfo)
{
    MQTTAgentSubscribeArgs_t *pxUnsubscribeArgs = (MQTTAgentSubscribeArgs_t *)pxCommandContext->pArgs;

    /* Store the result in the application defined context so the task that
     * initiated the subscribe can check the operation's status. */
    pxCommandContext->xReturnStatus = pxReturnInfo->returnCode;

    /* Check if the unsubscribe operation is a success. */
    if (pxReturnInfo->returnCode == MQTTSuccess)
    {
        /* Remove subscription from subscription manager. */
        removeSubscription((SubscriptionElement_t *)xGlobalMqttAgentContext.pIncomingCallbackContext,
                           pxUnsubscribeArgs->pSubscribeInfo->pTopicFilter,
                           pxUnsubscribeArgs->pSubscribeInfo->topicFilterLength);
    }

    if (pxCommandContext->xTaskToNotify != NULL)
    {
        xTaskNotify(pxCommandContext->xTaskToNotify,
                    pxCommandContext->ulNotificationValue,
                    eSetValueWithOverwrite);
    }
}

static BaseType_t prvWaitForNotification(uint32_t *pulNotifiedValue)
{
    BaseType_t xReturn;

    /* Wait for this task to get notified, passing out the value it gets
     * notified with. */
    xReturn = xTaskNotifyWait(0,
                              0,
                              pulNotifiedValue,
                              portMAX_DELAY);
    return xReturn;
}

static void prvIncomingPublishCallback(void *pvIncomingPublishCallbackContext,
                                       MQTTPublishInfo_t *pxPublishInfo)
{

    IncomingPublishCallbackContext_t *pxIncomingPublishCallbackContext = (IncomingPublishCallbackContext_t *)pvIncomingPublishCallbackContext;

    /* Create a message that contains the incoming MQTT payload to the logger,
     * terminating the string first. */
    if (pxPublishInfo->payloadLength < PAYLOAD_STRING_BUFFER_LENGTH)
    {
        memcpy((void *)(pxIncomingPublishCallbackContext->pcIncomingPublish),
               pxPublishInfo->pPayload,
               pxPublishInfo->payloadLength);

        (pxIncomingPublishCallbackContext->pcIncomingPublish)[pxPublishInfo->payloadLength] = 0x00;
    }
    else
    {
        memcpy((void *)(pxIncomingPublishCallbackContext->pcIncomingPublish),
               pxPublishInfo->pPayload,
               PAYLOAD_STRING_BUFFER_LENGTH);

        (pxIncomingPublishCallbackContext->pcIncomingPublish)[PAYLOAD_STRING_BUFFER_LENGTH - 1] = 0x00;
    }

    xTaskNotify(pxIncomingPublishCallbackContext->xTaskToNotify,
                pxIncomingPublishCallbackContext->ulNotificationValue,
                eSetValueWithOverwrite);

    strncpy(configJSONdata, pxIncomingPublishCallbackContext->pcIncomingPublish, PAYLOAD_STRING_BUFFER_LENGTH - 1);
    configJSONdata[PAYLOAD_STRING_BUFFER_LENGTH - 1] = '\0'; // Ensure null-termination

    char topicBuffer[TOPIC_BUFFER_LENGTH];

    if (pxPublishInfo->topicNameLength < TOPIC_BUFFER_LENGTH)
    {
        memcpy(topicBuffer, pxPublishInfo->pTopicName, pxPublishInfo->topicNameLength);
        topicBuffer[pxPublishInfo->topicNameLength] = '\0'; // Null-terminate the string
    }
    if (strcmp(topicBuffer, shadow_update_delta_topic) == 0)
    {
        ESP_LOGI(TAG, "*CONFIG CHANGE DETECTED!*: %s on topic: %s, ", configJSONdata, topicBuffer);
        if (!processJsonAndStoreConfig(configJSONdata))
        {
            ESP_LOGE(TAG, "Failed to extract board settings from JSON!");
            // Handle the error as needed
        }
        else
        {
            ESP_LOGI(TAG, "*Adopting and reporting new configs* \n");
            set_LED(conf.led);
            set_BUZ(conf.buz);
            // uart_remove_driver(); // Remove UART driver
            // uart_initialize();    // Reinitialize UART with new baudrate
            newConfig = true; /*this triggers the reporting part of the prvConfigPublishTask*/
        }
    }
    else if (strcmp(topicBuffer, shadow_get_accepted_topic) == 0)
    {
        ESP_LOGI(TAG, "*GOT INITIAL CONFIG*: %s on topic: %s, ", configJSONdata, topicBuffer);
        if (!processJsonAndStoreConfig(configJSONdata))
        {
            ESP_LOGE(TAG, "Failed to extract board settings from JSON!");
            // Handle the error as needed
        }
        else
        {
            ESP_LOGI(TAG, "*Adopting and Reporting the adopted board settings!* \n");
            set_LED(conf.led);
            set_BUZ(conf.buz);
            // uart_remove_driver(); // Remove UART driver
            // uart_initialize();    // Reinitialize UART with new baudrate
            newConfig = true; /*this triggers the reporting part of the prvConfigPublishTask*/
        }
    }
    else
    {
        ESP_LOGI(TAG, "*Got unknown config*: %s on topic: %s, ", configJSONdata, topicBuffer);
    }
}

static void prvPublishToTopic(MQTTQoS_t xQoS,
                              char *pcTopicName,
                              char *pcPayload)
{
    uint32_t ulPublishMessageId, ulNotifiedValue = 0;

    MQTTStatus_t xCommandAdded;
    BaseType_t xCommandAcknowledged = pdFALSE;

    MQTTPublishInfo_t xPublishInfo = {0};

    MQTTAgentCommandContext_t xCommandContext = {0};
    MQTTAgentCommandInfo_t xCommandParams = {0};

    xTaskNotifyStateClear(NULL);

    /* Create a unique number for the publish that is about to be sent.
     * This number is used in the command context and is sent back to this task
     * as a notification in the callback that's executed upon receipt of the
     * publish from coreMQTT-Agent.
     * That way this task can match an acknowledgment to the message being sent.
     */
    xSemaphoreTake(xMessageIdSemaphore, portMAX_DELAY);
    {
        ++ulMessageId;
        ulPublishMessageId = ulMessageId;
    }
    xSemaphoreGive(xMessageIdSemaphore);

    /* Configure the publish operation. The topic name string must persist for
     * duration of publish! */
    xPublishInfo.qos = xQoS;
    xPublishInfo.pTopicName = pcTopicName;
    xPublishInfo.topicNameLength = (uint16_t)strlen(pcTopicName);
    xPublishInfo.pPayload = pcPayload;
    xPublishInfo.payloadLength = (uint16_t)strlen(pcPayload);

    /* Complete an application defined context associated with this publish
     * message.
     * This gets updated in the callback function so the variable must persist
     * until the callback executes. */
    xCommandContext.ulNotificationValue = ulPublishMessageId;
    xCommandContext.xTaskToNotify = xTaskGetCurrentTaskHandle();

    xCommandParams.blockTimeMs = PAYLOAD_MAX_COMMAND_SEND_BLOCK_TIME_MS;
    xCommandParams.cmdCompleteCallback = prvPublishCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = &xCommandContext;

    do
    {
        /* Wait for coreMQTT-Agent task to have working network connection and
         * not be performing an OTA update. */
        xEventGroupWaitBits(xNetworkEventGroup,
                            CORE_MQTT_AGENT_CONNECTED_BIT | CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT,
                            pdFALSE,
                            pdTRUE,
                            portMAX_DELAY);

        ESP_LOGI(TAG,
                 "Task \"%s\" sending publish request to coreMQTT-Agent with message \"%s\" on topic \"%s\" with ID %" PRIu32 ".",
                 pcTaskGetName(xCommandContext.xTaskToNotify),
                 pcPayload,
                 pcTopicName,
                 ulPublishMessageId);

        /* To ensure ulNotification doesn't accidentally hold the expected value
         * as it is to be checked against the value sent from the callback.. */
        ulNotifiedValue = ~ulPublishMessageId;

        xCommandAcknowledged = pdFALSE;

        xCommandAdded = MQTTAgent_Publish(&xGlobalMqttAgentContext,
                                          &xPublishInfo,
                                          &xCommandParams);

        if (xCommandAdded == MQTTSuccess)
        {
            /* For QoS 1 and 2, wait for the publish acknowledgment.  For QoS0,
             * wait for the publish to be sent. */
            ESP_LOGI(TAG,
                     "Task \"%s\" waiting for publish %" PRIu32 " to complete.",
                     pcTaskGetName(xCommandContext.xTaskToNotify),
                     ulPublishMessageId);

            xCommandAcknowledged = prvWaitForNotification(&ulNotifiedValue);
        }
        else
        {
            ESP_LOGE(TAG,
                     "Failed to enqueue publish command. Error code=%s",
                     MQTT_Status_strerror(xCommandAdded));
        }

        /* Check all ways the status was passed back just for demonstration
         * purposes. */
        if ((xCommandAcknowledged != pdTRUE) ||
            (xCommandContext.xReturnStatus != MQTTSuccess) ||
            (ulNotifiedValue != ulPublishMessageId))
        {
            ESP_LOGW(TAG,
                     "Error or timed out waiting for ack for publish message %" PRIu32 ". Re-attempting publish.",
                     ulPublishMessageId);
        }
        else
        {
            ESP_LOGI(TAG,
                     "Publish %" PRIu32 " succeeded for task \"%s\".",
                     ulPublishMessageId,
                     pcTaskGetName(xCommandContext.xTaskToNotify));
        }
    } while ((xCommandAcknowledged != pdTRUE) ||
             (xCommandContext.xReturnStatus != MQTTSuccess) ||
             (ulNotifiedValue != ulPublishMessageId));
}

static void prvSubscribeToTopic(IncomingPublishCallbackContext_t *pxIncomingPublishCallbackContext,
                                MQTTQoS_t xQoS,
                                char *pcTopicFilter)
{
    uint32_t ulSubscribeMessageId, ulNotifiedValue = 0;

    MQTTStatus_t xCommandAdded;
    BaseType_t xCommandAcknowledged = pdFALSE;

    MQTTAgentSubscribeArgs_t xSubscribeArgs = {0};
    MQTTSubscribeInfo_t xSubscribeInfo = {0};

    MQTTAgentCommandContext_t xCommandContext = {0};
    MQTTAgentCommandInfo_t xCommandParams = {0};

    xTaskNotifyStateClear(NULL);

    /* Create a unique number for the subscribe that is about to be sent.
     * This number is used in the command context and is sent back to this task
     * as a notification in the callback that's executed upon receipt of the
     * publish from coreMQTT-Agent.
     * That way this task can match an acknowledgment to the message being sent.
     */
    xSemaphoreTake(xMessageIdSemaphore, portMAX_DELAY);
    {
        ++ulMessageId;
        ulSubscribeMessageId = ulMessageId;
    }
    xSemaphoreGive(xMessageIdSemaphore);

    /* Configure the subscribe operation.  The topic string must persist for
     * duration of subscription! */
    xSubscribeInfo.qos = xQoS;
    xSubscribeInfo.pTopicFilter = pcTopicFilter;
    xSubscribeInfo.topicFilterLength = (uint16_t)strlen(pcTopicFilter);

    xSubscribeArgs.pSubscribeInfo = &xSubscribeInfo;
    xSubscribeArgs.numSubscriptions = 1;

    /* Complete an application defined context associated with this subscribe
     * message.
     * This gets updated in the callback function so the variable must persist
     * until the callback executes. */
    xCommandContext.ulNotificationValue = ulSubscribeMessageId;
    xCommandContext.xTaskToNotify = xTaskGetCurrentTaskHandle();
    xCommandContext.pxIncomingPublishCallbackContext = pxIncomingPublishCallbackContext;
    xCommandContext.pArgs = (void *)&xSubscribeArgs;

    xCommandParams.blockTimeMs = PAYLOAD_MAX_COMMAND_SEND_BLOCK_TIME_MS;
    xCommandParams.cmdCompleteCallback = prvSubscribeCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = (void *)&xCommandContext;

    do
    {
        /* Wait for coreMQTT-Agent task to have working network connection and
         * not be performing an OTA update. */
        xEventGroupWaitBits(xNetworkEventGroup,
                            CORE_MQTT_AGENT_CONNECTED_BIT | CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT,
                            pdFALSE,
                            pdTRUE,
                            portMAX_DELAY);

        ESP_LOGI(TAG,
                 "Task \"%s\" sending subscribe request to coreMQTT-Agent for topic filter: %s with id %" PRIu32 "",
                 pcTaskGetName(xCommandContext.xTaskToNotify),
                 pcTopicFilter,
                 ulSubscribeMessageId);

        xCommandAcknowledged = pdFALSE;

        xCommandAdded = MQTTAgent_Subscribe(&xGlobalMqttAgentContext,
                                            &xSubscribeArgs,
                                            &xCommandParams);

        if (xCommandAdded == MQTTSuccess)
        {
            /* For QoS 1 and 2, wait for the subscription acknowledgment. For QoS0,
             * wait for the subscribe to be sent. */
            xCommandAcknowledged = prvWaitForNotification(&ulNotifiedValue);
        }
        else
        {
            ESP_LOGE(TAG,
                     "Failed to enqueue subscribe command. Error code=%s",
                     MQTT_Status_strerror(xCommandAdded));
        }

        /* Check all ways the status was passed back just for demonstration
         * purposes. */
        if ((xCommandAcknowledged != pdTRUE) ||
            (xCommandContext.xReturnStatus != MQTTSuccess) ||
            (ulNotifiedValue != ulSubscribeMessageId))
        {
            ESP_LOGW(TAG,
                     "Error or timed out waiting for ack to subscribe message %" PRIu32 ". Re-attempting subscribe.",
                     ulSubscribeMessageId);
        }
        else
        {
            ESP_LOGI(TAG,
                     "Subscribe %" PRIu32 " for topic filter %s succeeded for task \"%s\".",
                     ulSubscribeMessageId,
                     pcTopicFilter,
                     pcTaskGetName(xCommandContext.xTaskToNotify));
        }
    } while ((xCommandAcknowledged != pdTRUE) ||
             (xCommandContext.xReturnStatus != MQTTSuccess) ||
             (ulNotifiedValue != ulSubscribeMessageId));
}

static void prvUnsubscribeToTopic(MQTTQoS_t xQoS,
                                  char *pcTopicFilter)
{
    uint32_t ulUnsubscribeMessageId, ulNotifiedValue = 0;

    MQTTStatus_t xCommandAdded;
    BaseType_t xCommandAcknowledged = pdFALSE;

    MQTTAgentSubscribeArgs_t xUnsubscribeArgs = {0};
    MQTTSubscribeInfo_t xUnsubscribeInfo = {0};

    MQTTAgentCommandContext_t xCommandContext = {0};
    MQTTAgentCommandInfo_t xCommandParams = {0};

    xTaskNotifyStateClear(NULL);

    /* Create a unique number for the subscribe that is about to be sent.
     * This number is used in the command context and is sent back to this task
     * as a notification in the callback that's executed upon receipt of the
     * publish from coreMQTT-Agent.
     * That way this task can match an acknowledgment to the message being sent.
     */
    xSemaphoreTake(xMessageIdSemaphore, portMAX_DELAY);
    {
        ++ulMessageId;
        ulUnsubscribeMessageId = ulMessageId;
    }
    xSemaphoreGive(xMessageIdSemaphore);

    /* Configure the subscribe operation.  The topic string must persist for
     * duration of subscription! */
    xUnsubscribeInfo.qos = xQoS;
    xUnsubscribeInfo.pTopicFilter = pcTopicFilter;
    xUnsubscribeInfo.topicFilterLength = (uint16_t)strlen(pcTopicFilter);

    xUnsubscribeArgs.pSubscribeInfo = &xUnsubscribeInfo;
    xUnsubscribeArgs.numSubscriptions = 1;

    /* Complete an application defined context associated with this subscribe
     * message.
     * This gets updated in the callback function so the variable must persist
     * until the callback executes. */
    xCommandContext.ulNotificationValue = ulUnsubscribeMessageId;
    xCommandContext.xTaskToNotify = xTaskGetCurrentTaskHandle();
    xCommandContext.pArgs = (void *)&xUnsubscribeArgs;

    xCommandParams.blockTimeMs = PAYLOAD_MAX_COMMAND_SEND_BLOCK_TIME_MS;
    xCommandParams.cmdCompleteCallback = prvUnsubscribeCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = (void *)&xCommandContext;

    do
    {
        /* Wait for coreMQTT-Agent task to have working network connection and
         * not be performing an OTA update. */
        xEventGroupWaitBits(xNetworkEventGroup,
                            CORE_MQTT_AGENT_CONNECTED_BIT | CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT,
                            pdFALSE,
                            pdTRUE,
                            portMAX_DELAY);
        ESP_LOGI(TAG,
                 "Task \"%s\" sending unsubscribe request to coreMQTT-Agent for topic filter: %s with id %" PRIu32 "",
                 pcTaskGetName(xCommandContext.xTaskToNotify),
                 pcTopicFilter,
                 ulUnsubscribeMessageId);

        xCommandAcknowledged = pdFALSE;

        xCommandAdded = MQTTAgent_Unsubscribe(&xGlobalMqttAgentContext,
                                              &xUnsubscribeArgs,
                                              &xCommandParams);

        if (xCommandAdded == MQTTSuccess)
        {
            /* For QoS 1 and 2, wait for the subscription acknowledgment.  For QoS0,
             * wait for the subscribe to be sent. */
            xCommandAcknowledged = prvWaitForNotification(&ulNotifiedValue);
        }
        else
        {
            ESP_LOGE(TAG,
                     "Failed to enqueue unsubscribe command. Error code=%s",
                     MQTT_Status_strerror(xCommandAdded));
        }

        /* Check all ways the status was passed back just for demonstration
         * purposes. */
        if ((xCommandAcknowledged != pdTRUE) ||
            (xCommandContext.xReturnStatus != MQTTSuccess) ||
            (ulNotifiedValue != ulUnsubscribeMessageId))
        {
            ESP_LOGW(TAG,
                     "Error or timed out waiting for ack to unsubscribe message %" PRIu32 ". Re-attempting subscribe.",
                     ulUnsubscribeMessageId);
        }
        else
        {
            ESP_LOGI(TAG,
                     "Unsubscribe %" PRIu32 " for topic filter %s succeeded for task \"%s\".",
                     ulUnsubscribeMessageId,
                     pcTopicFilter,
                     pcTaskGetName(xCommandContext.xTaskToNotify));
        }
    } while ((xCommandAcknowledged != pdTRUE) ||
             (xCommandContext.xReturnStatus != MQTTSuccess) ||
             (ulNotifiedValue != ulUnsubscribeMessageId));
}

bool prvSubscribeAll(IncomingPublishCallbackContext_t *context, MQTTQoS_t xQoS)
{
    char pcPayloadEmpty[1] = "";
    uint32_t ulNotifiedValue = 0; // Declare ulNotifiedValue

    // Subscribe to shadow_get_accepted_topic
    prvSubscribeToTopic(context, xQoS, shadow_get_accepted_topic);
    ESP_LOGI(TAG, "Attempted to subscribe to %s", shadow_get_accepted_topic);

    // Publish to shadow_get_topic
    prvPublishToTopic(xQoS, shadow_get_topic, pcPayloadEmpty);
    ESP_LOGI(TAG, "Attempted to publish to %s", shadow_get_topic);

    prvWaitForNotification(&ulNotifiedValue);
    // Unsubscribe from shadow_get_accepted_topic
    prvUnsubscribeToTopic(xQoS, shadow_get_accepted_topic);
    ESP_LOGI(TAG, "Attempted to unsubscribe from %s", shadow_get_accepted_topic);

    // Subscribe to shadow_update_delta_topic
    prvSubscribeToTopic(context, xQoS, shadow_update_delta_topic);
    ESP_LOGI(TAG, "Attempted to subscribe to %s", shadow_update_delta_topic);
    return true;
}

static void prvConfigPublishTask(void *pvParameters)
{
    // struct DemoParams *pxParams = (struct DemoParams *)pvParameters;
    MQTTQoS_t xQoS;
    char pcPayload[PAYLOAD_STRING_BUFFER_LENGTH];
    uint32_t ulValueToNotify = 0UL;
    // IncomingPublishCallbackContext_t xIncomingPublishCallbackContext;
    // xIncomingPublishCallbackContext.ulNotificationValue = pxParams->ulTaskNumber;
    // xIncomingPublishCallbackContext.xTaskToNotify = xTaskGetCurrentTaskHandle();

    xQoS = (MQTTQoS_t)PAYLOAD_QOS_LEVEL;

    while (1)
    {
        if (newConfig) // Assuming newConfig is a global variable
        {
            ldo_on();
            vTaskDelay(pdMS_TO_TICKS(10)); // Delay to switch on LDO
            led_red_on();
            prepReportJsonPayload(pcPayload, ulValueToNotify);

            /*Publish Data*/
            prvPublishToTopic(xQoS, shadow_update_topic, pcPayload);

            if (conf.wifi_rst == 1)
            {
                ESP_LOGI(TAG, "Resetting WiFi credentials flags on shadows\n");
                wifi_rst_reinit_JsonPayload(pcPayload, ulValueToNotify); // Reset WiFi Credentials n desired fields on shadow
                prvPublishToTopic(xQoS, shadow_update_topic, pcPayload);
                // prvWaitForNotification(&ulValueToNotify);
                reset_wifi_credentials(); // Reset WiFi Credentials
                ESP_LOGI(TAG, "WiFi Credentials reset complete, Rebooting ...\n");
                vTaskDelay(pdMS_TO_TICKS(100)); // Delay to switch on LDO
                esp_restart();
            }

            //bz_beat_up();
            newConfig = false;

            setup_wdt(WDT_TIMEOUT_SENDER_TASK); // Set up WDT with a 10-second timeout
            feed_wdt();                         // Feed the WDT

            led_off();
            ldo_off();
        }
        else
        {
            // If newConfig is false, delay for a longer period to minimize CPU usage.
            /*This will automatically adjust sleep to sending period
             *If you want to manually force idle freertos delay then use
             */
            //set_sleep_configs();
            //IDLE_DELAY(0.1); //for 5 seconds

            device_sleep(0);
            //device_sleep(0);
            // IDLE_DELAY(0.5) //for 5 seconds
        }
    }
    vTaskDelete(NULL);
}

static void prvSubscribePublishUnsubscribeTask(void *pvParameters)
{
    struct DemoParams *pxParams = (struct DemoParams *)pvParameters;
    MQTTQoS_t xQoS;
    char pcPayload[PAYLOAD_STRING_BUFFER_LENGTH];
    uint32_t ulValueToNotify = 0UL;

    IncomingPublishCallbackContext_t xIncomingPublishCallbackContext;
    xIncomingPublishCallbackContext.ulNotificationValue = pxParams->ulTaskNumber;
    xIncomingPublishCallbackContext.xTaskToNotify = xTaskGetCurrentTaskHandle();
    xQoS = (MQTTQoS_t)PAYLOAD_QOS_LEVEL;

    if (prvSubscribeAll(&xIncomingPublishCallbackContext, xQoS))
    {
        ESP_LOGI(TAG, "Subscribed to topics");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to subscribe to topics");
    }

    setup_wdt(WDT_TIMEOUT_SENDER_TASK); // Set up WDT with a 10-second timeout
    feed_wdt();                         // Feed the WDT

    const uint32_t publishIntervalMs = conf.publish_interval * 1000;

    const uint32_t dynamicRetryInterval = MAX(5000, MIN(GET_UART_RETRY_INTERVAL, publishIntervalMs / 4));
    const TickType_t retryIntervalTicks = pdMS_TO_TICKS(dynamicRetryInterval);
    const TickType_t dynamicTimeout = MIN(GET_UART_READ_TIMEOUT, (publishIntervalMs * 2 / 3));

    while (1)
    {
        setupHardware();

        const TickType_t timeoutTick = xTaskGetTickCount() + pdMS_TO_TICKS(dynamicTimeout); // UART read timeout
        bool isValidUartData = waitForValidUartData(retryIntervalTicks, timeoutTick, pcPayload, &ulValueToNotify);

        ESP_LOGI(TAG, "Publish interval: %lu, UART read timeout: %lu, Uart retry loop delay: %lu ", publishIntervalMs, dynamicTimeout, dynamicRetryInterval);
        if (!isValidUartData)
        {
            handleTimeoutScenario(pcPayload, &ulValueToNotify);
        }
        prvPublishToTopic((MQTTQoS_t)PAYLOAD_QOS_LEVEL, shadow_payload_update_topic, pcPayload);
        led_green_on();

        teardownHardware();
        IDLE_DELAY(0.1); //for 1 seconds

        device_sleep(0); // Sleep or delay to match the publish interval

        //ldo_off();
        //eth_pwr_off();
    }

    vTaskDelete(NULL); // Cleanup at the end of the task
}

static void setupHardware()
{
    ESP_LOGI(TAG, "Setting up hardware");
    //set_wake_configs();
    disable_holds();
    //reset_gpios_dis_hold();
    ldo_on();
    //eth_pwr_on();
    led_blue_on();
    start_uart_data_task();
    setup_wdt(WDT_TIMEOUT_SENDER_TASK);
}

static void teardownHardware()
{
    ESP_LOGI(TAG, "Tearing down hardware");
    led_off();
    ldo_off();
    eth_pwr_off();
    set_sleep_configs();
    feed_wdt();
}

static bool waitForValidUartData(const TickType_t retryIntervalTicks, const TickType_t timeoutTick, char *pcPayload, uint32_t *ulValueToNotify)
{
    bool isValidUartData = false;
    while (xTaskGetTickCount() < timeoutTick && !isValidUartData)
    {
        handleUartData(&isValidUartData, pcPayload, ulValueToNotify);
        if (!isValidUartData)
        {
            vTaskDelay(retryIntervalTicks);
        }
    }
    return isValidUartData;
}

static void handleUartData(bool *isValidUartData, char *pcPayload, uint32_t *ulValueToNotify)
{
    prepareJsonPayload(pcPayload, *ulValueToNotify, isValidUartData);
    if (*isValidUartData)
    {
        stop_uart_data_task();
        ESP_LOGI(TAG, "Received Valid RS485 data. Stopping RS485 task");
    }
    else
    {
        ESP_LOGW(TAG, "Retrying to get Valid RS485 data");
    }
}

static void handleTimeoutScenario(char *pcPayload, uint32_t *ulValueToNotify)
{
    ESP_LOGW(TAG, "Retry Timed Out sending NAN on RS485");
    prepareJsonPayload(pcPayload, *ulValueToNotify, &(bool){false}); // Assume prepareJsonPayload can handle false isValidUartData to prepare NAN payload
    stop_uart_data_task();
}

/* Public function definitions ************************************************/
void vStartShadowPub(void)
{
    static struct DemoParams pxParams[PAYLOAD_NUM_TASKS_TO_CREATE];
    char pcTaskNameBuf[15];
    uint32_t ulTaskNumber;

    xMessageIdSemaphore = xSemaphoreCreateMutex();
    xNetworkEventGroup = xEventGroupCreate();
    xCoreMqttAgentManagerRegisterHandler(prvCoreMqttAgentEventHandler);

    /* Initialize the coreMQTT-Agent event group. */
    xEventGroupSetBits(xNetworkEventGroup,
                       CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT);

    ulTaskNumber = 0 /*change this to create a new task identifier */;

    memset(pcTaskNameBuf,
           0x00,
           sizeof(pcTaskNameBuf));

    snprintf(pcTaskNameBuf,
             sizeof(pcTaskNameBuf) - 1,
             "%s%d",
             "dataSender",
             (int)ulTaskNumber);

    pxParams[ulTaskNumber].ulTaskNumber = ulTaskNumber;

    xTaskCreate(prvSubscribePublishUnsubscribeTask,
                pcTaskNameBuf,
                PAYLOAD_TASK_STACK_SIZE,
                (void *)&pxParams[ulTaskNumber],
                PAYLOAD_TASK_PRIORITY,
                NULL);

    ulTaskNumber = 1 /*change this to create a new task identifier */;

    memset(pcTaskNameBuf,
           0x00,
           sizeof(pcTaskNameBuf));

    snprintf(pcTaskNameBuf,
             sizeof(pcTaskNameBuf) - 1,
             "%s%d",
             "confSender",
             (int)ulTaskNumber);

    pxParams[ulTaskNumber].ulTaskNumber = ulTaskNumber;

    xTaskCreate(prvConfigPublishTask,
                pcTaskNameBuf,
                PAYLOAD_TASK_STACK_SIZE,
                (void *)&pxParams[ulTaskNumber],
                PAYLOAD_TASK_PRIORITY,
                NULL);

    // ulTaskNumber = 2 /*change this to create a new task identifier */;

    // memset(pcTaskNameBuf,
    //        0x00,
    //        sizeof(pcTaskNameBuf));

    // snprintf(pcTaskNameBuf,
    //          sizeof(pcTaskNameBuf) - 1,
    //          "%s%d",
    //          "vmesh",
    //          (int)ulTaskNumber);

    // pxParams[ulTaskNumber].ulTaskNumber = ulTaskNumber;

    // xTaskCreate(mesh_send_task,
    //             pcTaskNameBuf,
    //             PAYLOAD_TASK_STACK_SIZE,
    //             (void *)&pxParams[ulTaskNumber],
    //             PAYLOAD_TASK_PRIORITY,
    //             NULL);

    // Start the hall sensor monitoring task

    // vTaskDelay(pdMS_TO_TICKS(500)); // Delay to switch on LDO

    // vmesh_init(get_wifi_connection_info);
    //  You can add more conditions here for other task names if needed
}

/* Each instance of prvSubscribePublishUnsubscribeTask() generates a unique
 * name and topic filter for itself from the number passed in as the task
 * parameter. */
/* Create a few instances of prvSubscribePublishUnsubscribeTask(). */
