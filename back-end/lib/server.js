import dotenv from 'dotenv';

console.log('MQTT_BROKER:', process.env.MQTT_BROKER_URL);
console.log('SUPABASE_URL:', process.env.SUPABASE_URL);
console.log('SUPABASE_KEY:', process.env.SUPABASE_KEY);

// Inicie seu app normal aqui
import './index.js';
