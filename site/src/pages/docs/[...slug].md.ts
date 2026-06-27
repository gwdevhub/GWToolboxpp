import type { APIRoute, GetStaticPaths } from 'astro';
import { getCollection } from 'astro:content';
import { docToMarkdown, type Doc } from '../../lib/llms';

// Serves each doc as raw Markdown at /docs/<slug>.md alongside the HTML page.
export const getStaticPaths: GetStaticPaths = async () => {
  const entries = await getCollection('docs', ({ data }) => !data.hidden);
  return entries.map((entry) => ({ params: { slug: entry.id }, props: { entry } }));
};

export const GET: APIRoute = ({ props }) => {
  const { entry } = props as { entry: Doc };
  return new Response(docToMarkdown(entry), {
    headers: { 'Content-Type': 'text/markdown; charset=utf-8' },
  });
};
