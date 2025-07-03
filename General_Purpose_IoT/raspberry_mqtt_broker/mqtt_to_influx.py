import paho.mqtt.client as mqtt
from influxdb import InfluxDBClient
import json
import time
import datetime
import os
import logging
from dotenv import load_dotenv

# --- Configuração ---
# Carrega variáveis de ambiente do arquivo .env
load_dotenv()

# Configura o logging para exibir mensagens informativas
# O formato inclui timestamp, nível do log e a mensagem.
# logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

# --- Leitura das Configurações do Ambiente ---
# Determina o perfil de rede ativo (ex: 'HOME' ou 'WORK')
ACTIVE_NETWORK = os.getenv("ACTIVE_NETWORK", "HOME").upper()
# logging.info(f"Perfil de rede ativo: {ACTIVE_NETWORK}")

# Configurações do Broker MQTT
MQTT_BROKER_HOST = os.getenv(f"{ACTIVE_NETWORK}_MQTT_BROKER_HOST")
MQTT_BROKER_PORT = int(os.getenv("MQTT_BROKER_PORT", 1883))
MQTT_USERNAME = os.getenv("MQTT_USERNAME")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD")

# Configurações do InfluxDB
INFLUXDB_HOST = os.getenv(f"{ACTIVE_NETWORK}_INFLUXDB_HOST")
INFLUXDB_PORT = int(os.getenv("INFLUXDB_PORT", 8086))
INFLUXDB_USERNAME = os.getenv("INFLUXDB_USERNAME")
INFLUXDB_PASSWORD = os.getenv("INFLUXDB_PASSWORD")
INFLUXDB_DATABASE = os.getenv("INFLUXDB_DATABASE", "esp32_dados")

# Tópicos MQTT para inscrição (suporta wildcards)
# Exemplo para .env: MQTT_TOPICS_JSON='["esp32_01/#", "esp32_02/#"]'
try:
    mqtt_topics_json_str = os.getenv("MQTT_TOPICS_JSON", '[]')
    # O script se inscreverá em cada tópico desta lista.
    # Usar wildcards como '#' é recomendado para capturar todas as mensagens de um dispositivo.
    MQTT_TOPICS = json.loads(mqtt_topics_json_str)
    # if not MQTT_TOPICS:
        # logging.warning("MQTT_TOPICS_JSON não está definido ou está vazio. Nenhuma inscrição será feita.")
except json.JSONDecodeError:
    # logging.error(f"Falha ao decodificar MQTT_TOPICS_JSON: '{mqtt_topics_json_str}'. Verifique o formato no seu arquivo .env.")
    MQTT_TOPICS = []

# Instância global do cliente InfluxDB
influx_client_global = None

def on_connect(client, userdata, flags, rc, properties=None):
    """Callback executado quando o cliente se conecta ao broker MQTT."""
    if rc == 0:
        # logging.info(f"Conectado com sucesso ao Broker MQTT em {MQTT_BROKER_HOST}")
        if MQTT_TOPICS:
            # Inscreve-se em todos os tópicos definidos na variável de ambiente
            subscriptions = [(topic, 0) for topic in MQTT_TOPICS]
            client.subscribe(subscriptions)
            # logging.info(f"Inscrito nos tópicos: {MQTT_TOPICS}")
        # else:
            # logging.warning("Nenhum tópico MQTT para se inscrever. Verifique sua configuração .env.")
    # else:
        # logging.error(f"Falha ao conectar ao Broker MQTT ({MQTT_BROKER_HOST}), código de retorno: {rc}")

