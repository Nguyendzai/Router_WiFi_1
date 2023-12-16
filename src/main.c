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
#define GPIO_BUTTON_PIN    23
#define LED_GPIO_PIN  22

static const char *TAG = "wifi softAP";

static bool is_password_set = false;
static char user_set_password[64] = {0};

// Hàm kiểm tra nút nhấn
static void button_task(void *pvParameters) {
    while (1) {
        if (gpio_get_level(GPIO_BUTTON_PIN) == 0) {  // Nút được nhấn
            ESP_LOGI(TAG, "Button pressed. Deleting NVS data...");

            // Xoá dữ liệu từ NVS
            nvs_handle_t my_nvs_handle;
            ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_nvs_handle));

            ESP_ERROR_CHECK(nvs_erase_all(my_nvs_handle));

            nvs_close(my_nvs_handle);

            ESP_LOGI(TAG, "NVS data deleted.");

            // Thực hiện reset ESP32
            ESP_LOGI(TAG, "Resetting ESP32...");
            esp_restart();
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);  // Ngủ 10ms để giảm tải CPU
    }
}


// Hàm xử lý sự kiện kết nối và ngắt kết nối Wi-Fi
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

// Hàm xử lý HTTP request cho trang chủ
    static esp_err_t root_handler(httpd_req_t *req) {
     const char *html;
     const char *ledhtml;
        if (is_password_set) {

        ledhtml = 
             "<html>"
          "  <head>"
          "    <style>"
          "      body {"
          "        font-family: 'Arial', sans-serif;"
          "        text-align: center;"
          "        background-color: #f4f4f4;"
          "        margin: 0;"
          "        padding: 0;"
          "      }"
          "      h1 {"
          "        color: #333;"
          "        margin-bottom: 20px;"
          "      }"
          "      form {"
          "        display: flex;"
          "        flex-direction: column;"
          "        align-items: center;"
          "        margin-top: 20px;"
          "      }"
          "      label {"
          "        font-size: 18px;"
          "        margin-bottom: 10px;"
          "        color: #555;"
          "      }"
          "      button {"
          "        background-color: #4caf50;"
          "        color: white;"
          "        padding: 10px 15px;"
          "        font-size: 16px;"
          "        border: none;"
          "        cursor: pointer;"
          "        border-radius: 5px;"
          "        margin-bottom: 20px;"
          "      }"
          "      button:hover {"
          "        background-color: #45a049;"
          "      }"
          "    </style>"
          "  </head>"
          "  <body>"
          "    <h1>------------------------</h1>"
          "    <form action=\"/submit\" method=\"post\">"
          "      <label for=\"data\">LED Control:</label>"
          "      <button type=\"submit\" name=\"data\" value=\"1\">LED On</button>"
          "      <button type=\"submit\" name=\"data\" value=\"11\">LED Off</button>"
          "    </form>"
          "  </body>"
          "</html>";
              httpd_resp_send(req, ledhtml, HTTPD_RESP_USE_STRLEN);
                 return ESP_OK;

    } else {
             html = "<html lang=\"en\">"
                "<head>"
                    "<meta charset=\"UTF-8\">"
                    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
                    "<title>Set Password</title>"
                    "<style>"
                        "body {"
                            "font-family: 'Arial', sans-serif;"
                            "background-color: #f4f4f4;"
                            "margin: 0;"
                            "padding: 0;"
                            "display: flex;"
                            "align-items: center;"
                            "justify-content: center;"
                            "height: 100vh;"
                        "}"
                        "form {"
                            "background-color: #fff;"
                            "padding: 20px;"
                            "border-radius: 5px;"
                            "box-shadow: 0 0 10px rgba(0, 0, 0, 0.1);"
                        "}"
                        "input[type=\"password\"] {"
                            "width: 100%;"
                            "padding: 10px;"
                            "margin-bottom: 10px;"
                            "box-sizing: border-box;"
                            "border: 1px solid #ccc;"
                            "border-radius: 4px;"
                        "}"
                        "input[type=\"submit\"] {"
                            "background-color: #4caf50;"
                            "color: #fff;"
                            "padding: 10px 15px;"
                            "border: none;"
                            "border-radius: 4px;"
                            "cursor: pointer;"
                        "}"
                        "input[type=\"submit\"]:hover {"
                            "background-color: #45a049;"
                        "}"
                    "</style>"
                "</head>"
                "<body>"
                    "<form method=\"post\" action=\"/set_password\">"
                        "<label for=\"password\">Password:</label>"
                        "<input type=\"password\" name=\"password\" id=\"password\"><br>"
                        "<input type=\"submit\" value=\"Set Password\">"
                    "</form>"
                "</body>"
                "</html>";

    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
}

// Hàm lưu mật khẩu vào NVS Flash
void save_password_to_nvs(const char* password, nvs_handle_t nvs_handle) {
    esp_err_t err = nvs_set_str(nvs_handle, "password", password);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting password in NVS");
    }
}

// Hàm đọc mật khẩu từ NVS Flash
void read_password_from_nvs(char* password, nvs_handle_t nvs_handle) {
    size_t required_size;
    esp_err_t err = nvs_get_str(nvs_handle, "password", NULL, &required_size);
    if (err == ESP_OK) {
        if (required_size <= sizeof(user_set_password)) {
            err = nvs_get_str(nvs_handle, "password", password, &required_size);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Error reading password from NVS");
            }
        } else {
            ESP_LOGE(TAG, "Password size mismatch in NVS");
        }
    }
}

