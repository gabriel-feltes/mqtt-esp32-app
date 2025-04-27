import React from 'react'
import ReactDOM from 'react-dom/client'
import { BrowserRouter, Routes, Route } from 'react-router-dom'
import App from './App'
import Login from './Login'

ReactDOM.createRoot(document.getElementById('root')).render(
  <BrowserRouter basename="/mqtt-esp32-app">
    <Routes>
      <Route path="/" element={<Login />} />
      <Route path="/app" element={<App />} />
    </Routes>
  </BrowserRouter>
)
