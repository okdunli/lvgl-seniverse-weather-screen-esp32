#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi_sta.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "lcd.h"
#include "lvgl.h"
#include "lcdfont.h"

#include "picture.h"
#include "lv_conf.h"
//定义主函数句柄
static const char* TAG = "main";

#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t s_wifi_ev = NULL;

//心知天气密钥
#define WEATHER_PRIVATE_KEY "SfSO4syYkYy4K8M32"

#define MAX_OUTPUT_BUFFER_LEN 1024
static char output_buffer[MAX_OUTPUT_BUFFER_LEN] = {0};
static int output_len = 0;
char *result_city_name = NULL;
char *result_weather = NULL;
char result_time[16] = {0};
//获取城市名字，放入定义的数组
static char s_city_name[128];
static char city_name[128]="maoming";
int ver=0;
#define CLIENT_HTTP_RECEIVE_BIT BIT0
static EventGroupHandle_t s_client_http_event = NULL;       

//该函数处理http客户端事件
static esp_err_t http_client_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:                             //错误事件
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:                      //连接不成功事件
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:                       //发送头事件
        ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:                         //接受头事件
        ESP_LOGI(TAG,"HTTP_EVENT_ON_HEADER");
        printf("%.*s", evt->data_len, (char*)evt->data);
        break;
    case HTTP_EVENT_ON_DATA:                           //接受数据事件
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        xEventGroupClearBits(s_client_http_event, CLIENT_HTTP_RECEIVE_BIT);
        if(output_len + evt->data_len < MAX_OUTPUT_BUFFER_LEN)
        {
            memcpy(&output_buffer[output_len], evt->data, evt->data_len);
            output_len += evt->data_len;
        }
        break;
    case HTTP_EVENT_ON_FINISH:                         //会话完成事件
        ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
        xEventGroupSetBits(s_client_http_event, CLIENT_HTTP_RECEIVE_BIT);
        output_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:                      //断开事件
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        output_len = 0;
        break;
    case HTTP_EVENT_REDIRECT:                          //重定向事件
        ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

//解析返回的天气数据    json格式
static esp_err_t parse_weather(char *weather_js)
{
    cJSON *wt_js = cJSON_Parse(weather_js); 
    if(!wt_js)
    {
        ESP_LOGE(TAG, "Error parsing weather json");
        return ESP_FAIL;
    }
    cJSON *result_js = cJSON_GetObjectItem(wt_js,"results");
    if(!result_js)
    {
        ESP_LOGE(TAG, "invaild weather result");
        return ESP_FAIL;
    }
    cJSON *result_child_js = result_js->child;
    while(result_child_js)
    {
        cJSON *location_js = cJSON_GetObjectItem(result_child_js,"location");
        cJSON *name_js = NULL;
        if(location_js)
        {
           name_js = cJSON_GetObjectItem(location_js,"name");
        }
        cJSON *now_js = cJSON_GetObjectItem(result_child_js,"now");
        cJSON *temp_js = NULL;
        cJSON *last_update_js = cJSON_GetObjectItem(result_child_js, "last_update");

if (last_update_js && cJSON_IsString(last_update_js))
{
    const char *time_str = cJSON_GetStringValue(last_update_js);
    if (time_str)
    {
        char *t_pos = strstr(time_str, "T");
        if (t_pos)
        {
           char *start = t_pos + 1;
            int i = 0;
            while (i < 16 && start[i] != '+' && start[i] != '\0') {
                result_time[i] = start[i];
                i++;
            }
            result_time[i] = '\0';
        }
    }
}
        if(now_js)
        {
            temp_js = cJSON_GetObjectItem(now_js,"temperature");
        }
        if(name_js && temp_js) 
        {
            ESP_LOGI(TAG,"city:%s,temperature:%s",cJSON_GetStringValue(name_js),cJSON_GetStringValue(temp_js));
            result_city_name = cJSON_GetStringValue(name_js);
            result_weather = cJSON_GetStringValue(temp_js);
        }  
        result_child_js = result_child_js->next;
    }
    ver=1;
    return ESP_OK;
}

//解析返回的位置数据
static esp_err_t parse_location(char *location_js)
{
   cJSON *lo_js = cJSON_Parse(location_js);
   if(!lo_js)
   {
        ESP_LOGE(TAG, "Error parsing location json");
        return ESP_FAIL;
   }
   cJSON *city_js = cJSON_GetObjectItem(lo_js,"city");
   if(!city_js)
   {
        ESP_LOGE(TAG, "Error parsing location json");
        return ESP_FAIL;
   }
   snprintf(s_city_name,sizeof(s_city_name),"%s",cJSON_GetStringValue(city_js));
   return ESP_OK;
}

//获取天气信息
static esp_err_t weather_http_init(void)
{
    esp_err_t ret_code = ESP_FAIL;
    static char weather_url[256];

    //定义http配置结构体
    esp_http_client_config_t config;
    snprintf(weather_url,sizeof(weather_url),
    "https://api.seniverse.com/v3/weather/now.json?key=%s&location=%s&language=zh-Hans&unit=c",
        WEATHER_PRIVATE_KEY,city_name);
    config.url = weather_url;
    config.skip_cert_common_name_check = true;
    config.event_handler = http_client_event_handler;
    //初始化结构体
    esp_http_client_handle_t http_client = esp_http_client_init(&config);	//初始化http连接
    if(!http_client)
    {
        ESP_LOGI(TAG,"http_client init fail!");
        return ESP_FAIL;
    }

    //设置发送请求 
    esp_http_client_set_method(http_client, HTTP_METHOD_POST);

    esp_err_t err  = esp_http_client_perform(http_client);
    if(err != ESP_OK)
    {
        return ESP_FAIL;
    }
    //获取返回的内容长度
    output_len =  esp_http_client_get_content_length(http_client);
    if(output_len > 0)
    {
        output_len = esp_http_client_read(http_client, output_buffer, MAX_OUTPUT_BUFFER_LEN);
        ret_code = parse_weather(output_buffer);
        output_len = 0;
    }
    else
    {
        //chunked块，需要从https event_handle获取数据
        EventBits_t ev = xEventGroupWaitBits(s_client_http_event,CLIENT_HTTP_RECEIVE_BIT,pdTRUE,pdFALSE,10*1000/portTICK_PERIOD_MS);
        if(ev & CLIENT_HTTP_RECEIVE_BIT)
        {
            ret_code = parse_weather(output_buffer);
            output_len = 0;
        }
    }
    return ret_code;
}

