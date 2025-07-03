idf_component_register(
    SRCS "esp32_mqtt_local.c"
    PRIV_REQUIRES 
        nvs_flash 
        esp_driver_gpio
        esp_adc
        mqtt 
        esp_wifi
        esp_event 
        esp_netif 
        bmp280
        i2cdev
        dht
    INCLUDE_DIRS "."
)
