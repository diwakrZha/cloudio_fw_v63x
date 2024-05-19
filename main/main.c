
/* Includes *******************************************************************/

/* Standard includes. */
#include <string.h>

/* FreeRTOS includes. */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

/* ESP-IDF includes. */
#include <esp_err.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <sdkconfig.h>
#include "driver/gpio.h"

/*App core includes*/
#include "buzzer.h"
#include "ldo_control.h"
#include "led.h"
#include "sensors.h"
#include "uart_handler.h"
#include "ping_time.h"
#include "aws_config_handler.h"
#include "dev_wdt.h"
#include "vinit.h"
#include "dev_sleep.h"
#include "get_mesh_data.h"
#include "psm_routine.h"
#include "device_config.h"
#include "button_press.h"

/* ESP Secure Certificate Manager include. */
#include "esp_secure_cert_read.h"

/* Network transport include. */
#include "network_transport.h"

/* coreMQTT-Agent network manager include. */
#include "core_mqtt_agent_manager.h"

/* WiFi provisioning/connection handler include. */
#include "app_wifi.h"
#include "esp_pm.h"
#include "connect_spec.h"
#include "nvs_data_ops.h"

// #include "mb_uart_test.h"

// #include "app_wifi_with_homekit.h"
// #include "app_priv.h"

#define CONFIG_GRI_ENABLE_SUB_PUB_UNSUB_DEMO 1
#define CONFIG_GRI_ENABLE_TEMPERATURE_PUB_SUB_AND_LED_CONTROL_DEMO 0
#define CONFIG_GRI_ENABLE_OTA_DEMO 1
#define CONFIG_GRI_RUN_QUALIFICATION_TEST 0
#define CONFIG_GRI_OUTPUT_CERTS_KEYS 0
#define CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL 0
#define CONFIG_PM_ENABLE 1
#define CONFIG_FREERTOS_USE_TICKLESS_IDLE 1

/* Demo includes. */
#if CONFIG_GRI_ENABLE_SUB_PUB_UNSUB_DEMO
#include "shadow_pub.h"
#endif /* CONFIG_GRI_ENABLE_SHADOW_PUB */

#if CONFIG_GRI_ENABLE_OTA_DEMO
#include "ota_pal.h"
#include "ota_mqtt.h"
#endif /* CONFIG_GRI_ENABLE_OTA_DEMO */

#if CONFIG_GRI_RUN_QUALIFICATION_TEST
#include "qualification_wrapper_config.h"
#endif /* CONFIG_GRI_RUN_QUALIFICATION_TEST */

/**
 * @brief The AWS RootCA1 passed in from ./certs/root_cert_auth.pem
 */
extern const char root_cert_auth_start[] asm("_binary_root_cert_auth_crt_start");
extern const char root_cert_auth_end[] asm("_binary_root_cert_auth_crt_end");

/* Global variables ***********************************************************/

/**
 * @brief Logging tag for ESP-IDF logging functions.
 */
static const char *TAG = "main";

/**
 * @brief The global network context used to store the credentials
 * and TLS connection.
 */
static NetworkContext_t xNetworkContext;

#if CONFIG_GRI_ENABLE_OTA_DEMO

/**
 * @brief The AWS code signing certificate passed in from ./certs/aws_codesign.crt
 */
extern const char pcAwsCodeSigningCertPem[] asm("_binary_aws_codesign_crt_start");

#endif /* CONFIG_GRI_ENABLE_OTA_DEMO */

/* Static function declarations ***********************************************/

/**
 * @brief This function initializes the global network context with credentials.
 *
 * This handles retrieving and initializing the global network context with the
 * credentials it needs to establish a TLS connection.
 */
static BaseType_t prvInitializeNetworkContext(void);

/**
 * @brief This function starts all enabled demos.
 */
static void prvStartEnabledDemos(void);

#if CONFIG_GRI_RUN_QUALIFICATION_TEST
extern BaseType_t xQualificationStart(void);
#endif /* CONFIG_GRI_RUN_QUALIFICATION_TEST */

