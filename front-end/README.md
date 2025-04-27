# Aplicativo de Controle ESP32 via MQTT - Documentação Técnica

## 🌳 Listagem dos arquivos feitos para edição direta

```bash
mqtt-esp32-app/
├── package.json            # Configuração do projeto e dependências
├── public/                 # Arquivos públicos estáticos
│   ├── favicon.ico         # Ícone do aplicativo
│   ├── index.html          # Template HTML principal
│   ├── logo192.png         # Logo 192x192
│   ├── logo512.png         # Logo 512x512
│   └── manifest.json       # Configuração PWA
└── src/                    # Código-fonte do aplicativo
    ├── App.css             # Estilos do componente principal
    ├── App.js              # Componente principal da aplicação
    ├── index.css           # Estilos globais
    ├── index.js            # Ponto de entrada do React
    ├── Login.js            # Componente de autenticação
    └── mqttService.js      # Serviço de conexão MQTT
  
```

## 🛠️ Componentes Principais

### `src/mqttService.js`

Módulo responsável pela comunicação com o broker MQTT. Oferece:

- Conexão segura via WebSocket com TLS
- Publicação e subscrição de tópicos
- Gerenciamento de status de conexão
- Monitoramento do dispositivo ESP32

### `src/App.js`

Componente principal que inclui:

- Interface de controle do LED
- Exibição do status da conexão
- Gerenciamento do estado da aplicação
- Integração com o serviço MQTT

### `src/Login.js`

Componente de autenticação que:

- Valida credenciais do usuário
- Armazena dados no localStorage
- Redireciona para a página principal

## 🔌 Configuração MQTT

Para configurar a conexão com seu broker MQTT, edite `src/mqttService.js`:

```javascript
const ESP32_STATUS_TOPIC = 'esp32/status';      // Tópico para status do dispositivo
const MQTT_LED_TOPIC = 'esp32/gpio/led';       // Tópico para controle do LED
const ESP32_TIMEOUT_MS = 10000;                // Tempo limite para considerar offline
```

## 🚀 Como Executar

1. Instale as dependências:

```bash
npm install
```

2. Inicie o servidor de desenvolvimento:

```bash
npm start
```

3. Acesse no navegador:

```bash
http://localhost:3000
```

## 🧪 Testes

Execute os testes com:

```bash
npm test
```

## 📦 Build para Produção

Para criar uma versão otimizada:

```bash
npm run build
```

## 🔄 Fluxo de Dados

1. **Login**:
   - Credenciais armazenadas no localStorage
   - Redirecionamento para a página principal

2. **Conexão MQTT**:
   - Estabelece conexão WebSocket segura
   - Subscreve aos tópicos necessários
   - Inicia monitoramento de status

3. **Controle do ESP32**:
   - Publica mensagens nos tópicos MQTT
   - Recebe atualizações de status em tempo real

## 🎨 Estilos

O aplicativo utiliza:

- CSS modular por componente
- Classes dinâmicas baseadas no estado
- Layout responsivo

## ⚠️ Solução de Problemas

Problemas comuns:

- **Conexão MQTT falha**: Verifique credenciais e certificados TLS
- **Dispositivo não responde**: Confira se o ESP32 está conectado ao broker
- **Erros de build**: Limpe cache com `npm cache clean --force` e reinstale dependências