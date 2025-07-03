# Como proceder com os códigos a serem executados no ESP32 WROOM e Raspberry Pi 3 Model B?

Cada dispositivo possui um código específico que deve ser executado. Os códigos estão organizados em pastas dentro do repositório, facilitando a identificação e o uso de cada um deles.

## Pasta ![`esp32_mqtt_cloud`](esp32_mqtt_cloud)

Contém o código para o ESP32 WROOM identificado como `ESP32_02` na topologia de rede. O código mostra como é possível comandar o GPIO2 do ESP32 WROOM através de um broker MQTT na nuvem, utilizando o EMQX Cloud. O código é escrito em C e utiliza a biblioteca ESP-IDF para interagir com o hardware do ESP32.

> [!NOTE]
> Para executar o código do ESP32, é necessário ter o ambiente de desenvolvimento configurado com o ESP-IDF. Isso foi mostrado no tutorial de configuração do ambiente de desenvolvimento para o ESP32. Para ativar o ambiente fora da pasta `$HOME/esp32`, você pode usar o comando `source $HOME/esp32/esp-idf/export.sh` no terminal.
> Adicione seu `emqxsl-ca.crt` na pasta `esp32_mqtt_cloud/main`!
> Adicione seu `credentials.h` na pasta `esp32_mqtt_cloud/main`, cujas variáveis devem ser preenchidas para o código funcionar corretamente.

```c
#ifndef CREDENTIALS_H
#define CREDENTIALS_H

// ======================================================
// --- CREDENCIAIS E ENDEREÇOS SENSÍVEIS ---
// ======================================================

// Credenciais da sua rede Wi-Fi
#define WIFI_SSID "WIFI_SSID"
#define WIFI_PASS "WIFI_PASS"

// --- Endereços dos Brokers ---
#define MQTT_CLOUD_BROKER "mqtts://********.ala.us-east-1.emqxsl.com:8883"
#define MQTT_LOCAL_BROKER "mqtt://RASPBERRY_LOCAL_IP"

// --- Credenciais para os brokers MQTT ---
#define MQTT_USER "MQTT_USER"
#define MQTT_PASS "MQTT_PASS"

#endif // CREDENTIALS_H
```

## Pasta ![`esp32_mqtt_local`](esp32_mqtt_local)

Contém o código para o ESP32 WROOM identificado como `ESP32_01` na topologia de rede. O código usa bibliotecas específicas de sensores, portanto tem inclusões no `CMakeLists.txt` para compilar corretamente.
> [!NOTE]
> Aqui também é necessário ativar o ambiente de desenvolvimento do ESP-IDF com o comando `source $HOME/esp32/esp-idf/export.sh` no terminal.
> Adicione seu `credentials.h` na pasta `esp32_mqtt_local/main`, cujas variáveis devem ser preenchidas para o código funcionar corretamente.

```c
#ifndef CREDENTIALS_H
#define CREDENTIALS_H

// ======================================================
// --- CONFIGURAÇÕES DE REDE E MQTT ---
// ======================================================
#define WIFI_SSID "WIFI_SSID"
#define WIFI_PASS "WIFI_PASS"
#define MQTT_BROKER "mqtt://RASPBERRY_LOCAL_IP:1883"
#define MQTT_USERNAME "MQTT_USERNAME"
#define MQTT_PASSWORD "MQTT_PASSWORD"

#endif // CREDENTIALS_H
```

## Pasta ![`raspberry_mqtt_broker`](raspberry_mqtt_broker)

Contém o código para o Raspberry Pi 3 Model B, que atua como um broker MQTT local. O código é escrito em Python e utiliza a biblioteca `paho-mqtt` para gerenciar as conexões MQTT. Como mostrado na topologia de rede, o script Python é responsável por receber mensagens dos dispositivos ESP32 através do broker MQTT local e gravar essas mensagens em tabelas no banco de dados local InfluxDB. Ele serve como um serviço de escuta que precisa ser executado continuamente para receber e processar as mensagens. A pasta `raspberry_mqtt_broker` precisa conter as variáveis de ambiente e, para isso, basta criar um arquivo `.env` nesta pasta com o seguinte conteúdo:

