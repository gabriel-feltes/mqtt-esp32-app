#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// ======================================================
// --- CONFIGURAÇÃO GERAL DO DISPOSITIVO ---
// ======================================================
#define DEVICE_ID "esp32_01"

// ======================================================
// --- CONFIGURAÇÕES DE REDE ---
// ======================================================
#define MQTT_KEEPALIVE_SECONDS 10
#define MQTT_LAST_WILL_MESSAGE "offline"

// Tópicos MQTT (prefixados com DEVICE_ID)
#define MQTT_STATUS_TOPIC         DEVICE_ID "/status"
#define MQTT_SENSOR_BMP280_TOPIC  DEVICE_ID "/sensor/bmp280"
#define MQTT_SENSOR_DHT11_TOPIC   DEVICE_ID "/sensor/dht11"
#define MQTT_SENSOR_MQ135_TOPIC   DEVICE_ID "/sensor/mq135"
#define MQTT_SENSOR_LDR_TOPIC     DEVICE_ID "/sensor/ldr"

// ======================================================
// --- CONFIGURAÇÕES DE PINOS (GPIO) E SENSORES ---
// ======================================================
#define RED_LED_GPIO GPIO_NUM_32
#define GREEN_LED_GPIO GPIO_NUM_26
#define BLUE_LED_GPIO GPIO_NUM_25
#define DHT11_GPIO GPIO_NUM_23
#define DHT11_SENSOR_TYPE DHT_TYPE_DHT11
#define MQ135_ADC_CHANNEL ADC_CHANNEL_6
#define LIGHT_SENSOR_ADC_CHANNEL ADC_CHANNEL_5
#define LIGHT_SENSOR_THRESHOLD_RED 1365
#define LIGHT_SENSOR_THRESHOLD_GREEN 2730
#define ADC_RESOLUTION_12_BITS 4095.0  // Resolução ADC (12 bits)
#define ADC_RESOLUTION_11_BITS 2047.0  // Resolução ADC (11 bits)
#define I2C_MASTER_SCL_IO GPIO_NUM_22
#define I2C_MASTER_SDA_IO GPIO_NUM_21
#define I2C_MASTER_FREQ_HZ 100000
#define BMP280_I2C_ADDR BMP280_I2C_ADDRESS_0
#define I2C_PORT I2C_NUM_0
#define ADC_UNIT ADC_UNIT_1
#define ADC_ATTEN ADC_ATTEN_DB_12
#define ADC_BITWIDTH ADC_BITWIDTH_12
#define ALTITUDE 27.0f  // Altitude do local em metros

// ======================================================
// --- CONFIGURAÇÕES DO SENSOR MQ-135 ---
// ======================================================
#define MQ135_CALIBRATION_SAMPLES 500
#define MQ135_CALIBRATION_DELAY_MS 10
#define MQ135_REF_VOLTAGE 10.0
#define MQ135_RS_R0_RATIO 3.7
#define MQ135_SLOPE -0.3376
#define MQ135_Y_INTERCEPT 0.7165

// ======================================================
// --- CONFIGURAÇÕES DO BMP280 ---
// ======================================================
#define BMP280_MODE BMP280_MODE_NORMAL
#define BMP280_FILTER BMP280_FILTER_16
#define BMP280_OVERSAMPLING BMP280_ULTRA_HIGH_RES
#define BMP280_STANDBY BMP280_STANDBY_250
#define PA_TO_HPA 0.01f  // Conversão de Pascal para hPa
#define STANDARD_LAPSE_RATE 0.0065 // K/m
#define SEA_LEVEL_TEMP_K 273.15 // K
#define EXPONENT 5.257  // Exponente para cálculo da pressão

// ======================================================
// --- CONFIGURAÇÕES DE BUFFER ---
// ======================================================
#define BMP280_BUFFER_SIZE 256
#define DHT11_BUFFER_SIZE 128
#define MQ135_BUFFER_SIZE 128
#define LDR_BUFFER_SIZE 64

// ======================================================
// --- CONFIGURAÇÕES DE TAREFAS ---
// ======================================================
#define HEARTBEAT_TASK_STACK_SIZE 3072
#define SENSOR_TASK_STACK_SIZE 4096
#define TASK_PRIORITY 5
#define HEARTBEAT_INTERVAL 20000
#define BMP280_READ_INTERVAL 2000
#define DHT11_READ_INTERVAL 2000
#define MQ135_READ_INTERVAL 2000
#define LIGHT_SENSOR_READ_INTERVAL 2000

#endif // BOARD_CONFIG_H