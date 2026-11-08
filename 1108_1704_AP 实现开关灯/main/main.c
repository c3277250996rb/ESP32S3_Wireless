/**
 ******************************************************************************
 * @file        main.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-01
 * @brief       WiFi-AP 模式创建 wifi 热点实验
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ******************************************************************************
 * @attention
 * 
 * 实验平台:正点原子 ESP32-S3 开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 ******************************************************************************
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_mac.h"
#include <netdb.h>
#include "led.h"
#include "myiic.h"
#include "my_spi.h"
#include "spilcd.h"
#include "xl9555.h"
#include <stdio.h>
#include <stdbool.h>


static const char *TAG = "AP";
#define EXAMPLE_ESP_WIFI_SSID   "123"
#define EXAMPLE_ESP_WIFI_PASS   "123456789"
#define EXAMPLE_MAX_STA_CONN    5
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
static char lcd_buff[100] = {0};

static bool led_is_on;

static esp_err_t root_get_handler(httpd_req_t *req)
{
    char page[1200];

    snprintf(page, sizeof(page),
             "<!doctype html><html lang=\"zh-CN\"><head>"
             "<meta charset=\"utf-8\"><meta name=\"viewport\" "
             "content=\"width=device-width,initial-scale=1\">"
             "<title>ESP32 LED 控制</title>"
             "<style>body{font-family:sans-serif;text-align:center;"
             "margin:3em auto;max-width:28em}button{font-size:1.2em;"
             "padding:.7em 1.5em;margin:.5em}</style></head><body>"
             "<h1>ESP32 LED 控制</h1><p>LED 当前状态：%s</p>"
             "<form method=\"post\" action=\"/led/on\">"
             "<button type=\"submit\">开灯</button></form>"
             "<form method=\"post\" action=\"/led/off\">"
             "<button type=\"submit\">关灯</button></form>"
             "</body></html>",
             led_is_on ? "开启" : "关闭");
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t led_on_handler(httpd_req_t *req)
{
    led_is_on = true;
    LED0(0);
    return root_get_handler(req);
}

static esp_err_t led_off_handler(httpd_req_t *req)
{
    led_is_on = false;
    LED0(1);
    return root_get_handler(req);
}

static void start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    const httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
    };
    const httpd_uri_t led_on_uri = {
        .uri = "/led/on",
        .method = HTTP_POST,
        .handler = led_on_handler,
    };
    const httpd_uri_t led_off_uri = {
        .uri = "/led/off",
        .method = HTTP_POST,
        .handler = led_off_handler,
    };

    ESP_ERROR_CHECK(httpd_start(&server, &config));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &led_on_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &led_off_uri));
    ESP_LOGI(TAG, "Web LED control ready at http://192.168.4.1/");
}

/**
 * @brief       WIFI链接糊掉函数
 * @param       arg:传入网卡控制块
 * @param       event_base:WIFI事件
 * @param       event_id:事件ID
 * @param       event_data:事件数据
 * @retval      无
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    /* 设备连接 */
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        spilcd_fill(0,90,320,240,WHITE);
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
        sprintf(lcd_buff, "MACSTR:"MACSTR,MAC2STR(event->mac));
        spilcd_show_string(0, 90, 320, 16, 16, lcd_buff, BLUE);
        spilcd_show_string(0, 110, 320, 16, 16, "With device connection", BLUE);
    } 
    /* 设备断开 */
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
        spilcd_fill(0,90,320,320,WHITE);
        sprintf(lcd_buff, "Device disconnected:"MACSTR,MAC2STR(event->mac));
        spilcd_show_string(0, 90, 320, 16, 16, lcd_buff, BLUE);
    }
}

/**
 * @brief       WIFI初始化
 * @param       无
 * @retval      无
 */
static void wifi_init_softap(void)
{
    /* 初始化网卡 */
    ESP_ERROR_CHECK(esp_netif_init());

    /* 创建新的事件循环 */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /* 使用默认配置初始化包括netif的Wi-Fi */
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    /* 配置WIFI */
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:'%s' password:'%s'",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    
    spilcd_show_string(0, 90, 240, 16, 16, "wifi connecting......", BLUE);
}


/**
 * @brief       程序入口
 * @param       无
 * @retval      无
 */
void app_main(void)
{
    esp_err_t ret;

    ret = nvs_flash_init();     /* 初始化NVS */
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    led_init();                 /* LED初始化 */
    my_spi_init();              /* SPI初始化 */
    myiic_init();               /* IIC初始化 */  
    xl9555_init();              /* 初始化按键 */
    spilcd_init();              /* LCD屏初始化 */
 
    spilcd_show_string(0, 0, 240, 32, 32, "ESP32-S3", RED);
    spilcd_show_string(0, 40, 240, 24, 24, "WiFi AP Test", RED);
    spilcd_show_string(0, 70, 240, 16, 16, "ATOM@ALIENTEK", RED);
    wifi_init_softap();
    start_webserver();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
