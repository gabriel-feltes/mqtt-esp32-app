import { useState } from 'react'
import { useNavigate } from 'react-router-dom'
import './App.css'

export default function Login() {
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [deployment, setDeployment] = useState('')
  const navigate = useNavigate()

  const handleSubmit = (e) => {
    e.preventDefault()
    localStorage.setItem('mqtt_username', username)
    localStorage.setItem('mqtt_password', password)
    localStorage.setItem('mqtt_deployment', deployment)
    navigate('/app')
  }

  return (
    <form onSubmit={handleSubmit}>
      <h2>Login MQTT</h2>
      <input 
        id="username" 
        placeholder="Usuário" 
        value={username} 
        onChange={e => setUsername(e.target.value)} 
        required 
      />
      <input 
        id="password" 
        placeholder="Senha" 
        value={password} 
        onChange={e => setPassword(e.target.value)} 
        required 
        type="password" 
      />
      <input 
        id="deployment" 
        placeholder="Deployment ID" 
        value={deployment} 
        onChange={e => setDeployment(e.target.value)} 
        required 
      />
      <button id="onBtn" type="submit">Entrar</button>
    </form>
  )
}