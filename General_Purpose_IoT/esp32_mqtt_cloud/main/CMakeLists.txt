idf_component_register(
    SRCS "esp32_mqtt_cloud.c"
    PRIV_REQUIRES 
        nvs_flash 
        esp_driver_gpio 
        mqtt 
        esp_wifi 
        esp_event 
        esp_netif 
    INCLUDE_DIRS "."
)

target_add_binary_data(${COMPONENT_TARGET} "emqxsl-ca.crt" TEXT)