def on_message(client, userdata, msg):
    """
    Callback executado quando uma mensagem é recebida.
    Esta função agora verifica se os campos existem antes de adicioná-los.
    """
    global influx_client_global
    if not influx_client_global:
        # logging.error("Cliente InfluxDB não inicializado. Não é possível gravar dados.")
        return

    topic = msg.topic
    try:
        payload_str = msg.payload.decode('utf-8')
    except UnicodeDecodeError:
        # logging.warning(f"Não foi possível decodificar o payload para o tópico {topic}. Pode não ser UTF-8.")
        return

    # logging.debug(f"Mensagem recebida - Tópico: {topic}, Payload: {payload_str}")

    json_body = []
    current_time_utc = datetime.datetime.utcnow().isoformat() + "Z"
    
    try:
        device_id = topic.split('/')[0]
    except IndexError:
        device_id = "unknown_device"
        # logging.warning(f"Não foi possível determinar o device_id do tópico: {topic}")

    # A tag 'device_id' é comum a todos os dados.
    tags = {"device_id": device_id}
    
    try:
        data = json.loads(payload_str)
    except json.JSONDecodeError:
        data = payload_str

    try:
        measurement_name = None
        fields = {} # Sempre começa com um dicionário de campos vazio

        # --- Lógica de Análise de Dados Simplificada ---
        
        # ESP32_01: Sensor BMP280
        if "sensor/bmp280" in topic and isinstance(data, dict):
            measurement_name = "bmp280"
            if "temperature" in data:
                fields["temperature"] = float(data["temperature"])
            if "pressure" in data:
                fields["pressure"] = float(data["pressure"])
            if "pressure_sea_level" in data:
                fields["pressure_sea_level"] = float(data["pressure_sea_level"])

        # ESP32_01: Sensor DHT11
        elif "sensor/dht11" in topic and isinstance(data, dict):
            measurement_name = "dht11"
            if "temperature" in data:
                fields["temperature"] = float(data["temperature"])
            if "humidity" in data:
                fields["humidity"] = float(data["humidity"])

        # ESP32_01: Sensor MQ135
        elif "sensor/mq135" in topic and isinstance(data, dict):
            measurement_name = "mq135"
            if "adc_raw" in data:
                fields["adc_raw"] = int(data["adc_raw"])
            if "ppm" in data:
                fields["ppm"] = float(data["ppm"])

        # ESP32_01: Sensor LDR
        elif "sensor/ldr" in topic and isinstance(data, dict):
            measurement_name = "ldr"
            if "ldr_raw" in data:
                fields["ldr_raw"] = int(data["ldr_raw"])

        # ESP32_02: Estado do LED (GPIO)
        elif "gpio/gpio2/state" in topic:
            measurement_name = "gpio_state"
            tags['pin'] = 'gpio2'
            fields = {"state": str(payload_str).upper()}

        # Tópico de Status Genérico para todos os dispositivos
        elif "/status" in topic:
            measurement_name = "device_status"
            fields = {"status": str(payload_str)}

        # Se um measurement foi identificado E o dicionário de campos não estiver vazio, envia os dados.
        if measurement_name and fields:
            json_body.append({
                "measurement": measurement_name,
                "tags": tags,
                "time": current_time_utc,
                "fields": fields
            })
            influx_client_global.write_points(json_body)
            # logging.info(f"Dados do tópico '{topic}' gravados na medição '{measurement_name}' do InfluxDB.")

    except (ValueError, TypeError):
        pass # logging.error(f"Erro ao processar mensagem do tópico {topic}: {e}. Payload: {payload_str}")
    except Exception:
        pass # logging.error(f"Um erro inesperado ocorreu ao processar a mensagem de {topic}: {e}")


def setup_mqtt_client():
    """Configura e conecta o cliente MQTT."""
    if not MQTT_BROKER_HOST:
        # logging.critical("MQTT_BROKER_HOST não está definido. Verifique seu arquivo .env.")
        return None
    
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    if MQTT_USERNAME and MQTT_PASSWORD:
        client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    
    client.on_connect = on_connect
    client.on_message = on_message
    
    try:
        client.connect(MQTT_BROKER_HOST, MQTT_BROKER_PORT, 60)
        return client
    except Exception:
        # logging.error(f"Não foi possível conectar ao Broker MQTT: {e}")
        return None

def setup_influxdb_client():
    """Configura o cliente InfluxDB e garante que o banco de dados exista."""
    global influx_client_global
    if not INFLUXDB_HOST:
        # logging.critical("INFLUXDB_HOST não está definido. Verifique seu arquivo .env.")
        return False
    
    try:
        # logging.info(f"Conectando ao InfluxDB em {INFLUXDB_HOST}:{INFLUXDB_PORT}...")
        influx_client_global = InfluxDBClient(
            host=INFLUXDB_HOST,
            port=INFLUXDB_PORT,
            username=INFLUXDB_USERNAME,
            password=INFLUXDB_PASSWORD,
            database=INFLUXDB_DATABASE
        )
        
        databases = influx_client_global.get_list_database()
        if {"name": INFLUXDB_DATABASE} not in databases:
            # logging.warning(f"Banco de dados '{INFLUXDB_DATABASE}' não encontrado. Criando agora...")
            influx_client_global.create_database(INFLUXDB_DATABASE)
            # logging.info(f"Banco de dados '{INFLUXDB_DATABASE}' criado com sucesso.")
        
        # logging.info("Conectado com sucesso ao InfluxDB.")
        return True
    except Exception:
        # logging.error(f"Não foi possível conectar ou configurar o InfluxDB: {e}")
        return False

if __name__ == '__main__':
    # logging.info("Iniciando serviço MQTT-para-InfluxDB...")
    
    if not setup_influxdb_client():
        # logging.critical("Saindo: Falha na configuração do InfluxDB.")
        exit(1)
        
    mqtt_client = setup_mqtt_client()
    if not mqtt_client:
        # logging.critical("Saindo: Falha na configuração do MQTT.")
        exit(1)
        
    mqtt_client.loop_start()
    # logging.info("Serviço em execução. Pressione Ctrl+C para parar.")
    
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass # logging.info("Serviço interrompido pelo usuário.")
    finally:
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
        influx_client_global.close()
        # logging.info("Serviço parado e clientes desconectados.")
