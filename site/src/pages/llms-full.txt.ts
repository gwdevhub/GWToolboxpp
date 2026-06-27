import type { APIRoute } from 'astro';
import { orderedDocs, docToMarkdown, SITE } from '../lib/llms';
import siteConfig from '../../site.config.json';

// Every documentation page concatenated as plain Markdown, for one-shot ingestion.
export const GET: APIRoute = async () => {
  const docs = await orderedDocs();
  const out: string[] = [
    `# ${siteConfig.title} — full documentation`,
    '',
    `> ${siteConfig.tagline}. Generated from ${SITE}. Each section below is one documentation page.`,
    '',
  ];
  for (const entry of docs) {
    out.push(docToMarkdown(entry), '---', '');
  }
  return new Response(out.join('\n'), {
    headers: { 'Content-Type': 'text/plain; charset=utf-8' },
  });
};