/* Static function definitions ************************************************/

static BaseType_t prvInitializeNetworkContext(void)
{
    /* This is returned by this function. */
    BaseType_t xRet = pdPASS;

    /* This is used to store the error return of ESP-IDF functions. */
    esp_err_t xEspErrRet;

    /* Verify that the MQTT endpoint and thing name have been configured by the
     * user. */
    if (strlen(MQTT_ENDPOINT) == 0)
    {
        ESP_LOGE(TAG, "Empty endpoint for MQTT broker. Set endpoint by "
                      "running idf.py menuconfig, then Golden Reference Integration -> "
                      "Endpoint for MQTT Broker to use.");
        xRet = pdFAIL;
    }

    if (strlen(device_id) == 0)
    {
        ESP_LOGE(TAG, "Empty thingname for MQTT broker. Set thing name by "
                      "running idf.py menuconfig, then Golden Reference Integration -> "
                      "Thing name.");
        xRet = pdFAIL;
    }

    /* Initialize network context. */

    xNetworkContext.pcHostname = MQTT_ENDPOINT;
    xNetworkContext.xPort = MQTT_PORT;

    /* Get the device certificate from esp_secure_crt_mgr and put into network
     * context. */
    xEspErrRet = esp_secure_cert_get_device_cert((char **)&xNetworkContext.pcClientCert,
                                                 &xNetworkContext.pcClientCertSize);

    if (xEspErrRet == ESP_OK)
    {
#if CONFIG_GRI_OUTPUT_CERTS_KEYS
        ESP_LOGI(TAG, "\nDevice Cert: \nLength: %" PRIu32 "\n%s",
                 xNetworkContext.pcClientCertSize,
                 xNetworkContext.pcClientCert);
#endif /* CONFIG_GRI_OUTPUT_CERTS_KEYS */
    }
    else
    {
        ESP_LOGE(TAG, "Error in getting device certificate. Error: %s",
                 esp_err_to_name(xEspErrRet));

        xRet = pdFAIL;
    }

    /* Putting the Root CA certificate into the network context. */
    xNetworkContext.pcServerRootCA = root_cert_auth_start;
    xNetworkContext.pcServerRootCASize = root_cert_auth_end - root_cert_auth_start;

    if (xEspErrRet == ESP_OK)
    {
#if CONFIG_GRI_OUTPUT_CERTS_KEYS
        ESP_LOGI(TAG, "\nCA Cert: \nLength: %" PRIu32 "\n%s",
                 xNetworkContext.pcServerRootCASize,
                 xNetworkContext.pcServerRootCA);
#endif /* CONFIG_GRI_OUTPUT_CERTS_KEYS */
    }
    else
    {
        ESP_LOGE(TAG, "Error in getting CA certificate. Error: %s",
                 esp_err_to_name(xEspErrRet));

        xRet = pdFAIL;
    }

#if CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
    /* If the digital signature peripheral is being used, get the digital
     * signature peripheral context from esp_secure_crt_mgr and put into
     * network context. */

    xNetworkContext.ds_data = esp_secure_cert_get_ds_ctx();

    if (xNetworkContext.ds_data == NULL)
    {
        ESP_LOGE(TAG, "Error in getting digital signature peripheral data.");
        xRet = pdFAIL;
    }
#else
    xEspErrRet = esp_secure_cert_get_priv_key((char **)&xNetworkContext.pcClientKey,
                                              &xNetworkContext.pcClientKeySize);

    if (xEspErrRet == ESP_OK)
    {
#if CONFIG_GRI_OUTPUT_CERTS_KEYS
        ESP_LOGI(TAG, "\nPrivate Key: \nLength: %" PRIu32 "\n%s",
                 xNetworkContext.pcClientKeySize,
                 xNetworkContext.pcClientKey);
#endif /* CONFIG_GRI_OUTPUT_CERTS_KEYS */
    }
    else
    {
        ESP_LOGE(TAG, "Error in getting private key. Error: %s",
                 esp_err_to_name(xEspErrRet));

        xRet = pdFAIL;
    }
#endif /* CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */

    xNetworkContext.pxTls = NULL;
    xNetworkContext.xTlsContextSemaphore = xSemaphoreCreateMutex();

    if (xNetworkContext.xTlsContextSemaphore == NULL)
    {
        ESP_LOGE(TAG, "Not enough memory to create TLS semaphore for global "
                      "network context.");

        xRet = pdFAIL;
    }

    return xRet;
}

