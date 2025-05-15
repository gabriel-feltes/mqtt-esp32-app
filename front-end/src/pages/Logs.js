// Logs.js
import { useEffect, useState } from 'react'

export default function Logs() {
  const [logs, setLogs] = useState([])

  useEffect(() => {
    const fetchLogs = async () => {
      try {
        const response = await fetch('https://back-end-gabriel-feltes-dos-santos-projects.vercel.app/log')
        const json = await response.json()

        const logsArray = Array.isArray(json) ? json : json.data
        if (Array.isArray(logsArray)) {
          setLogs(logsArray)
        } else {
          console.error('Formato inesperado:', json)
          setLogs([])
        }
      } catch (err) {
        console.error('Erro ao buscar logs:', err)
      }
    }

    fetchLogs()
    const interval = setInterval(fetchLogs, 3000)

    return () => clearInterval(interval)
  }, [])

  const formatDate = (isoString) => {
    const date = new Date(isoString)
    return date.toLocaleString('pt-BR') // Exibe data/hora no formato brasileiro
  }

  return (
    <div style={{ padding: '1rem' }}>
      <h2>Logs MQTT</h2>
      {logs.length === 0 ? (
        <p>Nenhum log disponível.</p>
      ) : (
        <table border="1" cellPadding="8" cellSpacing="0" style={{ width: '100%', borderCollapse: 'collapse' }}>
          <thead>
            <tr>
              <th>ID</th>
              <th>Data/Hora</th>
              <th>Tópico</th>
              <th>Mensagem</th>
              <th>Usuário</th>
            </tr>
          </thead>
          <tbody>
            {logs.map(log => (
              <tr key={log.id}>
                <td>{log.id}</td>
                <td>{formatDate(log.created_at)}</td>
                <td>{log.topic}</td>
                <td>{log.message}</td>
                <td>{log.user}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </div>
  )
}