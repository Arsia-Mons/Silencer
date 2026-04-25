import './globals.css';

export const metadata = { title: 'Silencer Admin', description: 'Game server dashboard' };

export default function RootLayout({ children }) {
  return (
    <html lang="en">
      <body>
        <div id="app-root" style={{ position: 'relative', zIndex: 1, minHeight: '100vh' }}>
          {children}
        </div>
      </body>
    </html>
  );
}