```env
# ======================================================
# --- PERFIL DE REDE ATIVO ---
# Altere esta variável para "HOME" ou "HOTSPOT"
# ======================================================
ACTIVE_NETWORK="HOME"

# ======================================================
# --- PERFIS DE CONFIGURAÇÃO ---
# ======================================================

# --- Perfil HOME ---
HOME_MQTT_BROKER_HOST=RASPBERRY_LOCAL_IP_WIFI
HOME_INFLUXDB_HOST=RASPBERRY_LOCAL_IP_WIFI

# --- Perfil HOTSPOT ---
HOTSPOT_MQTT_BROKER_HOST=RASPBERRY_LOCAL_IP_HOTSPOT
HOTSPOT_INFLUXDB_HOST=RASPBERRY_LOCAL_IP_HOTSPOT

# ======================================================
# --- CONFIGURAÇÕES COMUNS ---
# ======================================================

# --- Configurações MQTT ---
MQTT_BROKER_PORT=1883
MQTT_USERNAME=MQTT_USERNAME
MQTT_PASSWORD=MQTT_PASSWORD

# --- Configurações InfluxDB ---
INFLUXDB_PORT=8086
INFLUXDB_DATABASE=esp32_dados
INFLUXDB_USERNAME=admin    # Usuário padrão do InfluxDB Local
INFLUXDB_PASSWORD=admin123 # Senha padrão do InfluxDB Local

# --- Tópicos MQTT para assinatura (formato JSON) ---
MQTT_TOPICS_JSON='["esp32_01/#", "esp32_02/#"]'
```

>[!NOTE]
> O arquivo `.env` deve ser criado na pasta `raspberry_mqtt_broker`. Ele contém as variáveis de ambiente necessárias para a configuração do broker MQTT e do banco de dados InfluxDB. As variáveis devem ser preenchidas de acordo com a configuração da sua rede e do seu ambiente. O script Python irá ler essas variáveis para se conectar corretamente ao broker MQTT e ao banco de dados InfluxDB.

## Executando os Códigos
Para executar os códigos, siga as instruções abaixo:

1. **ESP32 WROOM**:
   - Navegue até a pasta `esp32_mqtt_cloud` ou `esp32_mqtt_local`.
   - Abra um terminal e ative o ambiente do ESP-IDF com o comando `source $HOME/esp32/esp-idf/export.sh`.
   - Compile o código com o comando `idf.py build`.
   - Conecte o ESP32 ao computador via USB.
   - Faça o upload do código para o ESP32 com o comando `idf.py flash`.
   - Reinicie o ESP32.

2. **Raspberry Pi 3 Model B**:
   - Navegue até a pasta `raspberry_mqtt_broker`.
   - Certifique-se de que o InfluxDB e o broker MQTT (EMQX) estão instalados e configurados corretamente.
   - No terminal, ative o ambiente virtual Python (se estiver usando um) e instale as dependências necessárias com `pip install -r requirements.txt`.
   - Execute o script Python com o comando `python mqtt_broker.py`.
   - Com uma execução bem-sucedida, o próximo passo é containerizar o código para que ele possa ser executado como um serviço no Raspberry Pi. Isso pode ser feito usando o Docker Compose. Basta abrir o terminal na pasta `raspberry_mqtt_broker` e executar o seguinte comando:

```bash
docker compose build
docker compose up -d
```

Isso iniciará o serviço do broker MQTT e do InfluxDB em contêineres Docker, permitindo que o Raspberry Pi funcione como um broker local para os dispositivos ESP32.

## Persistência de Dados
Os dados recebidos pelo Raspberry Pi são armazenados no banco de dados InfluxDB. Com a configuração correta do InfluxDB, você poderá consultar os dados armazenados usando ferramentas como o InfluxDB CLI ou o Grafana para visualização. O Grafana foi usado na topologia de rede para visualizar os dados em tempo real, permitindo uma análise mais fácil e intuitiva dos dados coletados pelos dispositivos ESP32. Basta baixar o Grafana para o Raspberry Pi e criar um painel de controle que usa queries para filtrar e exibir os dados do InfluxDB de forma gráfica e interativa.