static void prvStartEnabledDemos(void)
{
    BaseType_t xResult;

#if (CONFIG_GRI_RUN_QUALIFICATION_TEST == 0)
#if CONFIG_GRI_ENABLE_SUB_PUB_UNSUB_DEMO
    vStartShadowPub();
#endif /* CONFIG_GRI_ENABLE_SIMPLE_PUB_SUB_DEMO */

#if CONFIG_GRI_ENABLE_TEMPERATURE_PUB_SUB_AND_LED_CONTROL_DEMO
    vStart_pub_shadow_Control();
#endif /* CONFIG_GRI_ENABLE_TEMPERATURE_LED_PUB_SUB_DEMO */

#if CONFIG_GRI_ENABLE_OTA_DEMO
#if CONFIG_GRI_OUTPUT_CERTS_KEYS
    ESP_LOGI(TAG, "\nCS Cert: \nLength: %zu\n%s",
             strlen(pcAwsCodeSigningCertPem),
             pcAwsCodeSigningCertPem);
#endif /* CONFIG_GRI_OUTPUT_CERTS_KEYS */

    if (otaPal_SetCodeSigningCertificate(pcAwsCodeSigningCertPem))
    {
        vStartOTACodeSigningDemo();
    }
    else
    {
        ESP_LOGE(TAG,
                 "Failed to set the code signing certificate for the AWS OTA "
                 "library. OTA demo will not be started.");
    }
#endif /* CONFIG_GRI_ENABLE_OTA_DEMO */

    /* Initialize and start the coreMQTT-Agent network manager. This handles
     * establishing a TLS connection and MQTT connection to the MQTT broker.
     * This needs to be started before starting WiFi so it can handle WiFi
     * connection events. */
    xResult = xCoreMqttAgentManagerStart(&xNetworkContext);

    if (xResult != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to initialize and start coreMQTT-Agent network "
                      "manager.");

        configASSERT(xResult == pdPASS);
    }
#endif /* CONFIG_GRI_RUN_QUALIFICATION_TEST == 0 */

#if CONFIG_GRI_RUN_QUALIFICATION_TEST
    /* Disable some logs to avoid failure on IDT log parser. */
    esp_log_level_set("esp_ota_ops", ESP_LOG_NONE);
    esp_log_level_set("esp-tls-mbedtls", ESP_LOG_NONE);
    esp_log_level_set("AWS_OTA", ESP_LOG_NONE);

    if ((xResult = xQualificationStart()) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to start Qualfication task: errno=%d", xResult);
    }

    configASSERT(xResult == pdPASS);
#endif /* CONFIG_GRI_RUN_QUALIFICATION_TEST */
}

/* Main function definition ***************************************************/
/**
 * @brief This function serves as the main entry point of this project.
 */

