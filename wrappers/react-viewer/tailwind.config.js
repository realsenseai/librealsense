/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        'rs-blue': '#0071c5',
        'rs-dark': '#1a1a2e',
        'rs-darker': '#16213e',
        'rs-light': '#e8e8e8',
      },
    },
  },
  plugins: [],
}
