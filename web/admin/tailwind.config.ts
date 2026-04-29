import type { Config } from 'tailwindcss';

const config: Config = {
  content: ['./app/**/*.{js,jsx,ts,tsx}', './components/**/*.{js,jsx,ts,tsx}', './lib/**/*.{js,jsx,ts,tsx}'],
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
        mono: ['"Silencer UI"', '"Courier New"', 'Courier', 'monospace'],
        'silencer-title': ['"Silencer Title"', '"Courier New"', 'monospace'],
        'silencer-large': ['"Silencer UI Large"', '"Courier New"', 'monospace'],
        'silencer-tiny': ['"Silencer Tiny"', '"Courier New"', 'monospace'],
      },
    },
  },
  plugins: [],
};

export default config;
