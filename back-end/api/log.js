import { supabase } from '../lib/supabaseClient.js';

export default async function handler(req, res) {
  // Permitir CORS para todos os métodos que o backend suporta
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

  if (req.method === 'OPTIONS') {
    // Responde ao preflight OPTIONS
    return res.status(200).end();
  }

  if (req.method === 'POST') {
    try {
      const { topic, message, user } = req.body;

      if (!topic || !message || !user) {
        return res.status(400).json({ error: 'Missing required fields' });
      }

      const { data, error } = await supabase
        .from('mqtt_logs')
        .insert([{ topic, message, user }]);

      if (error) {
        return res.status(500).json({ error: error.message });
      }

      return res.status(200).json({ message: 'Log inserted', data });
    } catch (err) {
      console.error('Erro interno:', err);
      return res.status(500).json({ error: 'Internal server error' });
    }
  }

  if (req.method === 'GET') {
    try {
      const { data, error } = await supabase
        .from('mqtt_logs')
        .select('id, created_at, topic, message, user')
        .order('created_at', { ascending: false });

      if (error) {
        return res.status(500).json({ error: error.message });
      }

      return res.status(200).json(data);
    } catch (err) {
      console.error('Erro ao buscar logs:', err);
      return res.status(500).json({ error: 'Internal server error' });
    }
  }

  // Método não permitido
  return res.status(405).json({ error: 'Method not allowed' });
}