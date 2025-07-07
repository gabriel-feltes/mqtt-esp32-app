import paho.mqtt.client as mqtt
from influxdb import InfluxDBClient
import json
import time
import datetime
import os
import logging
import threading
import uuid
from dotenv import load_dotenv

# --- Configuração do Logging ---
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(threadName)s - %(levelname)s - %(message)s')

# --- Carregando Configurações do Ambiente ---
load_dotenv()

# --- Classe Principal do Gateway ---
class IoTGateway:
    def __init__(self):
        logging.info("Inicializando o Gateway IoT...")
        self.load_config()
        self.influx_client = self.setup_influxdb_client()
        self.local_mqtt_client = self.setup_local_mqtt_client()
        self.cloud_mqtt_client = self.setup_cloud_mqtt_client()

        if not all([self.influx_client, self.local_mqtt_client, self.cloud_mqtt_client]):
            raise RuntimeError("Falha ao inicializar um ou mais clientes. Verifique os logs e a configuração.")

    def load_config(self):
        """Carrega todas as configurações do arquivo .env"""
        self.active_network = os.getenv("ACTIVE_NETWORK", "HOME").upper()
        
        # MQTT Local
        self.local_mqtt_host = os.getenv(f"{self.active_network}_MQTT_BROKER_HOST")
        self.local_mqtt_port = int(os.getenv("MQTT_BROKER_PORT", 1883))
        self.local_mqtt_user = os.getenv("MQTT_USERNAME")
        self.local_mqtt_pass = os.getenv("MQTT_PASSWORD")
        self.local_mqtt_topics = json.loads(os.getenv("MQTT_TOPICS_JSON", '[]'))

        # MQTT Nuvem
        self.cloud_mqtt_host = os.getenv("CLOUD_MQTT_BROKER_HOST")
        self.cloud_mqtt_port = int(os.getenv("CLOUD_MQTT_BROKER_PORT", 8883))
        self.cloud_mqtt_user = os.getenv("CLOUD_MQTT_USERNAME")
        self.cloud_mqtt_pass = os.getenv("CLOUD_MQTT_PASSWORD")
        
        # Tópicos Nuvem
        self.topic_manage_rules = "sistema/regras/gerenciar"
        self.topic_list_rules = "sistema/regras/lista"
        self.topic_dashboard_status = "sistema/dashboard/status"

        # InfluxDB
        self.influx_host = os.getenv(f"{self.active_network}_INFLUXDB_HOST")
        self.influx_port = int(os.getenv("INFLUXDB_PORT", 8086))
        self.influx_user = os.getenv("INFLUXDB_USERNAME")
        self.influx_pass = os.getenv("INFLUXDB_PASSWORD")
        self.influx_db = os.getenv("INFLUXDB_DATABASE", "esp32_dados")

        # Arquivo de Regras
        self.rules_file = "automation_rules.json"
        self.check_interval = 15
        self.status_timeout = 15  # Timeout in seconds for device status

    def setup_influxdb_client(self):
        """Configura e retorna um cliente InfluxDB."""
        try:
            client = InfluxDBClient(
                host=self.influx_host, port=self.influx_port,
                username=self.influx_user, password=self.influx_pass,
                database=self.influx_db
            )
            client.create_database(self.influx_db)
            logging.info("Conexão com InfluxDB estabelecida com sucesso.")
            return client
        except Exception as e:
            logging.critical(f"Falha crítica na configuração do InfluxDB: {e}")
            return None

    def setup_local_mqtt_client(self):
        """Configura e conecta o cliente MQTT para a rede local."""
        client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
        client.username_pw_set(self.local_mqtt_user, self.local_mqtt_pass)
        client.on_connect = self.on_local_connect
        client.on_message = self.on_local_message
        try:
            client.connect(self.local_mqtt_host, self.local_mqtt_port, 60)
            return client
        except Exception as e:
            logging.error(f"Não foi possível conectar ao Broker MQTT LOCAL: {e}")
            return None

    def setup_cloud_mqtt_client(self):
        """Configura e conecta o cliente MQTT para a nuvem."""
        client = mqtt.Client(client_id=f"CloudManager-{uuid.uuid4()}", callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
        client.username_pw_set(self.cloud_mqtt_user, self.cloud_mqtt_pass)
        client.on_connect = self.on_cloud_connect
        client.on_message = self.on_cloud_message
        client.tls_set()
        try:
            client.connect(self.cloud_mqtt_host, self.cloud_mqtt_port, 60)
            return client
        except Exception as e:
            logging.error(f"Não foi possível conectar ao Broker MQTT da NUVEM: {e}")
            return None

    def on_local_connect(self, client, userdata, flags, rc, properties=None):
        if rc == 0:
            logging.info(f"Conectado ao Broker MQTT LOCAL.")
            if self.local_mqtt_topics:
                subscriptions = [(topic, 0) for topic in self.local_mqtt_topics]
                client.subscribe(subscriptions)
                logging.info(f"Inscrito nos tópicos locais: {self.local_mqtt_topics}")
        else:
            logging.error(f"Falha ao conectar ao Broker MQTT LOCAL, código: {rc}")

    def on_cloud_connect(self, client, userdata, flags, rc, properties=None):
        if rc == 0:
            logging.info(f"Conectado ao Broker MQTT da NUVEM.")
            client.subscribe(self.topic_manage_rules)
            logging.info(f"Inscrito no tópico de gerenciamento: {self.topic_manage_rules}")
        else:
            logging.error(f"Falha ao conectar ao Broker da NUVEM, código: {rc}")

    def on_local_message(self, client, userdata, msg):
        """Processa mensagens da rede local (sensores) e grava no InfluxDB."""
        topic = msg.topic
        payload_str = msg.payload.decode('utf-8')

        try:
            topic_parts = topic.split('/')
            device_id = topic_parts[0]
            tags, fields, measurement_name = {"device_id": device_id}, {}, None
            
            try: 
                data = json.loads(payload_str)
            except json.JSONDecodeError: 
                data = payload_str
            
            if len(topic_parts) > 2 and topic_parts[1] == 'sensor':
                sensor_type = topic_parts[2]
                if sensor_type == "bmp280" and isinstance(data, dict):
                    measurement_name = "bmp280"
                    if "temperature" in data: fields["temperature"] = float(data["temperature"])
                    if "pressure" in data: fields["pressure"] = float(data["pressure"])
                    if "pressure_sea_level" in data: fields["pressure_sea_level"] = float(data["pressure_sea_level"])
                elif sensor_type == "dht11" and isinstance(data, dict):
                    measurement_name = "dht11"
                    if "temperature" in data: fields["temperature"] = float(data["temperature"])
                    if "humidity" in data: fields["humidity"] = float(data["humidity"])
                elif sensor_type == "mq135" and isinstance(data, dict):
                    measurement_name = "mq135"
                    if "adc_raw" in data: fields["adc_raw"] = int(data["adc_raw"])
                    if "ppm" in data: fields["ppm"] = float(data["ppm"])
                elif sensor_type == "ldr" and isinstance(data, dict):
                    measurement_name = "ldr"
                    if "ldr_raw" in data: fields["ldr_raw"] = int(data["ldr_raw"])
            
            elif len(topic_parts) == 4 and topic_parts[1] == 'gpio' and topic_parts[3] == 'state':
                measurement_name = "gpio_state"
                pin_num = topic_parts[2]
                tags['pin'] = f"gpio{pin_num}"
                fields["state"] = payload_str.upper()  # Store "ON" or "OFF" as string
            
            elif (len(topic_parts) == 3 and topic_parts[1] == "system" and topic_parts[2] == "status") or \
                 (len(topic_parts) == 2 and topic_parts[1] == "status"):
                measurement_name = "device_status"
                fields["status"] = payload_str

            if measurement_name and fields:
                json_body = [{"measurement": measurement_name, "tags": tags, "time": datetime.datetime.utcnow().isoformat() + "Z", "fields": fields}]
                self.influx_client.write_points(json_body)
            else:
                logging.warning(f"Nenhuma medição ou campo válido identificado para o tópico '{topic}'. Nenhum dado foi gravado.")

        except Exception as e:
            logging.error(f"Erro inesperado ao processar mensagem local de {topic}: {e}")

    def on_cloud_message(self, client, userdata, msg):
        """Processa comandos de gerenciamento de regras vindos do frontend."""
        try:
            payload = json.loads(msg.payload.decode())
            command = payload.get("command")
            
            rules = self.get_rules()
            if command == "add_rule":
                new_rule = payload.get("rule")
                new_rule['id'] = str(uuid.uuid4())
                rules.append(new_rule)
            elif command == "delete_rule":
                rule_id_to_delete = payload.get("rule_id")
                rules = [rule for rule in rules if rule.get('id') != rule_id_to_delete]
            
            self.save_rules(rules)
            self.cloud_mqtt_client.publish(self.topic_list_rules, json.dumps(rules), qos=1)
        except Exception as e:
            logging.error(f"Erro ao processar comando da nuvem: {e}")

    def get_rules(self):
        try:
            with open(self.rules_file, 'r') as f: return json.load(f)
        except: return []

    def save_rules(self, rules):
        with open(self.rules_file, 'w') as f: json.dump(rules, f, indent=4)

    def automation_loop(self):
        """Loop principal da thread de automação e status."""
        while True:
            rules = self.get_rules()
            if rules:
                for rule in rules:
                    try:
                        aggregator = "last" if rule['measurement'] == "gpio_state" else "mean"
                        query = f"SELECT {aggregator}(\"{rule['field']}\") FROM \"{rule['measurement']}\" WHERE time > now() - {rule['range']}"
                        if rule.get('filter'): query += f" AND {rule['filter']}"
                        
                        result = self.influx_client.query(query)
                        points = list(result.get_points())
                        
                        if points and points[0].get(aggregator) is not None:
                            value, threshold, op = points[0][aggregator], float(rule["threshold"]), rule["operator"]
                            if (op == ">" and value > threshold) or (op == "<" and value < threshold) or (op == "==" and value == threshold):
                                self.cloud_mqtt_client.publish(rule['action_topic'], rule['action_payload'], qos=1)
                    except Exception as e:
                        logging.error(f"Erro ao executar a regra '{rule['name']}': {e}")

            try:
                status_data = {}
                
                def query_and_add(measurement, field, key, tag_filter="", is_gpio=False, round_digits=2, check_timeout=False):
                    try:
                        query = f"SELECT last(\"{field}\") FROM \"{measurement}\""
                        if tag_filter:
                            query += f" WHERE {tag_filter}"
                        result = self.influx_client.query(query)
                        points = list(result.get_points())
                        if points and points[0].get('last') is not None:
                            value = points[0]['last']
                            if check_timeout:
                                # Get timestamp of the last point
                                last_time = points[0].get('time')
                                last_time_dt = datetime.datetime.strptime(last_time, "%Y-%m-%dT%H:%M:%S.%fZ")
                                time_diff = (datetime.datetime.utcnow() - last_time_dt).total_seconds()
                                if time_diff > self.status_timeout:
                                    status_data[key] = "offline"
                                    return
                            if is_gpio:
                                status_data[key] = "ON" if value == "ON" else "OFF"
                            elif isinstance(value, (int, float)):
                                status_data[key] = round(value, round_digits)
                            elif measurement == "device_status" and value in ["heartbeat", "online"]:
                                status_data[key] = "online"
                            else:
                                status_data[key] = value
                    except:
                        pass

                # esp32_01 sensor and GPIO data
                query_and_add("dht11", "temperature", "dht11_temperature", tag_filter="\"device_id\" = 'esp32_01'")
                query_and_add("dht11", "humidity", "dht11_humidity", tag_filter="\"device_id\" = 'esp32_01'", round_digits=1)
                query_and_add("bmp280", "temperature", "bmp280_temperature", tag_filter="\"device_id\" = 'esp32_01'")
                query_and_add("bmp280", "pressure", "bmp280_pressure", tag_filter="\"device_id\" = 'esp32_01'")
                query_and_add("bmp280", "pressure_sea_level", "bmp280_sea_level_pressure", tag_filter="\"device_id\" = 'esp32_01'")
                query_and_add("mq135", "ppm", "mq135_ppm", tag_filter="\"device_id\" = 'esp32_01'")
                query_and_add("ldr", "ldr_raw", "ldr_raw", tag_filter="\"device_id\" = 'esp32_01'", round_digits=0)
                query_and_add("gpio_state", "state", "gpio_2_state", tag_filter="\"pin\" = 'gpio2' AND \"device_id\" = 'esp32_01'", is_gpio=True)

                # esp32_02 status (mapped to device_status for dashboard)
                query_and_add("device_status", "status", "device_status", tag_filter="\"device_id\" = 'esp32_02'", check_timeout=True)

                if status_data:
                    status_data['last_update'] = datetime.datetime.now().isoformat()
                    self.cloud_mqtt_client.publish(self.topic_dashboard_status, json.dumps(status_data), qos=1)
                    logging.info(f"Status publicado para o dashboard: {status_data}")

            except Exception as e:
                logging.error(f"Erro ao publicar status periódico completo: {e}")

            time.sleep(self.check_interval)

    def run(self):
        """Inicia todos os loops e threads."""
        self.local_mqtt_client.loop_start()
        self.cloud_mqtt_client.loop_start()

        automation_thread = threading.Thread(target=self.automation_loop, name="AutomationThread", daemon=True)
        automation_thread.start()

        logging.info("Gateway IoT em execução. Pressione Ctrl+C para parar.")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            logging.info("Desligando o Gateway IoT...")
            self.local_mqtt_client.loop_stop()
            self.cloud_mqtt_client.loop_stop()
            logging.info("Gateway desligado.")

if __name__ == '__main__':
    gateway = IoTGateway()
    gateway.run()
