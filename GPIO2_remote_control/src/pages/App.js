import { useEffect, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { connectMQTT, publishMessage, disconnectMQTT } from '../services/mqttService';
import '../styles/App.css';

export default function App() {
  const navigate = useNavigate();
  const [connectionStatus, setConnectionStatus] = useState('Conectando...');
  const [esp32Status, setEsp32Status] = useState('Verificando...');
  const [publishStatus, setPublishStatus] = useState('');
  const [buttonsDisabled, setButtonsDisabled] = useState(true);

  useEffect(() => {
    const checkCredentials = () => {
      console.log('Verificando credenciais no localStorage...');
      const token = localStorage.getItem('session_token');
      console.log('Token encontrado:', token);

      if (!token) {
        console.log('Token ausente, redirecionando para login');
        navigate('/');
      } else {
        console.log('Token válido, iniciando conexão MQTT');
        const [username, password, deployment] = atob(token).split(':');
        const handleEsp32Status = (status) => {
          setEsp32Status(status);
          setButtonsDisabled(!status.includes('Online'));
        };

        connectMQTT(username, password, deployment, setConnectionStatus, handleEsp32Status);
      }
    };

    const timer = setTimeout(checkCredentials, 100);

    return () => {
      clearTimeout(timer);
      disconnectMQTT();
    };
  }, [navigate]);

  const handleLogout = () => {
    console.log('Logout iniciado, limpando localStorage');
    localStorage.removeItem('session_token');
    disconnectMQTT();
    navigate('/');
  };

  const handlePublish = async (topic, message) => {
    try {
      if (esp32Status.includes('Online')) {
        await publishMessage(topic, message);
        setPublishStatus(`Mensagem "${message}" publicada com sucesso no tópico "${topic}"`);
      } else {
        setPublishStatus('Erro: ESP32 parece estar offline');
      }
    } catch (error) {
      setPublishStatus(`Erro ao publicar mensagem: ${error.message}`);
    }
  };

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
      'verificando...': 'status-reconnecting',
    },
  };

  const connectionClass = statusClasses.connection[connectionStatus.toLowerCase()] || '';
  const esp32Class = statusClasses.esp32[esp32Status.toLowerCase()] || '';

  return (
    <div className="app-container">
      <div className="header">
        <h1>Controle do LED via MQTT</h1>
        <button id="logoutBtn" onClick={handleLogout} className="logout-button">
          Logout
        </button>
      </div>
      
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
          onClick={() => handlePublish('esp32_02/gpio/gpio2/command', 'ON')}
          disabled={buttonsDisabled}
        >
          Ligar LED
        </button>
        <button
          id="offBtn"
          onClick={() => handlePublish('esp32_02/gpio/gpio2/command', 'OFF')}
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
    </div>
  );
}