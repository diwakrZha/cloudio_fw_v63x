#include "messaging.h"
#include "esp_timer.h"
#include "ping_time.h"
#include "get_mesh_data.h" // Include the header where registerIncomingJsonHandler and IncomingJsonHandler are declared
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "aws_config_handler.h"

#define ESPNOW_WIFI_IF ESP_IF_WIFI_STA

#define TYPE_PAIR 0
#define TYPE_DATA 1
#define TYPE_ACK 2
#define TYPE_PAIR_ACK 3

#define QUEUE_SIZE 10

static const char *TAG = "VROOT_NODE";

uint8_t key[ESP_NOW_KEY_LEN] = {'v', 'o', 'u', 'k', '_', 'k', '1', 'y', '_', 'h', 'e', 'r', 'o'}; // Adjust your key
// uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t broadcastAddress[ESP_NOW_ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
uint8_t sensor_mac[ESP_NOW_ETH_ALEN] = {0x40, 0x4c, 0xca, 0x46, 0xb2, 0x74};

int channel;

typedef struct
{
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int len;
} ESPNowReceiveStruct;

// Function to convert MAC address to string
char *get_mac_str(const uint8_t *mac_addr);
void send_data_ack(const uint8_t *mac_addr, int id);

static cJSON *latestIncomingMessage = NULL;
static SemaphoreHandle_t messageMutex = NULL;

void initMessagingModule()
{
    messageMutex = xSemaphoreCreateMutex();
}

void cleanupMessagingModule()
{
    if (messageMutex)
    {
        if (xSemaphoreTake(messageMutex, portMAX_DELAY) == pdTRUE)
        {
            if (latestIncomingMessage)
            {
                cJSON_Delete(latestIncomingMessage);
                latestIncomingMessage = NULL;
            }
            xSemaphoreGive(messageMutex);
        }
        vSemaphoreDelete(messageMutex);
        messageMutex = NULL;
    }
}

void updateLatestIncomingMessage(cJSON *json)
{
    if (xSemaphoreTake(messageMutex, portMAX_DELAY) == pdTRUE)
    {
        if (latestIncomingMessage)
        {
            cJSON_Delete(latestIncomingMessage);
        }
        latestIncomingMessage = cJSON_Duplicate(json, 1); // Make a deep copy
        xSemaphoreGive(messageMutex);
    }
}

cJSON *getLatestIncomingMessage()
{
    cJSON *copy = NULL;
    if (xSemaphoreTake(messageMutex, portMAX_DELAY) == pdTRUE)
    {
        copy = cJSON_Duplicate(latestIncomingMessage, 1); // Return a copy to avoid alteration
        xSemaphoreGive(messageMutex);
    }
    return copy; // Caller must delete this cJSON object
}

/*---------- ESPNow Interrupts: Posting to queues ----------*/

void OnESPNowSend(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    static const char *TAG = "OnESPNowSend";

    ESP_LOGD(TAG, "Last Packet Send Status: %d", status == ESP_NOW_SEND_SUCCESS);
}

static void OnESPNowRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
{
    static const char *TAG = "OnESPNowRecv";
    ESPNowReceiveStruct *recvStruct = malloc(sizeof(ESPNowReceiveStruct));

    if (recvStruct == NULL)
    {
        ESP_LOGE(TAG, "Memory allocation failed for ESPNowReceiveStruct");
        return;
    }

    memcpy(recvStruct->mac_addr, recv_info->src_addr, ESP_NOW_ETH_ALEN);
    recvStruct->data = malloc(len);
    if (recvStruct->data == NULL)
    {
        ESP_LOGE(TAG, "Memory allocation failed for data");
        free(recvStruct);
        return;
    }
    memcpy(recvStruct->data, incomingData, len);
    recvStruct->len = len;

    if (xQueueSend(incomingESPNowQueue, &recvStruct, portMAX_DELAY) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to send packet to incomingESPNowQueue queue");
        free(recvStruct->data);
        free(recvStruct);
    }
}

/*---------- RTOS Tasks ----------*/

