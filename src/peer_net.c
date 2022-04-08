#include "peer_net.h"
#include "configuration.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_crc.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
//marek 7c:9e:bd:38:c5:10 7c:9e:bd:38:c5:11
//lukas 7c:9e:bd:f3:ab:d4 7c:9e:bd:f3:ab:d5
const static uint8_t mac_1[ESP_NOW_ETH_ALEN] = { 0x7c,0x9e,0xbd,0x38,0xc5,0x11 };
const static uint8_t mac_2[ESP_NOW_ETH_ALEN] = { 0x7c,0x9e,0xbd,0xf3,0xab,0xd5 };
const static int  scan_list_size = 20;

static wifi_config_t wifi_config = {
    .sta = {
        .ssid = CONFIG_ESP_WIFI_SSID,
        .password = CONFIG_ESP_WIFI_PASS,
        /* Setting a password implies station will connect to all 
            * security modes including WEP/WPA.
            * However these modes are deprecated and not advisable to be 
            * used. Incase your Access point
            * doesn't support WPA2, these mode can be enabled by commenting 
            * below line */
        .threshold.authmode = WIFI_AUTH_WPA2_PSK,

        .pmf_cfg = {
            .capable = true,
            .required = false
        },
    },
};

static int s_retry_num = 0;
static EventGroupHandle_t s_wifi_event_group;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(CONFIG_TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, CONFIG_ESP_WIFI_FAIL_BIT);
        }
        ESP_LOGI(CONFIG_TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(CONFIG_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, CONFIG_ESP_WIFI_CONNECTED_BIT);
    }
}

void add_peer(const uint8_t mac_addr[ESP_NOW_ETH_ALEN])
{
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESP_IF_WIFI_AP;
    peer->encrypt = true;
    memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
    memcpy(peer->peer_addr, mac_addr, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK( esp_now_add_peer(peer) );
    free(peer);
}

void wifi_init(void)
{
#ifdef CONFIG_IS_GATEWAY
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_APSTA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start());
#else
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    uint16_t number = scan_list_size;
    wifi_ap_record_t ap_info[scan_list_size];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_start());
    int chanelRpi = 0;
    for (int j = 0; j < 10 && chanelRpi == 0; j++)
    {
        esp_wifi_scan_start(NULL, true);
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
        ESP_LOGI(CONFIG_TAG, "Total APs scanned = %u", ap_count);
        for (int i = 0; (i < scan_list_size) && (i < ap_count); i++) {
            ESP_LOGI(CONFIG_TAG, "SSID \t\t%s", ap_info[i].ssid);
            ESP_LOGI(CONFIG_TAG, "RSSI \t\t%d", ap_info[i].rssi);
            ESP_LOGI(CONFIG_TAG, "Channel \t\t%d\n", ap_info[i].primary);
            if (strcmp((char *)ap_info[i].ssid, CONFIG_ESP_WIFI_SSID) == 0)
            {
                chanelRpi = ap_info[i].primary;
            }
        }
    }
    ESP_LOGI(CONFIG_TAG, "FOUND RPI AT CHANNEL \t\t%d\n", chanelRpi);
    ESP_ERROR_CHECK(esp_wifi_set_channel(chanelRpi, WIFI_SECOND_CHAN_NONE));
#endif
}

static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    uint8_t primary, second;
    esp_wifi_get_channel(&primary, &second);
    ESP_LOGI(CONFIG_TAG, "Sending data on chanel p: %hhi s: %hhi", primary, second);
}

static void espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len)
{
    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(CONFIG_TAG, "Receive cb arg error");
        return;
    }
    printf("Received [%d B]: %d From: "MACSTR"\n", len, *((int *)data), MAC2STR(mac_addr));
}

void espnow_init(void)
{
    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_send_cb(espnow_send_cb) );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(espnow_recv_cb) );
    ESP_ERROR_CHECK( esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK) );
}



void peer_net_task(void *pvParameter)
{
    for(uint32_t i = 0;;i++)
    {
        i = i%42;
        esp_now_send(mac_1, (uint8_t *)&i, sizeof(int));
        esp_now_send(mac_2, (uint8_t *)&i, sizeof(int));
        //esp_now_send(mac_2, (uint8_t *)&i, sizeof(int));
        printf("Sending %u\n", i);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_init();
    espnow_init();

    add_peer(mac_1);
    add_peer(mac_2);

    xTaskCreate(&peer_net_task, "peer_net_task", 2048, NULL, 5, NULL);
    for(;;)
    {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}