#ifndef __WIFI_STA_H
#define __WIFI_STA_H

#include "esp_err.h"

//存放wifi名字和wifi密码
#define DEFAULT_WIFI_SSID       "Hi90"                         //wifi名字
#define DEFAULT_WIFI_PASSWORD   "10241024"                    //wifi密码


//枚举类型用来表示wifi状态
typedef enum{
    WIFI_DISCONNECTED,            //wifi断开
    WIFI_CONNECTED,               //wifi已连接
}WIFI_EV_e;

typedef void(*wifi_event_cb)(WIFI_EV_e);

//函数声明
esp_err_t wifi_sta_init(wifi_event_cb f);


#endif