void sendESPNowTask(void *pvParameters)
{
    static const char *TAG = "sendESPNowTask";
    ESPNowMessage outgoingMessage;

    while (1)
    {
        // Receive the Message struct from the queue
        if (xQueueReceive(outgoingESPNowQueue, &outgoingMessage, portMAX_DELAY) == pdTRUE)
        {

            ESP_LOGD(TAG, "Received message from outgoingESPNowQueue: %s", outgoingMessage.bodyserialized);
            ESP_LOGD(TAG, "Sending to %02x:%02x:%02x:%02x:%02x:%02x", outgoingMessage.destinationMAC[0], outgoingMessage.destinationMAC[1], outgoingMessage.destinationMAC[2], outgoingMessage.destinationMAC[3], outgoingMessage.destinationMAC[4], outgoingMessage.destinationMAC[5]);

            int len = strlen(outgoingMessage.bodyserialized) + 1;

            esp_err_t result = esp_now_send(outgoingMessage.destinationMAC, (uint8_t *)outgoingMessage.bodyserialized, len);

            if (result == ESP_OK)
            {
                ESP_LOGD(TAG, "Published packet to ESP-NOW: %s", outgoingMessage.bodyserialized);
            }
            else
            {
                ESP_LOGE(TAG, "Error: %s while sending to %s", esp_err_to_name(result), get_mac_str(outgoingMessage.destinationMAC));
            }

            ESP_LOGD(TAG, "Freeing memory...");
            free(outgoingMessage.bodyserialized);
        }
    }
}

