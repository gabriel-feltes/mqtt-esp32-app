import supabase from './supabaseClient.js';

export async function insertLog({ topic, message, user }) {
  const { data, error } = await supabase.from('mqtt_logs').insert([{ topic, message, user }]);

  if (error) {
    console.error('Erro ao inserir no Supabase:', error);
    throw error;
  }

  return data;
}