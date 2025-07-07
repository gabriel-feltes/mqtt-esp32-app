#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "mqtt_client.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "bmp280.h"
#include "i2cdev.h"
#include "esp_adc/adc_oneshot.h"
#include "dht.h"

#include "board_config.h"
#include "credentials.h"

// Variáveis globais
static const char *TAG = "MQTT_APP";
static const char *TAG_BMP280 = "BMP280";
static const char *TAG_DHT11 = "DHT11";
static const char *TAG_MQ135 = "MQ135";
static const char *TAG_LIGHT_SENSOR = "LDR_SENSOR";

// Variável global para o sensor MQ-135
float R0 = 10.55;   // Resistência do sensor em ar limpo (calculada)

// Handles globais
esp_mqtt_client_handle_t client;
TaskHandle_t heartbeat_task_handle, bmp280_task_handle, dht11_task_handle, mq135_task_handle, light_sensor_task_handle;
static bool mqtt_connected = false;
bmp280_t bmp280_dev;
adc_oneshot_unit_handle_t adc1_handle;

// Função para calcular pressão ao nível do mar
static float calculate_sea_level_pressure(float pressure, float temperature, float altitude) {
    // Pressão em hPa (pressure vem em Pa, dividir por 100)
    float P = pressure * PA_TO_HPA; // Conversão de Pascal para hPa
    // Fórmula barométrica para pressão ao nível do mar
    float sea_level_pressure = P / pow(1.0 - (STANDARD_LAPSE_RATE * altitude) / (temperature + STANDARD_LAPSE_RATE * altitude + SEA_LEVEL_TEMP_K), EXPONENT);
    return sea_level_pressure;
}

/*
// Função para calibrar o sensor MQ-135 e calcular R0
static void calibrate_mq135() {
    ESP_LOGI(TAG_MQ135, "[%s] Calibrando sensor MQ-135 para obter R0...", DEVICE_ID);
    float sensor_valor = 0.0;
    float RS_air;
    int adc_reading;

    // Loop para tirar média das leituras do sensor
    for (int x = 0; x < MQ135_CALIBRATION_SAMPLES; x++) {
        if (adc_oneshot_read(adc1_handle, MQ135_ADC_CHANNEL, &adc_reading) == ESP_OK) {
            sensor_valor += adc_reading; // Acumula as leituras do ADC
        } else {
            ESP_LOGE(TAG_MQ135, "[%s] Falha ao ler ADC durante calibração", DEVICE_ID);
        }
        vTaskDelay(pdMS_TO_TICKS(MQ135_CALIBRATION_DELAY_MS)); // Pequeno delay para evitar sobrecarga
    }
    sensor_valor = sensor_valor / MQ135_CALIBRATION_SAMPLES; // Calcula a média das leituras

    ESP_LOGI(TAG_MQ135, "[%s] Média das leituras = %.2f", DEVICE_ID, sensor_valor);

    // Calcula RS em ar limpo
    if (sensor_valor > 0) {
        RS_air = (MQ135_REF_VOLTAGE * ADC_RESOLUTION_12_BITS / sensor_valor) - MQ135_REF_VOLTAGE;
    } else {
        ESP_LOGE(TAG_MQ135, "[%s] Erro: Leitura do sensor inválida na calibração!", DEVICE_ID);
        RS_air = 0;
    }

    // Calcula R0 com base na razão típica do sensor MQ-135 (3.7 para CO2 em ar limpo)
    R0 = RS_air / MQ135_RS_R0_RATIO;

    ESP_LOGI(TAG_MQ135, "[%s] Valor de RS = %.2f", DEVICE_ID, RS_air);
    ESP_LOGI(TAG_MQ135, "[%s] Valor de R0 = %.2f", DEVICE_ID, R0);
}
*/

static void heartbeat_task(void *pvParameters) {
    while (1) {
        if (mqtt_connected) {
            esp_mqtt_client_publish(client, MQTT_STATUS_TOPIC, "heartbeat", 0, 0, 0);
            ESP_LOGI(TAG, "[%s] Heartbeat enviado para %s", DEVICE_ID, MQTT_BROKER);
        }
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL));
    }
}