void receiveESPNowTask(void *pvParameters)
{
    static const char *TAG = "receiveESPNowTask";
    messageHandler handler = (messageHandler)pvParameters;
    ESPNowReceiveStruct *recvStruct;
    const char *errorPtr;

    while (1)
    {
        ESP_LOGD(TAG, "Waiting for incoming data...");
        if (xQueueReceive(incomingESPNowQueue, &recvStruct, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGD(TAG, "Received message from incomingESPNowQueue");

            cJSON *incomingJSON = cJSON_ParseWithOpts((char *)recvStruct->data, &errorPtr, 0);
            if (incomingJSON == NULL)
            {
                ESP_LOGE(TAG, "Failed to parse incoming JSON: Error at %s", errorPtr);
                free(recvStruct->data);
                free(recvStruct);
                continue;
            }

            // // Checking if the type is 1
            // cJSON *type = cJSON_GetObjectItem(incomingJSON, "type");
            // if (cJSON_IsNumber(type) && type->valueint == 1) {
            //     // Extract ID from JSON
            //     cJSON *idJson = cJSON_GetObjectItem(incomingJSON, "id");
            //     if (cJSON_IsNumber(idJson)) {
            //         int id = idJson->valueint;
            //         // Call send_data_ack with MAC address and ID
            //         send_data_ack(recvStruct->mac_addr, id);
            //     }
            // }

            // Log MAC address and pass data to handler
            ESP_LOGI(TAG, "Received %d bytes from %s", recvStruct->len, get_mac_str(recvStruct->mac_addr));
            handler(incomingJSON, recvStruct->mac_addr);

            // Free allocated memory
            free(recvStruct->data);
            free(recvStruct);
            cJSON_Delete(incomingJSON);
        }
    }
}

static void initialize_nvs()
{
    // ESP_ERROR_CHECK(nvs_flash_erase());
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGE(TAG, "NVS partition was truncated and needs to be erased.");

        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

// static void wifi_init(void)
// {
//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());
//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));
//     ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
//     ESP_ERROR_CHECK(esp_wifi_start());
//     ESP_ERROR_CHECK(esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_LR));
//     ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));
// }
/*---------- Setup Functions ----------*/

// esp_err_t setupESPNow(messageHandler handler)
esp_err_t setupESPNow(messageHandler handler, int ch, const uint8_t sensor_macs[][ESP_NOW_ETH_ALEN], size_t sensor_mac_count)
{
    static const char *TAG = "setupESPNow";
    esp_err_t ret = ESP_OK;
    ESP_LOGW(TAG, "MESH...");
    // initialize_nvs();

    // ESP_LOGD(TAG, "Initializing tcpip adapter...");
    // ret = esp_netif_init();
    // if (ret != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "Failed to initialize tcpip adapter");
    //     return ret;
    // }

    // Confirm if this can be removed
    // ESP_LOGD(TAG, "Initializing default station...");
    // esp_netif_create_default_wifi_sta();

    // wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // ret = esp_wifi_init(&cfg);
    // if (ret != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "Failed to initialize Wi-Fi");
    //     return ret;
    // }

    // ESP_LOGD(TAG, "Setting Wi-Fi storage and mode...");
    // ret = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    // if (ret != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "Failed to set Wi-Fi storage");
    //     return ret;
    // }

    // ESP_LOGD(TAG, "Setting Wi-Fi mode...");
    // ret = esp_wifi_set_mode(WIFI_MODE_STA);
    // if (ret != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "Failed to set Wi-Fi mode");
    //     return ret;
    // }

    // ESP_LOGD(TAG, "Starting Wi-Fi...");
    // ret = esp_wifi_start();
    // if (ret != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "Failed to start Wi-Fi");
    //     return ret;
    // }
    channel = ch;
    ESP_LOGW(TAG, "Setting Channel %d", channel);
    // ESP_ERROR_CHECK(esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_LR));
    // ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));

    ESP_LOGD(TAG, "Initializing ESP-NOW...");
    ret = esp_now_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error initializing ESP-NOW");
        return ret;
    }

    ESP_LOGD(TAG, "Registering send callback for ESP-NOW...");
    ret = esp_now_register_send_cb(OnESPNowSend);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register send callback");
        return ret;
    }

    ESP_LOGD(TAG, "Creating Broadcast Peer...");
    // uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    // memcpy(broadcastPeer.peer_addr, broadcastAddress, ESP_NOW_ETH_ALEN);
    // broadcastPeer.channel = channel;
    // broadcastPeer.encrypt = false;

    // Set up the peer information
    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, broadcastAddress, ESP_NOW_ETH_ALEN);
    peer_info.channel = channel;
    peer_info.ifidx = ESPNOW_WIFI_IF;
    peer_info.encrypt = false;
    memcpy(peer_info.lmk, key, sizeof(key)); // Set the encryption key

    if (!esp_now_is_peer_exist(broadcastAddress))
    {
        ESP_LOGI(TAG, "Attempting to add new peer: %s", get_mac_str(broadcastAddress));
        if (esp_now_add_peer(&peer_info) == ESP_OK)
        {
            ESP_LOGW(TAG, "Added peer: %s at channel %d", get_mac_str(broadcastAddress), channel);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to add peer");
        }
    }
    else
    {
        ESP_LOGW(TAG, "Peer already exists");
    }

    // Add sensor mac
    // esp_now_peer_info_t peer_info = {};
    for (size_t i = 0; i < sensor_mac_count; ++i)
    {
        // esp_now_peer_info_t peer_info = {};
        memcpy(peer_info.peer_addr, sensor_macs[i], ESP_NOW_ETH_ALEN);
        peer_info.channel = ch;
        peer_info.ifidx = ESPNOW_WIFI_IF;
        peer_info.encrypt = true; // or true if you want encryption
        // Optionally set the LMK if encryption is enabled
        memcpy(peer_info.lmk, key, sizeof(key));

        if (!esp_now_is_peer_exist(sensor_macs[i]))
        {
            ESP_LOGI(TAG, "Attempting to add new peer");
            if (esp_now_add_peer(&peer_info) == ESP_OK)
            {
                ESP_LOGW(TAG, "Added peer %s at channel %d", get_mac_str(sensor_macs[i]), ch);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to add peer %s", get_mac_str(sensor_macs[i]));
            }
        }
        else
        {
            ESP_LOGW(TAG, "Peer %s already exists", get_mac_str(sensor_macs[i]));
        }
    }

    ESP_LOGD(TAG, "Registering receive callback for ESP-NOW...");
    ret = esp_now_register_recv_cb(OnESPNowRecv);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to register receive callback");
        return ret;
    }

    ESP_LOGD(TAG, "Creating incomingESPNowQueue...");
    incomingESPNowQueue = xQueueCreate(QUEUE_SIZE, sizeof(ESPNowReceiveStruct *));
    if (incomingESPNowQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create incomingESPNowQueue");
        return ESP_FAIL;
    }
    // incomingESPNowQueue = xQueueCreate(10, sizeof(cJSON *));
    // if (incomingESPNowQueue == NULL)
    // {
    //     ESP_LOGE(TAG, "Failed to create incomingESPNowQueue");
    //     return ESP_FAIL;
    // }

    ESP_LOGD(TAG, "Creating outgoingESPNowQueue...");
    outgoingESPNowQueue = xQueueCreate(QUEUE_SIZE, sizeof(ESPNowMessage));
    if (outgoingESPNowQueue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create outgoingESPNowQueue");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Starting receive task with abstract handler...");
    if (pdPASS != xTaskCreate(receiveESPNowTask, "listenESPNow_task", TASK_STACK_SIZE, handler, TASK_PRIORITY, &receiveESPNowTaskHandle))
    {
        ESP_LOGE(TAG, "Failed to create listener task");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Starting sender task...");
    if (pdPASS != xTaskCreate(sendESPNowTask, "sendESPNow_task", TASK_STACK_SIZE, NULL, TASK_PRIORITY, &sendESPNowTaskHandle))
    {
        ESP_LOGE(TAG, "Failed to create sender task");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "ESPNow setup complete");
    return ESP_OK;
}