// Hàm lưu sự kiện is_password_set vào NVS Flash
void save_is_password_set_to_nvs(bool is_set, nvs_handle_t nvs_handle) {
    esp_err_t err = nvs_set_u8(nvs_handle, "is_password_set", is_set);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting is_password_set in NVS");
    }
}

// Hàm đọc sự kiện is_password_set từ NVS Flash
void read_is_password_set_from_nvs(bool* is_set, nvs_handle_t nvs_handle) {
    esp_err_t err = nvs_get_u8(nvs_handle, "is_password_set", (uint8_t*)is_set);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error reading is_password_set from NVS");
    }
}

// Hàm kiểm tra và xử lý set password
static esp_err_t set_password_handler(httpd_req_t *req) {
    char buf[64];
    char pass[64];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    buf[ret] = '\0';
    strcpy(pass, buf + 9);

    int long_pass = strlen(pass);

    if (long_pass > 0) {
        ESP_LOGI(TAG, "Received password: %s", pass);
        if (!is_password_set) {
            nvs_handle_t my_nvs_handle;  // Tạo một biến nvs_handle mới
            ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_nvs_handle));

            // Lưu mật khẩu vào NVS chỉ nếu nó chưa được đặt
            strncpy(user_set_password, pass, sizeof(user_set_password));
            is_password_set = true;
            ESP_LOGI(TAG, "is_password_set is now true");
            
            // Lưu mật khẩu vào NVS Flash
            save_password_to_nvs(user_set_password, my_nvs_handle);
            
            // Lưu sự kiện is_password_set vào NVS Flash
            save_is_password_set_to_nvs(is_password_set, my_nvs_handle);

            // Đóng NVS Handle
            nvs_close(my_nvs_handle);

            // Khởi động lại tự động sau khi đặt mật khẩu
            esp_restart();
        }
    }
    const char *resp = "Password has been set successfully!";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Hàm xử lý HTTP request để bật/tắt LED
static esp_err_t submit_handler(httpd_req_t *req) {
    char content[10];
    int recv_len = httpd_req_recv(req, content, sizeof(content));

        content[recv_len] = '\0';
       // ESP_LOGI(TAG, "Received LED control command: %s", content);

        // Đọc giá trị từ dữ liệu nhận được
        int control_value = strlen(content);

        // Kiểm tra giá trị và bật/tắt LED tương ứng
        if (control_value == 6) {
            gpio_set_level(LED_GPIO_PIN, 1);  // Bật LED
          //  ESP_LOGI(TAG, "LED GPIO state after setting: %d", gpio_get_level(LED_GPIO_PIN));

        } else if (control_value == 7) {
            gpio_set_level(LED_GPIO_PIN, 0);  // Tắt LED
          //  ESP_LOGI(TAG, "LED GPIO state after setting: %d", gpio_get_level(LED_GPIO_PIN));
        }

        const char *resp_str = "LED control successful!";
        httpd_resp_send(req, resp_str, strlen(resp_str));

    return ESP_OK;
}

// Ca hinh root URI handler
static httpd_uri_t root_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_handler,
    .user_ctx  = NULL
};


// Cau hinh set_password URI handler
static httpd_uri_t set_password_uri = {
    .uri       = "/set_password",
    .method    = HTTP_POST,
    .handler   = set_password_handler,
    .user_ctx  = NULL
};
//Cau hinh turn led
static httpd_uri_t submit_uri = {
    .uri      = "/submit",
    .method   = HTTP_POST,
    .handler  = submit_handler,
    .user_ctx = NULL
};

// Cau hinh HTTP server
static httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();

// Hàm cấu hình Wi-Fi access point
static void wifi_init_softap(nvs_handle_t nvs_handle) {
    // Khoi tao WiFi voi cau hinh mac dinh
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

    // Đọc giá trị của is_password_set từ NVS Flash
    read_is_password_set_from_nvs(&is_password_set, nvs_handle);

    if (is_password_set) {
        // Đọc mật khẩu từ NVS Flash
        read_password_from_nvs(user_set_password, nvs_handle);

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
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &submit_uri));

    ESP_LOGI(TAG, "After Wi-Fi config: is_password_set=%s", is_password_set ? "true" : "false");
    ESP_LOGI(TAG, "wifi_init_softap completed. SSID:%s password:%s channel:%d",
             ESP_WIFI_SSID, wifi_config.ap.password, ESP_WIFI_CHANNEL);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    nvs_handle_t my_nvs_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_nvs_handle));

    // Cấu hình GPIO cho nút nhấn
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GPIO_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT, // kieu nhan tin hieu
        .intr_type = GPIO_PIN_INTR_DISABLE,  // Chế độ ngắt không được sử dụng
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Tạo một task để theo dõi trạng thái của nút nhấn
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);
    // Khoi tao  GPIO cho LED
    gpio_set_direction(LED_GPIO_PIN, GPIO_MODE_OUTPUT);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
    wifi_init_softap(my_nvs_handle);
}
