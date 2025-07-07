#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#define DEVICE_ID "esp32_02"

// --- Estrutura de Tópicos Genérica ---
#define MQTT_GPIO_COMMAND_TOPIC_PREFIX   DEVICE_ID "/gpio/"
#define MQTT_GPIO_COMMAND_TOPIC_SUFFIX   "/set"
#define MQTT_GPIO_STATE_TOPIC_FORMAT     DEVICE_ID "/gpio/%d/state"
#define MQTT_SYSTEM_STATUS_TOPIC         DEVICE_ID "/system/status"

// --- Outras Configurações ---
#define HEARTBEAT_INTERVAL_MS 5000
#define NVS_NAMESPACE "storage"
#define NVS_STATE_KEY_FORMAT "gpio_state_%d" 

#endif // BOARD_CONFIG_H
