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
        'rs-accent': '#2f97e3',
        // Surfaces, from page background outwards to nested blocks and inputs.
        'rs-darker': '#0e1220',
        'rs-dark': '#151b2b',
        'rs-inset': '#1b2233',
        'rs-border': '#29314a',
        // Text ramp: primary / secondary / disabled.
        'rs-light': '#f2f4f9',
        'rs-text': '#f2f4f9',
        'rs-muted': '#bcc4d4',
        'rs-dim': '#8e97ab',
        // Reserved for state only, never for identity.
        'rs-ok': '#3fb950',
        'rs-warn': '#d5a021',
        'rs-err': '#f0574f',
      },
    },
  },
  plugins: [],
}
