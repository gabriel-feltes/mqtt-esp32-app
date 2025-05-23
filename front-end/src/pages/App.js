import { useEffect, useState } from 'react'
import { useNavigate, Link } from 'react-router-dom'
import { connectMQTT, publishMessage } from '../services/mqttService'
import '../styles/App.css'

export default function App() {
  const navigate = useNavigate()
  const [connectionStatus, setConnectionStatus] = useState('Conectando...')
  const [esp32Status, setEsp32Status] = useState('Verificando...')
  const [publishStatus, setPublishStatus] = useState('')
  const [buttonsDisabled, setButtonsDisabled] = useState(true)

  useEffect(() => {
    const username = localStorage.getItem('mqtt_username')
    const password = localStorage.getItem('mqtt_password')
    const deployment = localStorage.getItem('mqtt_deployment')

    if (!username || !password || !deployment) {
      navigate('/')
      return
    }

    const handleEsp32Status = (status) => {
      setEsp32Status(status)
      setButtonsDisabled(!status.includes('Online'))
    }

    connectMQTT(
      username,
      password,
      deployment,
      setConnectionStatus,
      handleEsp32Status
    )
  }, [navigate])

  const handlePublish = async (topic, message) => {
    try {
      if (esp32Status.includes('Online')) {
        await publishMessage(topic, message)
        setPublishStatus(`Mensagem "${message}" publicada com sucesso no tópico "${topic}"`)

        await fetch('https://back-end-gabriel-feltes-dos-santos-projects.vercel.app/log', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            topic,
            message,
            user: localStorage.getItem('mqtt_username')
          })
        })
      } else {
        setPublishStatus('Erro: ESP32 parece estar offline')
      }
    } catch (error) {
      setPublishStatus(`Erro ao publicar mensagem: ${error.message}`)
    }
  }

  const statusClasses = {
    connection: {
      conectado: 'status-connected',
      reconectando: 'status-reconnecting',
      'erro de conexão': 'status-error',
      desconectado: 'status-disconnected',
      conectando: 'status-disconnected',
    },
    esp32: {
      online: 'status-connected',
      offline: 'status-error',
      'offline (sem resposta)': 'status-error',
      'verificando...': 'status-reconnecting'
    }
  }

  const connectionClass = statusClasses.connection[connectionStatus.toLowerCase()] || ''
  const esp32Class = statusClasses.esp32[esp32Status.toLowerCase()] || ''

  return (
    <div className="app-container">
      <h1>Controle do LED via MQTT</h1>
      
      <div className="status-container">
        <p className="status-text">
          Status do Broker: <span className={connectionClass}>{connectionStatus}</span>
        </p>
        <p className="status-text">
          Status do ESP32: <span className={esp32Class}>{esp32Status}</span>
        </p>
      </div>

      <div className="button-container">
        <button 
          id="onBtn" 
          onClick={() => handlePublish('esp32/gpio/led', 'ON')}
          disabled={buttonsDisabled}
        >
          Ligar LED
        </button>
        <button 
          id="offBtn" 
          onClick={() => handlePublish('esp32/gpio/led', 'OFF')}
          disabled={buttonsDisabled}
        >
          Desligar LED
        </button>
      </div>

      {publishStatus && (
        <p className={`publish-status ${publishStatus.includes('Erro') ? 'error' : 'success'}`}>
          {publishStatus}
        </p>
      )}

      {/* Link para visualizar os logs */}
      <div style={{ marginTop: '1rem' }}>
        <Link to="/log">
          <button style={{ padding: '0.5rem 1rem' }}>Ver Logs MQTT</button>
        </Link>
      </div>
    </div>
  )
}