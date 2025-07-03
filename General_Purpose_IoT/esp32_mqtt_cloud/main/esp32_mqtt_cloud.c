#include <stdio.h>
#include <string.h>
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

#include "board_config.h"
#include "credentials.h"

// --- Constantes e Variáveis Globais ---
static const char *TAG = "DUAL_MQTT_APP";
#define NVS_NAMESPACE "storage"
#define NVS_STATE_KEY "gpio_state"

esp_mqtt_client_handle_t cloud_client = NULL;
esp_mqtt_client_handle_t local_client = NULL;
TaskHandle_t heartbeat_task_handle = NULL;
static bool cloud_mqtt_connected = false;
static uint8_t g_gpio_state = 0; // Estado global do GPIO (0=OFF, 1=ON)

// Referência ao certificado da nuvem
extern const uint8_t emqxsl_ca_crt_start[] asm("_binary_emqxsl_ca_crt_start");
extern const uint8_t emqxsl_ca_crt_end[]   asm("_binary_emqxsl_ca_crt_end");

// --- Funções de Persistência (NVS) ---

/**
 * @brief Salva o estado do GPIO no NVS.
 * @param state O estado a ser salvo (0 para OFF, 1 para ON).
 */
static void save_gpio_state(uint8_t state) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao abrir o NVS para escrita (%s)", esp_err_to_name(err));
        return;
    }

    err = nvs_set_u8(nvs_handle, NVS_STATE_KEY, state);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao salvar estado no NVS (%s)", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Estado do GPIO (%s) salvo no NVS.", state == 1 ? "ON" : "OFF");
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao comitar NVS (%s)", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    g_gpio_state = state; // Atualiza a variável global
}

/**
 * @brief Carrega o estado do GPIO do NVS.
 * @return O estado carregado (0 para OFF, 1 para ON). Retorna 0 (OFF) se não houver estado salvo.
 */
static uint8_t load_gpio_state(void) {
    nvs_handle_t nvs_handle;
    uint8_t state = 0; // Padrão é OFF

    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao abrir o NVS para leitura (%s)", esp_err_to_name(err));
        return state; // Retorna o padrão
    }

    err = nvs_get_u8(nvs_handle, NVS_STATE_KEY, &state);
    switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "Estado anterior carregado do NVS: %s", state == 1 ? "ON" : "OFF");
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGI(TAG, "Nenhum estado anterior encontrado no NVS. Usando padrão (OFF).");
            // O estado já é 0 (padrão), então não fazemos nada.
            break;
        default:
            ESP_LOGE(TAG, "Erro ao ler o NVS (%s)", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    g_gpio_state = state; // Atualiza a variável global
    return state;
}

// --- Tarefa de Heartbeat (só publica na NUVEM) ---
static void heartbeat_task(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
        if (cloud_mqtt_connected && cloud_client) {
            esp_mqtt_client_publish(cloud_client, MQTT_CLOUD_STATUS_TOPIC, "heartbeat", 0, 0, 0);
            ESP_LOGI(TAG, "Heartbeat enviado para a NUVEM.");
        }
    }
}

// --- Manipulador de Eventos para o Cliente LOCAL ---
static void local_mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Cliente LOCAL conectado.");
            // Publica o estado ATUAL do GPIO ao se conectar
            const char* current_state_str = (g_gpio_state == 1) ? "ON" : "OFF";
            esp_mqtt_client_publish(local_client, MQTT_LOCAL_STATE_TOPIC, current_state_str, 0, 1, 1);
            ESP_LOGI(TAG, "Publicado estado inicial para o broker LOCAL: %s", current_state_str);
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
            ESP_LOGI(TAG, "Cliente da NUVEM conectado. Inscrevendo-se em: %s", MQTT_CLOUD_COMMAND_TOPIC);
            cloud_mqtt_connected = true;
            esp_mqtt_client_publish(cloud_client, MQTT_CLOUD_STATUS_TOPIC, "online", 0, 1, 1);
            esp_mqtt_client_subscribe(cloud_client, MQTT_CLOUD_COMMAND_TOPIC, 0);
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
            if (strncmp(event->topic, MQTT_CLOUD_COMMAND_TOPIC, event->topic_len) == 0) {
                const char* new_state_str = NULL;
                if (strncmp(event->data, "ON", event->data_len) == 0) {
                    gpio_set_level(LED_GPIO, 1);
                    save_gpio_state(1); // <-- SALVA O NOVO ESTADO
                    new_state_str = "ON";
                } else if (strncmp(event->data, "OFF", event->data_len) == 0) {
                    gpio_set_level(LED_GPIO, 0);
                    save_gpio_state(0); // <-- SALVA O NOVO ESTADO
                    new_state_str = "OFF";
                }
                if (new_state_str && local_client) {
                    ESP_LOGI(TAG, "Comando '%s' da NUVEM recebido. Reportando para o broker LOCAL.", new_state_str);
                    esp_mqtt_client_publish(local_client, MQTT_LOCAL_STATE_TOPIC, new_state_str, 0, 1, 1);
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
                    .topic = MQTT_CLOUD_STATUS_TOPIC,
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
    // Inicializa o NVS (necessário para WiFi e para nossa lógica)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Configura o pino GPIO
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    
    // Carrega o último estado do NVS e o aplica ao pino
    uint8_t initial_state = load_gpio_state();
    gpio_set_level(LED_GPIO, initial_state);

    // Inicia a conexão Wi-Fi (que por sua vez iniciará o MQTT)
    wifi_init_sta();
}
