/** @type {import('tailwindcss').Config} */
module.exports = {
  content: ['./app/**/*.{js,jsx}', './components/**/*.{js,jsx}', './lib/**/*.{js,jsx}'],
  theme: {
    extend: {
      colors: {
        // Silencer game palette (extracted from game icon)
        game: {
          bg:       '#050a05',
          bgCard:   'rgba(10,18,10,0.88)',
          bgHover:  'rgba(15,26,15,0.92)',
          border:   '#1a2e1a',
          primary:  '#00a328',
          dark:     '#005b1c',
          light:    '#0fa835',
          muted:    '#4a7a4a',
          text:     '#d1fad7',
          textDim:  '#7ab87a',
          danger:   '#ef4444',
          warning:  '#f59e0b',
          info:     '#22d3ee',
        },
      },
      fontFamily: {
        mono: ['"Courier New"', 'Courier', 'monospace'],
      },
    },
  },
  plugins: [],
};
