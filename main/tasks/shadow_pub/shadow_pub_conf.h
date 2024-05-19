
#ifndef SHADOW_PUB_CONF_H_
#define SHADOW_PUB_CONF_H_

/* ESP-IDF sdkconfig include. */
#include <sdkconfig.h>

/**
 * @brief Size of statically allocated buffers for holding topic names and
 * payloads.
 */

#define TOPIC_BUFFER_LENGTH                             ( ( unsigned int ) ( 128 ) ) /* bytes */
#define PAYLOAD_STRING_BUFFER_LENGTH                    ( ( unsigned int ) ( 1024 ) ) /* bytes */
/**
 * @brief Delay for each task between subscription, publish, unsubscription
 * loops.
 */
#define PAYLOAD_DELAY_BETWEEN_SUB_PUB_UNSUB_LOOPS_MS    ( ( unsigned int ) ( 10000 ) ) /* ms */

/**
 * @brief The maximum amount of time in milliseconds to wait for the commands
 * to be posted to the MQTT agent should the MQTT agent's command queue be full.
 * Tasks wait in the Blocked state, so don't use any CPU time.
 */
#define PAYLOAD_MAX_COMMAND_SEND_BLOCK_TIME_MS          ( ( unsigned int ) ( 500 ) ) /* ms */

/**
 * @brief The QoS level of MQTT messages sent by this demo. This must be 0 or 1
 * if using AWS as AWS only supports levels 0 or 1. If using another MQTT broker
 * that supports QoS level 2, this can be set to 2.
 */
#define PAYLOAD_QOS_LEVEL                               ( ( unsigned long ) ( 0 ) )

/**
 * @brief The number of SubPubUnsub tasks to create for this demo.
 */
#define PAYLOAD_NUM_TASKS_TO_CREATE                     ( ( unsigned long ) ( 1 ) )

/**
 * @brief The task priority of each of the SubPubUnsub tasks.
 */
#define PAYLOAD_TASK_PRIORITY                           ( ( unsigned int ) ( 1 ) )

/**
 * @brief The task stack size for each of the SubPubUnsub tasks.
 */
#define PAYLOAD_TASK_STACK_SIZE                         ( ( unsigned int ) ( 5120 ) )

#endif /* SHADOW_PUB_CONF_H_ */