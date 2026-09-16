import React from 'react'
import ReactDOM from 'react-dom/client'
import App from './App'
import { useAppStore } from './store'
import './index.css'

// A development-only handle on the store, so a browser console (or a test) can see what the
// viewer believes about the cameras without adding logging to every action.
if (import.meta.env.DEV) {
  ;(window as unknown as { __rsStore?: unknown }).__rsStore = useAppStore
}

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
)
