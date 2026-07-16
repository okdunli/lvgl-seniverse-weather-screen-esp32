#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "wifi_ap.h"

static const char *TAG = "wifi_ap";

//wifi处理函数
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{ 
   //客户端连接AP
   if (event_id == WIFI_EVENT_AP_STACONNECTED) 
   {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "设备已连接: %02x:%02x:%02x:%02x:%02x:%02x , 客户端编号: %d",
                 event->mac[0], event->mac[1], event->mac[2], 
                 event->mac[3], event->mac[4], event->mac[5], 
                 event->aid);
   }

   //客户端断开AP
   else if(event_id == WIFI_EVENT_AP_STADISCONNECTED)
   {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "设备已断开: %02x:%02x:%02x:%02x:%02x:%02x , 客户端编号: %d",
                 event->mac[0], event->mac[1], event->mac[2], 
                 event->mac[3], event->mac[4], event->mac[5], 
                 event->aid);
   }
}

//wifi的ap模式配置
void wifi_init_softap(void)
{
    //1.初始化nvs
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "ESP wifi AP 模式启动中...");

    //2.创建WiFi驱动事件循环
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    //3.配置wifi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //4.注册wifi事件回调
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    //5.AP模式配置
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_WIFI_SSID,                       //wifi名字
            .ssid_len = strlen(ESP_WIFI_SSID),           //wifi名字长度
            .password = ESP_WIFI_PASS,                   //wifi密码
            .max_connection = MAX_STA_CONN,              //最大连接数
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,           //认证模式
            .channel = ESP_WIFI_CHANNEL,                  //wifi频道
            .pmf_cfg = {                                  //保护管理帧配置
                .capable = true,                          //是否支持
                .required = false,                        //是否需要
            },
        },
    };

    //无密码模式
    if(strlen(ESP_WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    //6.启动wifi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP 模式启动完成, SSID: %s, PASS: %s", ESP_WIFI_SSID, ESP_WIFI_PASS);
    //ESP_LOGI(TAG, "IP: %s", esp_wifi_ap_get_ip6_linklocal(WIFI_IF_AP));
}