//发起http请求,获取当前的城市
static esp_err_t location_http_init(void)
{
    esp_err_t ret_code = ESP_OK;

    //定义http配置结构体
    esp_http_client_config_t config = {
        .url = "http://ip-api.com/json/?lang=en",
        .skip_cert_common_name_check = true,
        .event_handler = http_client_event_handler,
    };
    //初始化结构体
    esp_http_client_handle_t http_client = esp_http_client_init(&config);	//初始化http连接
    if(!http_client)
    {
        ESP_LOGI(TAG,"http_client init fail!");
        return ESP_FAIL;
    }

    //设置发送请求 
    esp_http_client_set_method(http_client, HTTP_METHOD_POST);

    esp_err_t err  = esp_http_client_perform(http_client);
    if(err != ESP_OK)
    {
        return ESP_FAIL;
    }

    output_len =  esp_http_client_get_content_length(http_client);
    if(output_len > 0)
    {
        output_len = esp_http_client_read(http_client, output_buffer, MAX_OUTPUT_BUFFER_LEN);
        ret_code = parse_location(output_buffer);
        output_len = 0;
    }
    else
    {
        //chunked块，需要从https event_handle获取数据
        EventBits_t ev = xEventGroupWaitBits(s_client_http_event,CLIENT_HTTP_RECEIVE_BIT,pdTRUE,pdFALSE,10*1000/portTICK_PERIOD_MS);
        if(ev & CLIENT_HTTP_RECEIVE_BIT)
        {
            ret_code = parse_location(output_buffer);
            output_len = 0;
        }
    }
    return ret_code;
}

//wifi事件通知
void wifi_event_handler(WIFI_EV_e ev)
{
    if(ev == WIFI_CONNECTED)
    {
        xEventGroupSetBits(s_wifi_ev,WIFI_CONNECTED_BIT);
    }
}


void app_main(void)
{
    //初始胡nvs
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        //执行擦除
        ESP_ERROR_CHECK(nvs_flash_erase());
        //重新尝试初始化
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    //创建一个事件组，用来监听wifi连接成功的事件
    s_wifi_ev = xEventGroupCreate();
    EventBits_t ev = 0;
    //创建一个事件组，用来通知http接受完成
    s_client_http_event = xEventGroupCreate();
    //初始化wifi
    wifi_sta_init(wifi_event_handler);

    //等待wifi连接成功，发起天气请求
    ev = xEventGroupWaitBits(s_wifi_ev,WIFI_CONNECTED_BIT,pdTRUE,pdFALSE,portMAX_DELAY);
    if(ev & WIFI_CONNECTED_BIT)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
        //首先发起位置请求，成功后再去获取天气信息
        if(location_http_init() == ESP_OK)
        {
            weather_http_init();
        }
    }

    bsp_lvgl_start();
    vTaskDelay(pdMS_TO_TICKS(2000));
    //lcd_draw_pictrue(0, 0, 240, 240, (uint16_t *)gImage_1);
        //创建一个标签
lv_obj_t *time_label = lv_label_create(lv_scr_act());
lv_obj_remove_style_all(time_label); 
lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, 0);
lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
lv_obj_set_pos(time_label, 95, 15);
lv_obj_set_style_bg_color(time_label, lv_color_hex(0x40A5F5), 0);
lv_obj_set_style_bg_opa(time_label, LV_OPA_100, 0);
lv_obj_set_style_pad_all(time_label, 0, 0);

lv_obj_t *weather_label = lv_label_create(lv_scr_act());
lv_obj_remove_style_all(weather_label); 
lv_obj_set_style_text_font(weather_label, &lv_font_montserrat_48, 0);
lv_obj_set_style_text_color(weather_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
lv_obj_set_pos(weather_label, 90, 85);
lv_obj_set_style_bg_color(weather_label, lv_color_hex(0x40A5F5), 0);
lv_obj_set_style_bg_opa(weather_label, LV_OPA_100, 0);
lv_obj_set_style_pad_all(weather_label, 0, 0);
    
lv_obj_t *city_label = lv_label_create(lv_scr_act());
lv_obj_remove_style_all(city_label); 
lv_obj_set_style_text_font(city_label, &font_custom, 0);
lv_obj_set_style_text_color(city_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
lv_obj_set_pos(city_label, 90, 195);
lv_obj_set_style_bg_color(city_label, lv_color_hex(0x40A5F5), 0);
lv_obj_set_style_bg_opa(city_label, LV_OPA_100, 0);
lv_obj_set_style_pad_all(city_label, 0, 0);


    while (1)
    {
    
    //设置标签显示文本

    if(ver == 1)
    {
    lv_label_set_text(weather_label, result_weather);
    lv_label_set_text(city_label, result_city_name);
    lv_label_set_text(time_label, result_time);
    vTaskDelay(pdMS_TO_TICKS(2000));
    ver=0;
    }
}
}