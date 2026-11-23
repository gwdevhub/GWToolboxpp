import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import tailwindcss from '@tailwindcss/vite';
import { viteSingleFile } from 'vite-plugin-singlefile';

// The build is one self-contained index.html that the launcher embeds. `npm run dev` serves the modules live instead,
// for a debug launcher run with GWL_DEV_URL set to this address.
export default defineConfig(({ command }) => ({
  plugins: [react(), tailwindcss(), ...(command === 'build' ? [viteSingleFile()] : [])],
  server: { host: '127.0.0.1', port: 5173, strictPort: true },
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    assetsInlineLimit: 100_000_000,
  },
}));
