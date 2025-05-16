import mqtt from 'mqtt';
import { insertLog } from './logger.js';
import 'dotenv/config';

// Variáveis de ambiente
const MQTT_BROKER_URL = process.env.MQTT_BROKER_URL;
const MQTT_USERNAME = process.env.MQTT_USERNAME;
const MQTT_PASSWORD = process.env.MQTT_PASSWORD;
const MQTT_TOPICS = process.env.MQTT_TOPICS?.split(',') || [];

if (!MQTT_BROKER_URL || !MQTT_USERNAME || !MQTT_PASSWORD || MQTT_TOPICS.length === 0) {
  throw new Error('❌ Verifique as variáveis de ambiente MQTT_BROKER_URL, MQTT_USERNAME, MQTT_PASSWORD e MQTT_TOPICS');
}

// Conexão com o broker MQTT
const client = mqtt.connect(MQTT_BROKER_URL, {
  username: MQTT_USERNAME,
  password: MQTT_PASSWORD,
  protocol: 'wss',
  reconnectPeriod: 5000,
});

client.on('connect', () => {
  console.log('✅ Conectado ao broker MQTT');

  client.subscribe(MQTT_TOPICS, (err, granted) => {
    if (err) {
      console.error('❌ Erro ao se inscrever nos tópicos:', err);
    } else {
      const subs = granted.map(g => g.topic).join(', ');
      console.log(`📡 Inscrito nos tópicos: ${subs}`);
    }
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
  console.error('❌ Erro na conexão MQTT:', err.message);
});

client.on('close', () => {
  console.warn('⚠️ Conexão com o broker MQTT fechada');
});