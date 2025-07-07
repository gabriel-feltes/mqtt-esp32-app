import { useEffect, useState } from 'react';
import { useNavigate } from 'react-router-dom';
import { connectMQTT, publishMessage, disconnectMQTT, subscribeToTopic } from '../services/mqttService';
import '../styles/App.css';

export default function App() {
  const navigate = useNavigate();
  const [connectionStatus, setConnectionStatus] = useState('Conectando ao Broker MQTT...');
  const [controlledGpios, setControlledGpios] = useState([]);
  const [rules, setRules] = useState([]);
  const [status, setStatus] = useState({
    device_status: '-',
    dht11_temperature: '-',
    dht11_humidity: '-',
    bmp280_temperature: '-',
    bmp280_pressure: '-',
    bmp280_sea_level_pressure: '-',
    mq135_ppm: '-',
    ldr_raw: '-',
    last_update: '-'
  });
  const [selectedMeasurement, setSelectedMeasurement] = useState('dht11');

  const measurementOptions = [
    { value: 'dht11', label: 'DHT11 (Temperatura/Umidade)' },
    { value: 'bmp280', label: 'BMP280 (Temperatura/Pressão)' },
    { value: 'mq135', label: 'MQ135 (Qualidade do Ar)' },
    { value: 'ldr', label: 'LDR (Luminosidade)' }
  ];

  const fieldOptions = {
    dht11: [
      { value: 'temperature', label: 'Temperatura' },
      { value: 'humidity', label: 'Umidade' }
    ],
    bmp280: [
      { value: 'temperature', label: 'Temperatura' },
      { value: 'pressure', label: 'Pressão' },
      { value: 'pressure_sea_level', label: 'Pressão ao Nível do Mar' }
    ],
    mq135: [
      { value: 'ppm', label: 'PPM (Qualidade do Ar)' },
      { value: 'adc_raw', label: 'ADC Bruto' }
    ],
    ldr: [
      { value: 'ldr_raw', label: 'Luminosidade (Raw)' }
    ]
  };

  const filterOptions = [
    { value: '', label: 'Nenhum' },
    { value: '"device_id" = \'esp32_01\'', label: 'Dispositivo: ESP32_01' },
    { value: '"device_id" = \'esp32_02\'', label: 'Dispositivo: ESP32_02' }
  ];

  const rangeOptions = [
    { value: '1m', label: 'Último 1 minuto' },
    { value: '5m', label: 'Últimos 5 minutos' },
    { value: '15m', label: 'Últimos 15 minutos' },
    { value: '1h', label: 'Última 1 hora' }
  ];

  const actionTopicOptions = [
    { value: 'esp32_02/gpio/2/set', label: 'GPIO 2' },
    { value: 'esp32_02/gpio/4/set', label: 'GPIO 4' },
    { value: 'esp32_02/gpio/5/set', label: 'GPIO 5' },
    { value: 'esp32_02/gpio/13/set', label: 'GPIO 13' },
    { value: 'esp32_02/gpio/14/set', label: 'GPIO 14' },
    { value: 'esp32_02/gpio/16/set', label: 'GPIO 16' },
    { value: 'esp32_02/gpio/17/set', label: 'GPIO 17' },
    { value: 'esp32_02/gpio/18/set', label: 'GPIO 18' },
    { value: 'esp32_02/gpio/19/set', label: 'GPIO 19' },
    { value: 'esp32_02/gpio/21/set', label: 'GPIO 21' },
    { value: 'esp32_02/gpio/22/set', label: 'GPIO 22' },
    { value: 'esp32_02/gpio/23/set', label: 'GPIO 23' },
    { value: 'esp32_02/gpio/25/set', label: 'GPIO 25' },
    { value: 'esp32_02/gpio/26/set', label: 'GPIO 26' },
    { value: 'esp32_02/gpio/27/set', label: 'GPIO 27' },
    { value: 'esp32_02/gpio/32/set', label: 'GPIO 32' },
    { value: 'esp32_02/gpio/33/set', label: 'GPIO 33' }
  ];

  const gpioPinOptions = [
    { value: '2', label: 'GPIO 2' },
    { value: '4', label: 'GPIO 4' },
    { value: '5', label: 'GPIO 5' },
    { value: '13', label: 'GPIO 13' },
    { value: '14', label: 'GPIO 14' },
    { value: '16', label: 'GPIO 16' },
    { value: '17', label: 'GPIO 17' },
    { value: '18', label: 'GPIO 18' },
    { value: '19', label: 'GPIO 19' },
    { value: '21', label: 'GPIO 21' },
    { value: '22', label: 'GPIO 22' },
    { value: '23', label: 'GPIO 23' },
    { value: '25', label: 'GPIO 25' },
    { value: '26', label: 'GPIO 26' },
    { value: '27', label: 'GPIO 27' },
    { value: '32', label: 'GPIO 32' },
    { value: '33', label: 'GPIO 33' }
  ];

  const actionPayloadOptions = [
    { value: 'ON', label: 'Ligar (ON)' },
    { value: 'OFF', label: 'Desligar (OFF)' }
  ];

  useEffect(() => {
    let isMounted = true;

    const checkCredentials = async () => {
      const token = localStorage.getItem('session_token');
      if (!token) {
        if (isMounted) navigate('/');
        return;
      }

      const [username, password, deployment] = atob(token).split(':');

      try {
        await new Promise((resolve, reject) => {
          connectMQTT(username, password, deployment, (status) => {
            if (isMounted) setConnectionStatus(status);
            if (status.includes('Conectado')) {
              resolve();
            } else if (status.includes('Erro') || status.includes('Desconectado')) {
              reject(new Error(status));
            }
          }, (topic, message) => {
            if (!isMounted) return;
            if (topic === 'sistema/regras/lista') {
              setRules(JSON.parse(message));
            } else if (topic === 'sistema/dashboard/status') {
              const newStatus = JSON.parse(message);
              setStatus({
                device_status: newStatus.device_status || '-',
                dht11_temperature: newStatus.dht11_temperature !== undefined ? `${newStatus.dht11_temperature} °C` : '-',
                dht11_humidity: newStatus.dht11_humidity !== undefined ? `${newStatus.dht11_humidity} %` : '-',
                bmp280_temperature: newStatus.bmp280_temperature !== undefined ? `${newStatus.bmp280_temperature} °C` : '-',
                bmp280_pressure: newStatus.bmp280_pressure !== undefined ? `${newStatus.bmp280_pressure} hPa` : '-',
                bmp280_sea_level_pressure: newStatus.bmp280_sea_level_pressure !== undefined ? `${newStatus.bmp280_sea_level_pressure} hPa` : '-',
                mq135_ppm: newStatus.mq135_ppm !== undefined ? `${newStatus.mq135_ppm} ppm` : '-',
                ldr_raw: newStatus.ldr_raw !== undefined ? newStatus.ldr_raw : '-',
                last_update: newStatus.last_update ? new Date(newStatus.last_update).toLocaleTimeString() : '-'
              });
            } else {
              setControlledGpios(prev => {
                const gpioStatus = prev.map(gpio => {
                  const topicFormat = `esp32_02/gpio/${gpio.pin}/state`;
                  if (topic === topicFormat) {
                    return { ...gpio, state: message.toString() };
                  }
                  return gpio;
                });
                return gpioStatus;
              });
            }
          });
        });

        if (isMounted) {
          subscribeToTopic('sistema/regras/lista');
          subscribeToTopic('sistema/dashboard/status');
          await publishMessage('sistema/regras/gerenciar', JSON.stringify({ command: "get_list" }));
        }
      } catch (err) {
        if (isMounted) {
          setConnectionStatus('Erro de conexão MQTT. Verifique o console.');
          console.error('Erro ao conectar MQTT:', err);
        }
      }
    };

    const timer = setTimeout(checkCredentials, 100);
    return () => {
      isMounted = false;
      clearTimeout(timer);
      disconnectMQTT();
    };
  }, [navigate]);

  const handleLogout = () => {
    localStorage.removeItem('session_token');
    disconnectMQTT();
    navigate('/');
  };

  const handleAddGpio = (e) => {
    e.preventDefault();
    const pin = e.target.elements['gpio-pin'].value;
    if (pin && !controlledGpios.some(gpio => gpio.pin === pin)) {
      setControlledGpios(prev => [...prev, { pin, state: '-' }]);
      subscribeToTopic(`esp32_02/gpio/${pin}/state`);
      e.target.reset();
    } else if (controlledGpios.some(gpio => gpio.pin === pin)) {
      alert(`GPIO ${pin} já está sendo controlado.`);
    }
  };

  const handleManualControl = (pin, command) => {
    publishMessage(`esp32_02/gpio/${pin}/set`, command)
      .catch(err => {
        console.error('Erro ao publicar comando:', err);
        alert('Erro ao enviar comando. Verifique a conexão MQTT.');
      });
  };

  const handleAddRule = (e) => {
    e.preventDefault();
    const rule = {
      command: "add_rule",
      rule: {
        name: e.target.elements['rule-name'].value,
        measurement: e.target.elements['rule-measurement'].value,
        field: e.target.elements['rule-field'].value,
        filter: e.target.elements['rule-filter'].value,
        range: e.target.elements['rule-range'].value,
        operator: e.target.elements['rule-operator'].value,
        threshold: parseFloat(e.target.elements['rule-threshold'].value),
        action_topic: e.target.elements['rule-action-topic'].value,
        action_payload: e.target.elements['rule-action-payload'].value
      }
    };
    publishMessage('sistema/regras/gerenciar', JSON.stringify(rule))
      .then(() => e.target.reset())
      .catch(err => {
        console.error('Erro ao adicionar regra:', err);
        alert('Erro ao salvar regra. Verifique a conexão MQTT.');
      });
  };

  const handleDeleteRule = (ruleId) => {
    if (window.confirm('Tem certeza que deseja deletar esta regra?')) {
      publishMessage('sistema/regras/gerenciar', JSON.stringify({
        command: "delete_rule",
        rule_id: ruleId
      }))
        .catch(err => {
          console.error('Erro ao deletar regra:', err);
          alert('Erro ao deletar regra. Verifique a conexão MQTT.');
        });
    }
  };

  return (
    <div className="container mx-auto p-4 md:p-8 bg-gray-900 text-gray-200">
      <header className="mb-8">
        <div className="flex justify-between items-center">
          <h1 className="text-4xl font-bold text-white">Gerenciador de Automação</h1>
          <button onClick={handleLogout} className="btn btn-red">Logout</button>
        </div>
        <p className={`text-${connectionStatus.includes('Conectado') ? 'green' : connectionStatus.includes('Erro') ? 'red' : 'yellow'}-400`}>
          {connectionStatus}
        </p>
      </header>

      <div className="mb-8">
        <h2 className="text-2xl font-semibold mb-4 text-white">Adicionar GPIO para Controle</h2>
        <div className="bg-gray-800 rounded-lg p-6 shadow-lg">
          <form onSubmit={handleAddGpio} className="flex flex-col sm:flex-row gap-4">
            <div>
              <label htmlFor="gpio-pin" className="block mb-2 font-medium">Número do GPIO</label>
              <select id="gpio-pin" className="form-input" required>
                {gpioPinOptions.map(option => (
                  <option key={option.value} value={option.value}>{option.label}</option>
                ))}
              </select>
            </div>
            <div className="self-end">
              <button type="submit" className="btn btn-blue">Adicionar GPIO</button>
            </div>
          </form>
        </div>
      </div>

      <div className="mb-8">
        <h2 className="text-2xl font-semibold mb-4 text-white">Status ao Vivo</h2>
        <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 lg:grid-cols-5 gap-4">
          {controlledGpios.map(gpio => (
            <div key={gpio.pin} className="status-card">
              <p className="text-sm text-gray-400">Estado GPIO {gpio.pin}</p>
              <p className={`text-2xl font-bold ${gpio.state === 'ON' ? 'text-green-400' : gpio.state === 'OFF' ? 'text-red-500' : 'text-white'}`}>
                {gpio.state}
              </p>
            </div>
          ))}
          <div className="status-card">
            <p className="text-sm text-gray-400">Status ESP32_02</p>
            <p className={`text-xl font-bold ${status.device_status.includes('online') || status.device_status.includes('heartbeat') ? 'text-green-400' : 'text-red-500'}`}>
              {status.device_status}
            </p>
          </div>
          <div className="status-card">
            <p className="text-sm text-gray-400">Temp. (DHT11)</p>
            <p className="text-2xl font-bold text-white">{status.dht11_temperature}</p>
          </div>
          <div className="status-card">
            <p className="text-sm text-gray-400">Umidade (DHT11)</p>
            <p className="text-2xl font-bold text-white">{status.dht11_humidity}</p>
          </div>
          <div className="status-card">
            <p className="text-sm text-gray-400">Temp. (BMP280)</p>
            <p className="text-2xl font-bold text-white">{status.bmp280_temperature}</p>
          </div>
          <div className="status-card">
            <p className="text-sm text-gray-400">Pressão Abs.</p>
            <p className="text-2xl font-bold text-white">{status.bmp280_pressure}</p>
          </div>
          <div className="status-card">
            <p className="text-sm text-gray-400">Pressão (Mar)</p>
            <p className="text-2xl font-bold text-white">{status.bmp280_sea_level_pressure}</p>
          </div>
          <div className="status-card">
            <p className="text-sm text-gray-400">Qualidade do Ar</p>
            <p className="text-2xl font-bold text-white">{status.mq135_ppm}</p>
          </div>
          <div className="status-card">
            <p className="text-sm text-gray-400">Luminosidade</p>
            <p className="text-2xl font-bold text-white">{status.ldr_raw}</p>
          </div>
          <div className="status-card">
            <p className="text-sm text-gray-400">Última Atualização</p>
            <p className="text-xl font-bold text-white">{status.last_update}</p>
          </div>
        </div>
      </div>

      <div className="mb-8">
        <h2 className="text-2xl font-semibold mb-4 text-white">Controle Manual</h2>
        <div className="space-y-4">
          {controlledGpios.length === 0 ? (
            <p className="text-gray-400">Nenhum GPIO configurado. Adicione um acima.</p>
          ) : (
            controlledGpios.map(gpio => (
              <div key={gpio.pin} className="bg-gray-800 rounded-lg p-6 shadow-lg">
                <div className="flex flex-col sm:flex-row justify-between items-center">
                  <h3 className="text-xl font-bold mb-4 sm:mb-0">GPIO {gpio.pin}</h3>
                  <div className="grid grid-cols-2 gap-2">
                    <button onClick={() => handleManualControl(gpio.pin, 'ON')} className="btn btn-green">ON</button>
                    <button onClick={() => handleManualControl(gpio.pin, 'OFF')} className="btn btn-red">OFF</button>
                  </div>
                </div>
              </div>
            ))
          )}
        </div>
      </div>

      <div className="bg-gray-800 rounded-lg p-6 shadow-lg mb-8">
        <h2 className="text-2xl font-semibold mb-4 text-white">Criar Nova Regra de Automação</h2>
        <form onSubmit={handleAddRule} className="grid grid-cols-1 md:grid-cols-2 gap-6">
          <div className="md:col-span-2">
            <label htmlFor="rule-name" className="block mb-2 font-medium">Nome da Regra</label>
            <input type="text" id="rule-name" className="form-input" placeholder="Digite o nome da regra" required />
          </div>
          <h3 className="md:col-span-2 text-lg font-semibold mt-4 border-b border-gray-600 pb-2">CONDIÇÃO (SE)</h3>
          <div>
            <label htmlFor="rule-measurement" className="block mb-2 font-medium">Measurement</label>
            <select
              id="rule-measurement"
              className="form-input"
              value={selectedMeasurement}
              onChange={(e) => setSelectedMeasurement(e.target.value)}
              required
            >
              {measurementOptions.map(option => (
                <option key={option.value} value={option.value}>{option.label}</option>
              ))}
            </select>
          </div>
          <div>
            <label htmlFor="rule-field" className="block mb-2 font-medium">Field</label>
            <select id="rule-field" className="form-input" required>
              {fieldOptions[selectedMeasurement].map(option => (
                <option key={option.value} value={option.value}>{option.label}</option>
              ))}
            </select>
          </div>
          <div>
            <label htmlFor="rule-filter" className="block mb-2 font-medium">Filtro de Tag (Opcional)</label>
            <select id="rule-filter" className="form-input">
              {filterOptions.map(option => (
                <option key={option.value} value={option.value}>{option.label}</option>
              ))}
            </select>
          </div>
          <div>
            <label htmlFor="rule-range" className="block mb-2 font-medium">Intervalo de Tempo (InfluxQL)</label>
            <select id="rule-range" className="form-input" required>
              {rangeOptions.map(option => (
                <option key={option.value} value={option.value}>{option.label}</option>
              ))}
            </select>
          </div>
          <div className="grid grid-cols-2 gap-4">
            <div>
              <label htmlFor="rule-operator" className="block mb-2 font-medium">Operador</label>
              <select id="rule-operator" className="form-input">
                <option value=">"> {'>'} (Maior que)</option>
                <option value="<"> {'<'} (Menor que)</option>
                <option value="=="> == (Igual a)</option>
              </select>
            </div>
            <div>
              <label htmlFor="rule-threshold" className="block mb-2 font-medium">Valor Limite</label>
              <input type="number" step="any" id="rule-threshold" className="form-input" placeholder="28.5" required />
            </div>
          </div>
          <h3 className="md:col-span-2 text-lg font-semibold mt-4 border-b border-gray-600 pb-2">AÇÃO (ENTÃO)</h3>
          <div>
            <label htmlFor="rule-action-topic" className="block mb-2 font-medium">Tópico MQTT da Ação</label>
            <select id="rule-action-topic" className="form-input" required>
              {actionTopicOptions.map(option => (
                <option key={option.value} value={option.value}>{option.label}</option>
              ))}
            </select>
          </div>
          <div>
            <label htmlFor="rule-action-payload" className="block mb-2 font-medium">Payload da Ação</label>
            <select id="rule-action-payload" className="form-input" required>
              {actionPayloadOptions.map(option => (
                <option key={option.value} value={option.value}>{option.label}</option>
              ))}
            </select>
          </div>
          <div className="md:col-span-2 text-right">
            <button type="submit" className="btn btn-blue">Salvar Regra</button>
          </div>
        </form>
      </div>

      <div className="bg-gray-800 rounded-lg p-6 shadow-lg">
        <h2 className="text-2xl font-semibold mb-4 text-white">Regras Ativas</h2>
        <div className="space-y-4">
          {rules.length === 0 ? (
            <p className="text-gray-400">Aguardando lista de regras do sistema...</p>
          ) : (
            rules.map(rule => (
              <div key={rule.id} className="bg-gray-700 p-4 rounded-md flex justify-between items-center">
                <div>
                  <p className="font-bold text-lg text-white">{rule.name}</p>
                  <p className="text-sm text-gray-300 font-mono">
                    SE media({rule.measurement}.{rule.field}) {rule.operator} {rule.threshold} NOS ÚLTIMOS {rule.range}
                  </p>
                  <p className="text-sm text-gray-400 font-mono">
                    ENTÃO -{'>'} Tópico: {rule.action_topic}, Mensagem: "{rule.action_payload}"
                  </p>
                </div>
                <button onClick={() => handleDeleteRule(rule.id)} className="btn btn-red">Deletar</button>
              </div>
            ))
          )}
        </div>
      </div>
    </div>
  );
}