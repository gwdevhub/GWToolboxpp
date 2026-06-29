import type { APIRoute } from 'astro';
import { SITE } from '../lib/llms';

export const GET: APIRoute = () => {
  const body = [
    '# GWToolbox++ — https://www.gwtoolbox.com',
    'User-agent: *',
    'Allow: /',
    '',
    `Sitemap: ${SITE}/sitemap-index.xml`,
    `# LLM-readable docs index: ${SITE}/llms.txt`,
    `# Full docs as one file:    ${SITE}/llms-full.txt`,
    '',
  ].join('\n');
  return new Response(body, {
    headers: { 'Content-Type': 'text/plain; charset=utf-8' },
  });
};
