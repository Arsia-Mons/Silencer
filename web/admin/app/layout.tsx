import './globals.css';
import type { Metadata } from 'next';
import type { ReactNode } from 'react';
import Starfield from '../components/Starfield';

export const metadata: Metadata = { title: 'Silencer Admin', description: 'Game server dashboard' };

export default function RootLayout({ children }: { children: ReactNode }) {
  return (
    <html lang="en">
      <body>
        <Starfield />
        <div id="app-root" style={{ position: 'relative', zIndex: 2, minHeight: '100vh' }}>
          {children}
        </div>
      </body>
    </html>
  );
}
