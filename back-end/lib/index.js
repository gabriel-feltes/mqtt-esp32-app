import mqtt from 'mqtt';
import { insertLog } from './logger.js';

// Pegando as variáveis do ambiente (.env ou Railway)
const MQTT_BROKER_URL = process.env.MQTT_BROKER_URL;
const MQTT_USERNAME = process.env.MQTT_USERNAME;
const MQTT_PASSWORD = process.env.MQTT_PASSWORD;
const MQTT_TOPIC = process.env.MQTT_TOPIC || '#'; // padrão se não definido

// Conectar ao broker MQTT
const client = mqtt.connect(MQTT_BROKER_URL, {
  username: MQTT_USERNAME,
  password: MQTT_PASSWORD,
  protocol: 'wss',
  reconnectPeriod: 5000,
});

client.on('connect', () => {
  console.log('✅ Conectado ao broker MQTT');
  client.subscribe(MQTT_TOPIC, (err) => {
    if (err) console.error('Erro ao se inscrever no tópico:', err);
    else console.log(`📡 Inscrito no tópico: ${MQTT_TOPIC}`);
  });
});

client.on('message', async (topic, message) => {
  const msg = message.toString();
  const user = 'backend-listener';

  try {
    await insertLog({ topic, message: msg, user });
    console.log(`💾 Log inserido: [${topic}] ${msg}`);
  } catch (err) {
    console.error('❌ Erro ao inserir log no Supabase:', err.message);
  }
});

client.on('error', (err) => {
  console.error('Erro na conexão MQTT:', err.message);
});

client.on('close', () => {
  console.warn('⚠️ Conexão com o broker MQTT fechada');
});