static void bmp280_read_task(void *pvParameters) {
    float temperature, pressure, pressure_sea_level;
    char sensor_data[BMP280_BUFFER_SIZE];
    ESP_LOGI(TAG_BMP280, "[%s] Tarefa bmp280_read_task iniciada.", DEVICE_ID);
    while (1) {
        if (mqtt_connected) {
            if (bmp280_read_float(&bmp280_dev, &temperature, &pressure, NULL) == ESP_OK) {
                // Calcular pressão ao nível do mar
                pressure_sea_level = calculate_sea_level_pressure(pressure, temperature, ALTITUDE);
                ESP_LOGI(TAG_BMP280, "[%s] Raw temperature: %.2f C, Raw pressure: %.2f Pa, Sea-level pressure: %.2f hPa", 
                         DEVICE_ID, temperature, pressure, pressure_sea_level);
                snprintf(sensor_data, sizeof(sensor_data),
                         "{\"temperature\":%.2f,\"pressure\":%.2f,\"pressure_sea_level\":%.2f}",
                         temperature, pressure * PA_TO_HPA, pressure_sea_level);
                esp_mqtt_client_publish(client, MQTT_SENSOR_BMP280_TOPIC, sensor_data, 0, 0, 0);
                ESP_LOGI(TAG_BMP280, "[%s] Dados BMP280 publicados: %s", DEVICE_ID, sensor_data);
            } else {
                ESP_LOGE(TAG_BMP280, "[%s] Falha ao ler dados do sensor BMP280", DEVICE_ID);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(BMP280_READ_INTERVAL));
    }
}

static void dht11_read_task(void *pvParameters) {
    float temperature, humidity;
    char sensor_data[DHT11_BUFFER_SIZE];
    ESP_LOGI(TAG_DHT11, "[%s] Tarefa dht11_read_task iniciada.", DEVICE_ID);
    while (1) {
        if (mqtt_connected) {
            if (dht_read_float_data(DHT11_SENSOR_TYPE, DHT11_GPIO, &humidity, &temperature) == ESP_OK) {
                snprintf(sensor_data, sizeof(sensor_data),
                         "{\"temperature\":%.1f,\"humidity\":%.1f}",
                         temperature, humidity);
                esp_mqtt_client_publish(client, MQTT_SENSOR_DHT11_TOPIC, sensor_data, 0, 0, 0);
                ESP_LOGI(TAG_DHT11, "[%s] Dados DHT11 publicados: %s", DEVICE_ID, sensor_data);
            } else {
                ESP_LOGE(TAG_DHT11, "[%s] Falha ao ler dados do sensor DHT11 no GPIO %d", DEVICE_ID, DHT11_GPIO);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(DHT11_READ_INTERVAL));
    }
}

static void mq135_read_task(void *pvParameters) {
    char sensor_data[MQ135_BUFFER_SIZE];
    int adc_reading;
    float m = MQ135_SLOPE;            // Parâmetro Slope da curva de calibração
    float b = MQ135_Y_INTERCEPT;      // Parâmetro Y-Intercept da curva de calibração
    float Rs;                         // Resistência do sensor
    float razao;                      // Razão entre Rs e R0
    float ppm_log;                    // Valor de ppm em escala linear
    float ppm;                        // Valor de ppm em escala logarítmica
    ESP_LOGI(TAG_MQ135, "[%s] Tarefa mq135_read_task iniciada.", DEVICE_ID);
    while (1) {
        if (mqtt_connected) {
            if (adc_oneshot_read(adc1_handle, MQ135_ADC_CHANNEL, &adc_reading) == ESP_OK) {
                // Cálculo da resistência do sensor (Rs)
                if (adc_reading > 0) {
                    Rs = (MQ135_REF_VOLTAGE * ADC_RESOLUTION_12_BITS / adc_reading) - MQ135_REF_VOLTAGE;
                } else {
                    ESP_LOGE(TAG_MQ135, "[%s] Erro: Leitura do sensor inválida!", DEVICE_ID);
                    Rs = 0;
                }

                // Cálculo da razão Rs/R0
                razao = Rs / R0;

                // Cálculo do valor de ppm em escala linear
                ppm_log = (log10(razao) - b) / m;

                // Conversão para escala logarítmica
                ppm = pow(10, ppm_log);

                // Publicação dos dados
                snprintf(sensor_data, sizeof(sensor_data), "{\"adc_raw\":%d,\"ppm\":%.2f}", adc_reading, ppm);
                esp_mqtt_client_publish(client, MQTT_SENSOR_MQ135_TOPIC, sensor_data, 0, 0, 0);
                ESP_LOGI(TAG_MQ135, "[%s] Dados MQ-135 publicados: %s", DEVICE_ID, sensor_data);
            } else {
                ESP_LOGE(TAG_MQ135, "[%s] Falha ao ler dados do sensor MQ-135 (ADC1 CH%d)", DEVICE_ID, MQ135_ADC_CHANNEL);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(MQ135_READ_INTERVAL));
    }
}

static void light_sensor_read_task(void *pvParameters) {
    int adc_reading;
    char sensor_data[LDR_BUFFER_SIZE];
    ESP_LOGI(TAG_LIGHT_SENSOR, "[%s] Tarefa light_sensor_read_task (com controle de LED) iniciada.", DEVICE_ID);
    while (1) {
        if (adc_oneshot_read(adc1_handle, LIGHT_SENSOR_ADC_CHANNEL, &adc_reading) == ESP_OK) {
            ESP_LOGI(TAG_LIGHT_SENSOR, "[%s] LDR ADC reading: %d", DEVICE_ID, adc_reading);
            if (mqtt_connected) {
                snprintf(sensor_data, sizeof(sensor_data), "{\"ldr_raw\":%d}", adc_reading);
                esp_mqtt_client_publish(client, MQTT_SENSOR_LDR_TOPIC, sensor_data, 0, 0, 0);
                ESP_LOGI(TAG_LIGHT_SENSOR, "[%s] Dados do LDR publicados: %s", DEVICE_ID, sensor_data);
            }

            // Customizar cores com base no nível de iluminação (0 a 4095)
            if (adc_reading <= LIGHT_SENSOR_THRESHOLD_RED) { // Baixa luminosidade: vermelho dominante
                gpio_set_level(RED_LED_GPIO, 1);   // Vermelho ligado
                gpio_set_level(GREEN_LED_GPIO, 0); // Verde apagado
                gpio_set_level(BLUE_LED_GPIO, 0);  // Azul apagado
            } else if (adc_reading <= LIGHT_SENSOR_THRESHOLD_GREEN) { // Média luminosidade: verde dominante
                gpio_set_level(RED_LED_GPIO, 0);   // Vermelho apagado
                gpio_set_level(GREEN_LED_GPIO, 1); // Verde ligado
                gpio_set_level(BLUE_LED_GPIO, 0);  // Azul apagado
            } else { // Alta luminosidade: azul dominante
                gpio_set_level(RED_LED_GPIO, 0);   // Vermelho apagado
                gpio_set_level(GREEN_LED_GPIO, 0); // Verde apagado
                gpio_set_level(BLUE_LED_GPIO, 1);  // Azul ligado
            }
        } else {
            ESP_LOGE(TAG_LIGHT_SENSOR, "[%s] Falha ao ler sensor de luminosidade (ADC1 CH%d)", DEVICE_ID, LIGHT_SENSOR_ADC_CHANNEL);
            // Apagar LEDs em caso de erro
            gpio_set_level(RED_LED_GPIO, 0);
            gpio_set_level(GREEN_LED_GPIO, 0);
            gpio_set_level(BLUE_LED_GPIO, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(LIGHT_SENSOR_READ_INTERVAL));
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "[%s] Conectado ao broker MQTT: %s", DEVICE_ID, MQTT_BROKER);
            mqtt_connected = true;
            esp_mqtt_client_publish(client, MQTT_STATUS_TOPIC, "online", 0, 1, 0);
            if (heartbeat_task_handle == NULL) xTaskCreate(heartbeat_task, "heartbeat_task", HEARTBEAT_TASK_STACK_SIZE, NULL, TASK_PRIORITY, &heartbeat_task_handle);
            if (bmp280_task_handle == NULL) xTaskCreate(bmp280_read_task, "bmp280_read_task", SENSOR_TASK_STACK_SIZE, NULL, TASK_PRIORITY, &bmp280_task_handle);
            if (dht11_task_handle == NULL) xTaskCreate(dht11_read_task, "dht11_read_task", SENSOR_TASK_STACK_SIZE, NULL, TASK_PRIORITY, &dht11_task_handle);
            if (mq135_task_handle == NULL) xTaskCreate(mq135_read_task, "mq135_read_task", SENSOR_TASK_STACK_SIZE, NULL, TASK_PRIORITY, &mq135_task_handle);
            if (light_sensor_task_handle == NULL) xTaskCreate(light_sensor_read_task, "light_sensor_read_task", SENSOR_TASK_STACK_SIZE, NULL, TASK_PRIORITY, &light_sensor_task_handle);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "[%s] Desconectado do broker MQTT", DEVICE_ID);
            mqtt_connected = false;
            if (heartbeat_task_handle != NULL) { vTaskDelete(heartbeat_task_handle); heartbeat_task_handle = NULL; }
            if (bmp280_task_handle != NULL) { vTaskDelete(bmp280_task_handle); bmp280_task_handle = NULL; }
            if (dht11_task_handle != NULL) { vTaskDelete(dht11_task_handle); dht11_task_handle = NULL; }
            if (mq135_task_handle != NULL) { vTaskDelete(mq135_task_handle); mq135_task_handle = NULL; }
            if (light_sensor_task_handle != NULL) { vTaskDelete(light_sensor_task_handle); light_sensor_task_handle = NULL; }
            break;
        default: break;
    }
}

static void wifi_event_handler_sta(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_STA_START) { esp_wifi_connect(); }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED) { ESP_LOGI(TAG, "[%s] Wi-Fi desconectado. Tentando reconectar...", DEVICE_ID); esp_wifi_connect(); }
}

