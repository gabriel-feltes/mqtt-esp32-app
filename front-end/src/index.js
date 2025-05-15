import React from 'react'
import ReactDOM from 'react-dom/client'
import { BrowserRouter, Routes, Route } from 'react-router-dom'
import App from './pages/App'
import Login from './pages/Login'
import Logs from './pages/Logs'

ReactDOM.createRoot(document.getElementById('root')).render(
  <BrowserRouter basename="/mqtt-esp32-app">
    <Routes>
      <Route path="/" element={<Login />} />
      <Route path="/app" element={<App />} />
      <Route path="/log" element={<Logs />} />
    </Routes>
  </BrowserRouter>
)
