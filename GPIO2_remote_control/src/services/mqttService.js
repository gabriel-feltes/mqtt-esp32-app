import mqtt from 'mqtt'

// Variáveis de estado
let client = null
let statusCallbacks = {
  connection: null,
  esp32: null
}
let lastEsp32MessageTime = null
let esp32StatusCheckInterval = null

// Configurações
const ESP32_STATUS_TOPIC = 'esp32_02/status'
const ESP32_TIMEOUT_MS = 10000 // 10 segundos sem resposta = offline
const STATUS_CHECK_INTERVAL = 5000 // Verificar a cada 5 segundos

export function connectMQTT(username, password, deployment, onConnectionStatus, onEsp32Status) {
  const connectUrl = `wss://${deployment}.ala.us-east-1.emqxsl.com:8084/mqtt`
  
  const options = {
    connectTimeout: 4000,
    clientId: 'web_client_' + Math.random().toString(16).slice(2, 10),
    username,
    password,
    will: {
      topic: ESP32_STATUS_TOPIC,
      payload: 'offline',
      qos: 1,
      retain: true
    }
  }

  // Armazena os callbacks
  statusCallbacks.connection = onConnectionStatus
  statusCallbacks.esp32 = onEsp32Status

  // Cria o cliente MQTT
  client = mqtt.connect(connectUrl, options)

  // Configura os listeners
  setupEventListeners()
}

function setupEventListeners() {
  if (!client) return

  client.on('connect', () => {
    console.log('Conectado ao broker MQTT')
    notifyConnectionStatus('Conectado')
    
    // Inscreve no tópico de status do ESP32
    subscribeToTopic(ESP32_STATUS_TOPIC)
    
    // Inicia a verificação periódica do status do ESP32
    startEsp32StatusMonitoring()
  })

  client.on('reconnect', () => {
    console.log('Tentando reconectar...')
    notifyConnectionStatus('Reconectando')
    stopEsp32StatusMonitoring()
  })

  client.on('error', (err) => {
    console.error('Erro de conexão:', err)
    notifyConnectionStatus('Erro de conexão')
    stopEsp32StatusMonitoring()
  })

  client.on('offline', () => {
    console.warn('Cliente offline')
    notifyConnectionStatus('Desconectado')
    stopEsp32StatusMonitoring()
  })

  client.on('message', (topic, message) => {
    if (topic === ESP32_STATUS_TOPIC) {
      handleEsp32StatusMessage(message.toString())
    }
  })

  client.on('message', (topic, message) => {
  const msg = message.toString()

  if (topic === ESP32_STATUS_TOPIC) {
    handleEsp32StatusMessage(msg)
  }
})

}

export function subscribeToTopic(topic) {
  if (client && client.connected) {
    client.subscribe(topic, (err) => {
      if (err) {
        console.error(`Erro ao subscrever no tópico ${topic}:`, err)
      } else {
        console.log(`Inscrito no tópico ${topic}`)
      }
    })
  }
}

function handleEsp32StatusMessage(message) {
  lastEsp32MessageTime = Date.now()
  
  if (message === 'online' || message === 'heartbeat') {
    notifyEsp32Status('Online')
  } else if (message === 'offline') {
    notifyEsp32Status('Offline')
  }
}

function startEsp32StatusMonitoring() {
  stopEsp32StatusMonitoring() // Limpa qualquer intervalo existente
  
  esp32StatusCheckInterval = setInterval(() => {
    if (!lastEsp32MessageTime || (Date.now() - lastEsp32MessageTime) > ESP32_TIMEOUT_MS) {
      notifyEsp32Status('Offline (sem resposta)')
    }
  }, STATUS_CHECK_INTERVAL)
}

function stopEsp32StatusMonitoring() {
  if (esp32StatusCheckInterval) {
    clearInterval(esp32StatusCheckInterval)
    esp32StatusCheckInterval = null
  }
}

function notifyConnectionStatus(status) {
  statusCallbacks.connection && statusCallbacks.connection(status)
}

function notifyEsp32Status(status) {
  statusCallbacks.esp32 && statusCallbacks.esp32(status)
}

export function publishMessage(topic, message) {
  return new Promise((resolve, reject) => {
    if (!client || !client.connected) {
      reject(new Error('MQTT não conectado'))
      return
    }

    client.publish(topic, message, { qos: 1 }, (err) => {
      if (err) {
        reject(err)
      } else {
        resolve()
      }
    })
  })
}

export function disconnectMQTT() {
  if (client) {
    client.end()
    client = null
  }
  stopEsp32StatusMonitoring()
  lastEsp32MessageTime = null
}

export function getConnectionStatus() {
  if (!client) return 'Desconectado'
  if (client.connected) return 'Conectado'
  if (client.reconnecting) return 'Reconectando'
  return 'Desconectado'
}

export function getEsp32Status() {
  if (!lastEsp32MessageTime) return 'Desconhecido'
  if ((Date.now() - lastEsp32MessageTime) > ESP32_TIMEOUT_MS) {
    return 'Offline'
  }
  return 'Online'
}