static void ip_event_handler_sta(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == IP_EVENT_STA_GOT_IP) { 
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "[%s] Conectado ao Wi-Fi! IP: " IPSTR, DEVICE_ID, IP2STR(&event->ip_info.ip));
    }
}

void wifi_init_sta(void) {
    ESP_LOGI(TAG, "[%s] Iniciando Wi-Fi em modo Station...", DEVICE_ID);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_t instance_any_id, instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler_sta, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler_sta, NULL, &instance_got_ip));
    wifi_config_t wifi_config = { .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS, .threshold.authmode = WIFI_AUTH_WPA2_PSK, }, };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void init_bmp280() {
    ESP_LOGI(TAG_BMP280, "[%s] Iniciando init_bmp280()...", DEVICE_ID);
    ESP_ERROR_CHECK(i2cdev_init());
    memset(&bmp280_dev, 0, sizeof(bmp280_t));
    ESP_ERROR_CHECK(bmp280_init_desc(&bmp280_dev, BMP280_I2C_ADDR, I2C_NUM_0, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO));
    bmp280_params_t params;
    bmp280_init_default_params(&params);
    params.mode = BMP280_MODE_NORMAL;
    params.filter = BMP280_FILTER_16;
    params.oversampling_pressure = BMP280_ULTRA_HIGH_RES;
    params.oversampling_temperature = BMP280_ULTRA_HIGH_RES;
    params.standby = BMP280_STANDBY_250;
    esp_err_t init_err = bmp280_init(&bmp280_dev, &params);
    if (init_err != ESP_OK) {
        ESP_LOGE(TAG_BMP280, "[%s] Falha ao inicializar BMP280: %s", DEVICE_ID, esp_err_to_name(init_err));
    } else {
        ESP_LOGI(TAG_BMP280, "[%s] BMP280 inicializado com sucesso, Chip ID: 0x%x", DEVICE_ID, bmp280_dev.id);
        if (bmp280_dev.id != BMP280_CHIP_ID && bmp280_dev.id != BME280_CHIP_ID) {
            ESP_LOGE(TAG_BMP280, "[%s] ID do chip BMP280 inválido: 0x%x", DEVICE_ID, bmp280_dev.id);
        }
    }
}

