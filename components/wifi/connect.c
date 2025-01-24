#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "netdb.h"
#include "nvs_flash.h"

#define STATIC_IP_ADDR &"192.168.0.57"[0]
#define STATIC_NETMASK_ADDR &"255.255.255.0"[0]
#define STATIC_GW_ADDR &"192.168.0.1"[0]
#define STATIC_DNS_ADDR &"1.1.1.1"[0]
#define STATIC_DNS_ADDR_BACKUP &"8.8.8.8"[0]

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_MAXIMUM_RETRY 10

static char *TAG = "WIFI_CONNECT";
static int s_retry_num = 0;

static esp_netif_t *esp_netif;
static EventGroupHandle_t s_wifi_event_group;

static void event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static esp_err_t set_dns_server(esp_netif_t *netif, uint32_t addr, esp_netif_dns_type_t type);
static esp_err_t get_dns_server(esp_netif_t *netif);
static void set_static_ip(esp_netif_t *netif);
static void connect_init(void);
static esp_err_t connect_sta();

esp_err_t wifi_init() {
    connect_init();
    return (connect_sta() != ESP_OK);
}

static void event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    switch (event_id) {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
        esp_wifi_connect();
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
        set_static_ip(esp_netif);
        get_dns_server(esp_netif);
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
        break;
    case IP_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    default:
        ESP_LOGI(TAG, "SOMETHING");
        break;
    }
}

static esp_err_t set_dns_server(esp_netif_t *netif, uint32_t addr, esp_netif_dns_type_t type) {
    if (addr && (addr != IPADDR_NONE)) {
        esp_netif_dns_info_t dns;
        memset(&dns, 0, sizeof(dns));
        dns.ip.u_addr.ip4.addr = addr;
        dns.ip.type = IPADDR_TYPE_V4;
        ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, type, &dns));
    }
    return ESP_OK;
}

static esp_err_t get_dns_server(esp_netif_t *netif) {
    esp_netif_dns_info_t dns;
    memset(&dns, 0, sizeof(dns));
    esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns);
    ESP_LOGI(TAG, "Name Server: " IPSTR, IP2STR(&dns.ip.u_addr.ip4));
    // ESP_LOGI(TAG, "Name Server: %s", ip4addr_ntoa(&dns));
    // ESP_LOGI(TAG, "dns: "IPSTR, IP2STR(dns.ip.u_addr.ip4.addr));
    // for (int i = 0 ; i<4 ; i++) ESP_LOGI(TAG, "dns: " IPSTR, IP2STR(&dns.ip.u_addr.ip6));
    return ESP_OK;
}

static void set_static_ip(esp_netif_t *netif) {
    if (esp_netif_dhcpc_stop(netif) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop dhcp client");
        return;
    }
    esp_netif_ip_info_t ip;
    memset(&ip, 0, sizeof(esp_netif_ip_info_t));
    ip.ip.addr = ipaddr_addr(STATIC_IP_ADDR);
    ip.netmask.addr = ipaddr_addr(STATIC_NETMASK_ADDR);
    ip.gw.addr = ipaddr_addr(STATIC_GW_ADDR);
    if (esp_netif_set_ip_info(netif, &ip) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set ip info");
        return;
    }
    ESP_LOGD(TAG, "Success to set static ip: %s, netmask: %s, gw: %s", STATIC_IP_ADDR, STATIC_NETMASK_ADDR, STATIC_GW_ADDR);
    ESP_ERROR_CHECK(set_dns_server(netif, ipaddr_addr(STATIC_DNS_ADDR), ESP_NETIF_DNS_MAIN));
    // ESP_ERROR_CHECK(set_dns_server(netif, ipaddr_addr(STATIC_DNS_ADDR_BACKUP), ESP_NETIF_DNS_BACKUP));
}

static void connect_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    esp_netif_init();
    esp_netif_set_hostname(esp_netif, "void-esp");
    esp_event_loop_create_default();
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL);
}

static esp_err_t connect_sta() {
    s_wifi_event_group = xEventGroupCreate();
    esp_netif = esp_netif_create_default_wifi_sta();
    esp_wifi_set_mode(WIFI_MODE_STA);
    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, CONFIG_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, CONFIG_WIFI_PASSWD, sizeof(wifi_config.sta.password) - 1);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWD);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWD);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
    vEventGroupDelete(s_wifi_event_group);
    return 0;
}
