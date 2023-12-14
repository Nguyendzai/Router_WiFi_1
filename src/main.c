#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "driver/gpio.h"
#include "esp_http_server.h"

#define ESP_WIFI_SSID      "WiFi_Protocol"
#define ESP_WIFI_CHANNEL   1
#define MAX_STA_CONN       10

static const char *TAG = "wifi softAP";

static bool is_password_set = false;
static char user_set_password[64] = {0};

// Event handler for Wi-Fi events
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Device "MACSTR" connected, AID=%d",
                 MAC2STR(event->mac), event->aid);

        if (!is_password_set) {
            is_password_set = false;
            ESP_LOGI(TAG, "Requesting user to set a password...");
        }
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Device "MACSTR" disconnected, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

// HTTP handler for root page
static esp_err_t root_handler(httpd_req_t *req) {
    const char *html;
    if (is_password_set) {
        html = "<html><body>Password has been set!</body></html>";
    } else {
        html = "<html><body>"
               "<form method=\"post\" action=\"/set_password\">"
               "Password: <input type=\"password\" name=\"password\"><br>"
               "<input type=\"submit\" value=\"Set Password\">"
               "</form>"
               "</body></html>";
    }

    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// HTTP handler for setting the password
static esp_err_t set_password_handler(httpd_req_t *req) {
    char buf[64];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    buf[ret] = '\0';

    int long_pass = strlen(buf);

    if (long_pass > 12) {             //true 9
        ESP_LOGI(TAG, "Received password: %s", buf);
        strncpy(user_set_password, buf, sizeof(user_set_password));
        is_password_set = true;
        ESP_LOGI(TAG, "is_password_set is now true");

    }

    const char *resp = "Password has been set successfully!";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Configuration for the root URI handler
static httpd_uri_t root_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_handler,
    .user_ctx  = NULL
};

// Configuration for the set_password URI handler
static httpd_uri_t set_password_uri = {
    .uri       = "/set_password",
    .method    = HTTP_POST,
    .handler   = set_password_handler,
    .user_ctx  = NULL
};

// Configuration for the HTTP server
static httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();

// Initialization of the Wi-Fi access point and HTTP server
static void wifi_init_softap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_WIFI_SSID,
            .ssid_len = strlen(ESP_WIFI_SSID),
            .channel = ESP_WIFI_CHANNEL,
            .max_connection = MAX_STA_CONN,
        },
    };

    ESP_LOGI(TAG, "Before Wi-Fi config: is_password_set=%s", is_password_set ? "true" : "false");

    if (is_password_set) {
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
        strncpy((char*)wifi_config.ap.password, user_set_password, sizeof(wifi_config.ap.password));
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        wifi_config.ap.password[0] = '\0';
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    httpd_handle_t server;
    ESP_ERROR_CHECK(httpd_start(&server, &httpd_config));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_uri));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &set_password_uri));

    ESP_LOGI(TAG, "After Wi-Fi config: is_password_set=%s", is_password_set ? "true" : "false");
    ESP_LOGI(TAG, "wifi_init_softap completed. SSID:%s password:%s channel:%d",
             ESP_WIFI_SSID, wifi_config.ap.password, ESP_WIFI_CHANNEL);
}

// Main function (app_main)
void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap();
}
