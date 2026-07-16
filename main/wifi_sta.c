#include "wifi_sta.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"

//输出句柄
static const char *TAG = "wifi";

static wifi_event_cb wifi_cb = NULL;

//事件回调函数
static void event_handler(void *arg,esp_event_base_t event_base,int32_t event_id,void* event_data)
{
    if(event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:           //wifi以sta模式启动后触发此事件
            esp_wifi_connect();              //启动WiFi连接
            break;
        case WIFI_EVENT_STA_CONNECTED:       //WIFI连上路由器后，触发此事件
            ESP_LOGI(TAG,"CONNECTED TO AP");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:    //wifi从路由器断开后触发此事件
            esp_wifi_connect();
            ESP_LOGI(TAG,"CONNECT TO THE AP FAIL,RETRY NOW");
            break;
        default:
            break;
        }
    }
    if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
            if(wifi_cb)
                wifi_cb(WIFI_CONNECTED);
            ESP_LOGI(TAG,"GET IP ADDRESS");
            break;
        
        default:
            break;
        }
    }
}


//WIFI sta初始化
esp_err_t wifi_sta_init(wifi_event_cb f)
{
    ESP_ERROR_CHECK(esp_netif_init());       //用于初始化协议栈
    ESP_ERROR_CHECK(esp_event_loop_create_default());   //创建系统调度循环
    esp_netif_create_default_wifi_sta();         //创建sta对象

    //初始化wifi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //注册事件
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&event_handler,NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,&event_handler,NULL));

    //WIFI配置
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = DEFAULT_WIFI_SSID,                         
            .password = DEFAULT_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,             //加密方式
            .pmf_cfg = {
                .capable = true,
                .required = false,
            },
        },
    };
    wifi_cb = f;
    
    //启动wifi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG,"wifi_init_sta finished");
    return ESP_OK;
}




