#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "mqtt_client.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "credentials.h"
#include "board_config.h"

// --- Constantes e Variáveis Globais ---
static const char *TAG = "GENERIC_MQTT_APP";

esp_mqtt_client_handle_t cloud_client = NULL;
esp_mqtt_client_handle_t local_client = NULL;
TaskHandle_t heartbeat_task_handle = NULL;
static bool cloud_mqtt_connected = false;

// Referência ao certificado da nuvem
extern const uint8_t emqxsl_ca_crt_start[] asm("_binary_emqxsl_ca_crt_start");
extern const uint8_t emqxsl_ca_crt_end[]   asm("_binary_emqxsl_ca_crt_end");

// --- Funções de Persistência (NVS) para Múltiplos Pinos ---

static void save_gpio_state(int gpio_num, uint8_t state) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao abrir o NVS para escrita (%s)", esp_err_to_name(err));
        return;
    }
    char key[20];
    snprintf(key, sizeof(key), NVS_STATE_KEY_FORMAT, gpio_num);
    err = nvs_set_u8(nvs_handle, key, state);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao salvar estado para GPIO %d no NVS (%s)", gpio_num, esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Estado do GPIO %d (%s) salvo no NVS.", gpio_num, state == 1 ? "ON" : "OFF");
    }
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}

static uint8_t load_gpio_state(int gpio_num) {
    nvs_handle_t nvs_handle;
    uint8_t state = 0; // Estado padrão: OFF
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao abrir o NVS para leitura (%s)", esp_err_to_name(err));
        return state;
    }
    char key[20];
    snprintf(key, sizeof(key), NVS_STATE_KEY_FORMAT, gpio_num);
    err = nvs_get_u8(nvs_handle, key, &state);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Erro ao ler NVS para GPIO %d (%s)", gpio_num, esp_err_to_name(err));
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Estado não encontrado para GPIO %d, usando padrão (%s)", gpio_num, state ? "ON" : "OFF");
    }
    nvs_close(nvs_handle);
    return state;
}

// --- Funções de Controle e Publicação ---

void update_and_publish_state(int gpio_num, uint8_t new_state) {
    if (!GPIO_IS_VALID_OUTPUT_GPIO(gpio_num)) {
        ESP_LOGE(TAG, "GPIO %d não é um pino de saída válido.", gpio_num);
        return;
    }

    gpio_reset_pin(gpio_num);
    gpio_set_direction(gpio_num, GPIO_MODE_OUTPUT);
    gpio_set_level(gpio_num, new_state);
    ESP_LOGI(TAG, "GPIO %d set to %s", gpio_num, new_state ? "ON" : "OFF");

    save_gpio_state(gpio_num, new_state);

    char state_topic[64];
    snprintf(state_topic, sizeof(state_topic), MQTT_GPIO_STATE_TOPIC_FORMAT, gpio_num);
    const char* state_str = (new_state == 1) ? "ON" : "OFF";
    int msg_id;

    ESP_LOGI(TAG, "Tentando publicar estado '%s' para GPIO %d em '%s'", state_str, gpio_num, state_topic);

    if (cloud_client && cloud_mqtt_connected) {
        msg_id = esp_mqtt_client_publish(cloud_client, state_topic, state_str, 0, 1, 1);
        if (msg_id != -1) {
            ESP_LOGI(TAG, "Estado publicado para o broker da NUVEM, msg_id=%d", msg_id);
        } else {
            ESP_LOGE(TAG, "FALHA ao publicar estado para o broker da NUVEM.");
        }
    }
    
    if (local_client) {
        msg_id = esp_mqtt_client_publish(local_client, state_topic, state_str, 0, 1, 1);
        if (msg_id != -1) {
            ESP_LOGI(TAG, "Estado publicado para o broker LOCAL, msg_id=%d", msg_id);
        } else {
            ESP_LOGE(TAG, "FALHA ao publicar estado para o broker LOCAL.");
        }
    }
}

// --- Tarefa de Heartbeat ---
static void heartbeat_task(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
        if (local_client) {
            int msg_id = esp_mqtt_client_publish(local_client, MQTT_SYSTEM_STATUS_TOPIC, "heartbeat", 0, 0, 0);
            if (msg_id != -1) {
                ESP_LOGI(TAG, "Heartbeat publicado para o broker LOCAL, msg_id=%d", msg_id);
            } else {
                ESP_LOGE(TAG, "FALHA ao publicar heartbeat para o broker LOCAL.");
            }
        }
    }
}

// --- Manipulador de Eventos para o Cliente LOCAL ---
static void local_mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Cliente LOCAL conectado.");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Cliente LOCAL desconectado.");
            break;
        default:
            break;
    }
}