void adc_init_all() {
    adc_oneshot_unit_init_cfg_t init_config = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc1_handle));
    adc_oneshot_chan_cfg_t chan_config = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12 };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, MQ135_ADC_CHANNEL, &chan_config));
    ESP_LOGI(TAG_MQ135, "[%s] ADC1 CH%d (MQ135) configurado.", DEVICE_ID, MQ135_ADC_CHANNEL);
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, LIGHT_SENSOR_ADC_CHANNEL, &chan_config));
    ESP_LOGI(TAG_LIGHT_SENSOR, "[%s] ADC1 CH%d (Light Sensor) configurado.", DEVICE_ID, LIGHT_SENSOR_ADC_CHANNEL);
}

void dht11_init_sensor() {
    ESP_LOGI(TAG_DHT11, "[%s] DHT11 no GPIO %d pronto para leitura.", DEVICE_ID, DHT11_GPIO);
}

void app_main(void) {
    ESP_LOGI(TAG, "[%s] Iniciando app_main...", DEVICE_ID);
    ESP_ERROR_CHECK(nvs_flash_init());
    
    // Resetando os pinos
    gpio_reset_pin(RED_LED_GPIO);
    gpio_reset_pin(GREEN_LED_GPIO);
    gpio_reset_pin(BLUE_LED_GPIO);

    // Configurando como saída
    gpio_set_direction(RED_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(GREEN_LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(BLUE_LED_GPIO, GPIO_MODE_OUTPUT);

    // Apagando todos inicialmente
    gpio_set_level(RED_LED_GPIO, 0);
    gpio_set_level(GREEN_LED_GPIO, 0);
    gpio_set_level(BLUE_LED_GPIO, 0);

    ESP_LOGI(TAG, "[%s] LED RGB inicializado nos GPIOs R:%d G:%d B:%d", DEVICE_ID, RED_LED_GPIO, GREEN_LED_GPIO, BLUE_LED_GPIO);
    
    init_bmp280(); 
    adc_init_all();
    dht11_init_sensor();
    //calibrate_mq135(); // Calibra o sensor MQ-135 ao iniciar

    wifi_init_sta();

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
        .session.keepalive = MQTT_KEEPALIVE_SECONDS,
        .session.last_will = { 
            .topic = MQTT_STATUS_TOPIC, 
            .msg = MQTT_LAST_WILL_MESSAGE, 
            .msg_len = strlen(MQTT_LAST_WILL_MESSAGE), 
            .qos = 1, 
            .retain = 1, 
        },
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    ESP_LOGI(TAG, "[%s] app_main concluída. As tarefas agora estão em execução.", DEVICE_ID);
}