/*---------- Messaging Functions ----------*/

esp_now_peer_info_t broadcastPeer;
TaskHandle_t receiveSerialTaskHandle, receiveESPNowTaskHandle, sendESPNowTaskHandle, sendSerialTaskHandle, serialDaemonTaskHandle;
QueueHandle_t incomingESPNowQueue, outgoingESPNowQueue, incomingSerialQueue, outgoingSerialQueue;

esp_err_t sendMessageESPNow(cJSON *body, const uint8_t *destinationMAC)
{
    static const char *TAG = "sendMessageESPNow";
    ESPNowMessage outgoingMessage;

    ESP_LOGD(TAG, "Creating ESPNowMessage...");
    outgoingMessage.bodyserialized = cJSON_PrintUnformatted(body);

    ESP_LOGD(TAG, "Copying destination MAC address to ESPNowMessage struct...");
    memcpy(outgoingMessage.destinationMAC, destinationMAC, ESP_NOW_ETH_ALEN);

    ESP_LOGD(TAG, "Posting to outgoingESPNowQueue...");
    if (xQueueSend(outgoingESPNowQueue, &outgoingMessage, 0) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to send struct to outgoingESPNowQueue queue");
        cJSON_Delete(body);
        return ESP_FAIL;
    }

    cJSON_Delete(body);
    return ESP_OK;
}

void mesh_sensor_data_handler(cJSON *incomingMessage, const uint8_t *mac_addr)
{
    // Handle incoming message
    // printf("Incoming message: %s\n", cJSON_Print(incomingMessage));

    updateLatestIncomingMessage(incomingMessage);

    // Checking if the type is 1
    cJSON *type = cJSON_GetObjectItem(incomingMessage, "type");
    if (cJSON_IsNumber(type) && type->valueint == 1)
    {
        // Extract ID from JSON
        cJSON *idJson = cJSON_GetObjectItem(incomingMessage, "id");
        if (cJSON_IsNumber(idJson))
        {
            int id = idJson->valueint;
            // Call send_data_ack with MAC address and ID
            send_data_ack(mac_addr, id);
        }
    }
    // free(incomingMessage);
}

// Function to save peer information to NVS
void save_peer_info_to_nvs(const uint8_t *mac_addr)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
        return;

    err = nvs_set_blob(nvs_handle, "root_peer", mac_addr, ESP_NOW_ETH_ALEN);
    // Check err for success...

    nvs_close(nvs_handle);
}

// Function to load peer information from NVS
esp_err_t load_peer_info_from_nvs(uint8_t *mac_addr)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
        return err;

    size_t required_size;
    err = nvs_get_blob(nvs_handle, "root_peer", NULL, &required_size);
    if (err != ESP_OK || required_size != ESP_NOW_ETH_ALEN)
    {
        nvs_close(nvs_handle);
        return ESP_ERR_NOT_FOUND;
    }

    err = nvs_get_blob(nvs_handle, "root_peer", mac_addr, &required_size);
    // Check err for success...

    nvs_close(nvs_handle);
    return ESP_OK;
}

char *get_mac_str(const uint8_t *mac_addr)
{
    static char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5]);
    return mac_str;
}

void send_data_ack(const uint8_t *mac_addr, int id)

{
    // Set this value based on your sensor type
    int type = 2;
    int ts = GetStandardTime(); // Timestamp, you might want to use `esp_timer_get_time()` or similar
    // Create JSON message
    cJSON *message = cJSON_CreateObject();
    cJSON_AddNumberToObject(message, "id", id);
    cJSON_AddNumberToObject(message, "type", type);
    cJSON_AddNumberToObject(message, "ts", ts);
    cJSON_AddNumberToObject(message, "channel", channel);
    cJSON_AddNumberToObject(message, "interval", conf.sensor_read_interval);

    // Send message to all devices
    printf("ACK message: %s\n", cJSON_Print(message));
    sendMessageESPNow(message, mac_addr);
    ESP_LOGW(TAG, "Sent ACK for %d to %s", id, get_mac_str(mac_addr));

    // free(broadcastAddress);

    // Free the cJSON object
    // cJSON_Delete(message);
}