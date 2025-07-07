import { useState } from 'react';
import { useNavigate } from 'react-router-dom';
import '../styles/App.css';

export default function Login() {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [deployment, setDeployment] = useState('');
  const navigate = useNavigate();

  const handleSubmit = (e) => {
    e.preventDefault();
    const token = btoa(`${username}:${password}:${deployment}`);
    localStorage.setItem('session_token', token);
    navigate('/app');
  };

  return (
    <div className="container mx-auto p-4 bg-gray-900 text-gray-200">
      <form onSubmit={handleSubmit} className="flex flex-col gap-4 max-w-md mx-auto bg-gray-800 p-6 rounded-lg shadow-lg">
        <h2 className="text-2xl font-semibold text-white">Login MQTT</h2>
        <div>
          <label htmlFor="username" className="block mb-2 font-medium">Usuário</label>
          <input
            id="username"
            placeholder="Usuário"
            value={username}
            onChange={(e) => setUsername(e.target.value)}
            className="form-input"
            required
          />
        </div>
        <div>
          <label htmlFor="password" className="block mb-2 font-medium">Senha</label>
          <input
            id="password"
            placeholder="Senha"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            className="form-input"
            type="password"
            required
          />
        </div>
        <div>
          <label htmlFor="deployment" className="block mb-2 font-medium">Deployment ID</label>
          <input
            id="deployment"
            placeholder="Deployment ID"
            value={deployment}
            onChange={(e) => setDeployment(e.target.value)}
            className="form-input"
            required
          />
        </div>
        <button type="submit" className="btn btn-blue">Entrar</button>
      </form>
    </div>
  );
}