// --- Manipulador de Eventos para o Cliente da NUVEM ---
static void cloud_mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Cliente da NUVEM conectado.");
            cloud_mqtt_connected = true;
            esp_mqtt_client_publish(cloud_client, MQTT_SYSTEM_STATUS_TOPIC, "online", 0, 1, 1);
            char command_topic_wildcard[64];
            snprintf(command_topic_wildcard, sizeof(command_topic_wildcard), "%s+%s", MQTT_GPIO_COMMAND_TOPIC_PREFIX, MQTT_GPIO_COMMAND_TOPIC_SUFFIX);
            esp_mqtt_client_subscribe(cloud_client, command_topic_wildcard, 1);
            ESP_LOGI(TAG, "Inscrito em: %s", command_topic_wildcard);
            if (heartbeat_task_handle == NULL) {
                xTaskCreate(heartbeat_task, "heartbeat_task", 3072, NULL, 5, &heartbeat_task_handle);
            }
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Cliente da NUVEM desconectado.");
            cloud_mqtt_connected = false;
            if (heartbeat_task_handle != NULL) {
                vTaskDelete(heartbeat_task_handle);
                heartbeat_task_handle = NULL;
            }
            break;

        case MQTT_EVENT_DATA:
            if (strncmp(event->topic, MQTT_GPIO_COMMAND_TOPIC_PREFIX, strlen(MQTT_GPIO_COMMAND_TOPIC_PREFIX)) == 0) {
                int pin_number;
                int items = sscanf(event->topic, MQTT_GPIO_COMMAND_TOPIC_PREFIX "%d" MQTT_GPIO_COMMAND_TOPIC_SUFFIX, &pin_number);
                if (items == 1) {
                    ESP_LOGI(TAG, "Comando recebido para o pino: %d", pin_number);
                    if (!GPIO_IS_VALID_OUTPUT_GPIO(pin_number)) {
                        ESP_LOGE(TAG, "GPIO %d não é um pino de saída válido.", pin_number);
                        return;
                    }
                    uint8_t current_state = gpio_get_level(pin_number);
                    uint8_t new_state = current_state;
                    if (strncmp(event->data, "ON", event->data_len) == 0) {
                        new_state = 1;
                    } else if (strncmp(event->data, "OFF", event->data_len) == 0) {
                        new_state = 0;
                    } else if (strncmp(event->data, "TOGGLE", event->data_len) == 0) {
                        new_state = !current_state;
                    } else {
                        ESP_LOGW(TAG, "Comando desconhecido: %.*s", event->data_len, event->data);
                        return;
                    }
                    update_and_publish_state(pin_number, new_state);
                }
            }
            break;
            
        default:
            break;
    }
}

// --- Wifi Event Handler ---
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi desconectado. Tentando reconectar...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "WiFi Conectado! Iniciando clientes MQTT...");
        
        if (cloud_client == NULL) {
            esp_mqtt_client_config_t cloud_mqtt_cfg = {
                .broker.address.uri = MQTT_CLOUD_BROKER,
                .broker.verification.certificate = (const char *)emqxsl_ca_crt_start,
                .credentials = {
                    .username = MQTT_USER,
                    .authentication.password = MQTT_PASS
                },
                .session.last_will = {
                    .topic = MQTT_SYSTEM_STATUS_TOPIC,
                    .msg = "offline",
                    .qos = 1,
                    .retain = 1
                },
            };
            cloud_client = esp_mqtt_client_init(&cloud_mqtt_cfg);
            esp_mqtt_client_register_event(cloud_client, ESP_EVENT_ANY_ID, cloud_mqtt_event_handler, NULL);
        }
        esp_mqtt_client_start(cloud_client);

        if (local_client == NULL) {
            esp_mqtt_client_config_t local_mqtt_cfg = {
                .broker.address.uri = MQTT_LOCAL_BROKER,
                .credentials = {
                    .username = MQTT_USER,
                    .authentication.password = MQTT_PASS,
                },
            };
            local_client = esp_mqtt_client_init(&local_mqtt_cfg);
            esp_mqtt_client_register_event(local_client, ESP_EVENT_ANY_ID, local_mqtt_event_handler, NULL);
        }
        esp_mqtt_client_start(local_client);
    }
}

// --- Inicialização do Wi-Fi ---
void wifi_init_sta(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id, instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// --- Função Principal ---
void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Lista de pinos para inicializar
    int pins[] = {2, 4, 5, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33}; // GPIOs
    int num_pins = sizeof(pins) / sizeof(pins[0]);

    // Inicializa cada pino
    for (int i = 0; i < num_pins; i++) {
        int pin = pins[i];
        if (!GPIO_IS_VALID_OUTPUT_GPIO(pin)) {
            ESP_LOGE(TAG, "GPIO %d não é um pino de saída válido.", pin);
            continue;
        }
        uint8_t initial_state = load_gpio_state(pin);
        gpio_reset_pin(pin);
        gpio_set_direction(pin, GPIO_MODE_OUTPUT);
        gpio_set_level(pin, initial_state);
        ESP_LOGI(TAG, "Estado inicial do GPIO %d definido para %s a partir do NVS.", pin, initial_state ? "ON" : "OFF");
    }

    wifi_init_sta();
}
