import mqtt from 'mqtt';

let client = null;
let messageCallback = null;

export function connectMQTT(username, password, deployment, onConnectionStatus, onMessage) {
  const connectUrl = `wss://${deployment}.ala.us-east-1.emqxsl.com:8084/mqtt`;
  const options = {
    connectTimeout: 4000,
    clientId: `web_client_${Math.random().toString(16).slice(2, 10)}`,
    username,
    password,
    clean: true
  };

  client = mqtt.connect(connectUrl, options);
  messageCallback = onMessage;

  client.on('connect', () => {
    onConnectionStatus('Conectado ao Broker!');
  });

  client.on('reconnect', () => {
    onConnectionStatus('Reconectando...');
  });

  client.on('error', (err) => {
    onConnectionStatus('Erro de conexão MQTT. Verifique o console.');
    console.error('MQTT Error:', err);
  });

  client.on('offline', () => {
    onConnectionStatus('Desconectado');
  });

  client.on('message', (topic, message) => {
    messageCallback?.(topic, message.toString());
  });
}

export function subscribeToTopic(topic) {
  if (client && client.connected) {
    client.subscribe(topic, (err) => {
      if (err) {
        console.error(`Erro ao subscrever no tópico ${topic}:`, err);
      } else {
        console.log(`Inscrito no tópico ${topic}`);
      }
    });
  }
}

export function publishMessage(topic, message) {
  return new Promise((resolve, reject) => {
    if (!client || !client.connected) {
      reject(new Error('MQTT não conectado'));
      return;
    }
    client.publish(topic, message, { qos: 1 }, (err) => {
      if (err) {
        reject(err);
      } else {
        resolve();
      }
    });
  });
}

export function disconnectMQTT() {
  if (client) {
    client.end();
    client = null;
    messageCallback = null;
  }
}