void app_main(void)
{
    /*initiate LDO and components*/
    setup_wdt(90); // Set up WDT with a 60-second timeout
    feed_wdt();    // Feed the WDT
    // IDLE_DELAY(1);
    //  read_nvs_config(&conf);
    get_device_id(); // Ensure device_id is set
    ESP_LOGI(TAG, "Device ID: %s", device_id);
    ESP_LOGI(TAG, "Project Name: %s, Version: %s \n", get_project_name(), get_project_version());

    //set_wake_configs();
    set_sleep_configs(); // Set sleep configurations

    init_shadow_topics(); // Initialize the shadow topics

    disable_holds(); // Disable holds on GPIOs

    /*Switch on LDO and components*/
    ldo_init();

    // configure_led();
    led_initialize();

    // uart_initialize();
    ldo_on(); // if the LDO is off, turn it on

    //Initialize GPIO
     ESP_LOGI("main", "Initializing button...");
     if (button_init() != ESP_OK)
     {
         ESP_LOGE("main", "Failed to initialize button.");
     }

    /*Initialize LED & Buzzer if the flags are 1*/
    // ESP_LOGI(TAG, "BUZZER set %d ", get_BUZ());
    ESP_LOGI(TAG, "LED set %d ", get_LED());

    led_red_on();
    ////bz_chirp_up();
    temperature_sensor_init();
    adc_en(true);


    /* This is used to store the return of initialization functions. */
    BaseType_t xRet;

    /* This is used to store the error return of ESP-IDF functions. */
    esp_err_t xEspErrRet;

    /* Initialize global network context. */
    xRet = prvInitializeNetworkContext();

    if (xRet != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to initialize global network context.");
        return;
    }

    /* Initialize NVS partition. This needs to be done before initializing
     * WiFi. */
    xEspErrRet = nvs_flash_init();

    if ((xEspErrRet == ESP_ERR_NVS_NO_FREE_PAGES) ||
        (xEspErrRet == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        ESP_LOGE(TAG, "NVS partition was truncated and needs to be erased.");
        /* NVS partition was truncated
         * and needs to be erased */
        ESP_ERROR_CHECK(nvs_flash_erase());

        /* Retry nvs_flash_init */
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* Initialize TCP/IP */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Initialize ESP-Event library default event loop.
     * This handles WiFi and TCP/IP events and this needs to be called before
     * starting WiFi and the coreMQTT-Agent network manager. */
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Start demo tasks. This needs to be done before starting WiFi and
     * and the coreMQTT-Agent network manager so demos can
     * register their coreMQTT-Agent event handlers before events happen. */
    prvStartEnabledDemos();
    feed_wdt(); // Feed the WDT

    //     #if CONFIG_PM_ENABLE
    //     // Configure dynamic frequency scaling:
    //     // maximum and minimum frequencies are set in sdkconfig,
    //     // automatic light sleep is enabled if tickless idle support is enabled.
    //     esp_pm_config_t pm_config = {
    //             .max_freq_mhz = 160,
    //             .min_freq_mhz = 80,
    // #if CONFIG_FREERTOS_USE_TICKLESS_IDLE
    //             .light_sleep_enable = true
    // #endif
    //     };
    //     ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );
    // #endif // CONFIG_PM_ENABLE

    // ESP_LOGI(TAG, "esp_wifi_set_ps().");
    // esp_wifi_set_ps(WIFI_PS_NONE);
    // reset_wifi_credentials();   // Reset WiFi credentials

    /* Start WiFi. */
    // app_wifi_with_homekit_init();
    // app_wifi_init();
    app_wifi_start(POP_TYPE_MAC);
    // app_wifi_start();

    // esp_err_t err  = app_wifi_with_homekit_start(POP_TYPE_MAC);

    GetStandardTime(); // Get Standard time
    // bz_chirp_down();

    if (save_channel(get_wifi_connection_channel()) != ESP_OK)
        ESP_LOGE(TAG, "Failed to save channel to NVS");

    if (save_ap_mac(get_connected_ap_mac()) != ESP_OK)
        ESP_LOGE(TAG, "Failed to save AP MAC to NVS");

    if (save_ip_address(fetch_external_ip()) != ESP_OK)
        ESP_LOGE(TAG, "Failed to save IP address to NVS");

    // static int ap_channel = -1;      // Initialize to an impossible value for debugging
    // char ap_mac[18], ip_address[16]; // Adjust sizes as needed

    // get_channel(&ap_channel);
    // ESP_LOGI(TAG, "AP Channel: %d", ap_channel);
    // get_ap_mac(ap_mac, sizeof(ap_mac));
    // ESP_LOGI(TAG, "AP MAC: %s", ap_mac);
    // get_ip_address(ip_address, sizeof(ip_address));
    // ESP_LOGI(TAG, "IP Address: %s", ip_address);

    // sense_mesh();
    led_off();
    ldo_off();
}