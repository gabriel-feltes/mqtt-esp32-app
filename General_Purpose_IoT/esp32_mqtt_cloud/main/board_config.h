#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// ======================================================
// --- CONFIGURAÇÃO GERAL DO DISPOSITIVO ---
// ======================================================
#define DEVICE_ID "esp32_02"

// ======================================================
// --- CONFIGURAÇÕES DE PINOS (GPIO) ---
// ======================================================
#define LED_GPIO GPIO_NUM_2

// ======================================================
// --- CONFIGURAÇÕES DE REDE E MQTT (NÃO SENSÍVEIS) ---
// ======================================================

// Os endereços dos brokers foram movidos para credentials.h

// --- Tópicos do Cliente 1: NUVEM ---
#define MQTT_CLOUD_COMMAND_TOPIC DEVICE_ID "/gpio/gpio2/command"
#define MQTT_CLOUD_STATUS_TOPIC  DEVICE_ID "/status"

// --- Tópicos do Cliente 2: LOCAL ---
#define MQTT_LOCAL_STATE_TOPIC DEVICE_ID "/gpio/gpio2/state"

// --- Outras Configurações ---
#define HEARTBEAT_INTERVAL_MS 5000

#endif // BOARD_CONFIG_H
