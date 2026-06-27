import { getCollection, type CollectionEntry } from 'astro:content';
import { navGroups } from './nav';

export type Doc = CollectionEntry<'docs'>;

// Keep in sync with `site` in astro.config.mjs.
export const SITE = 'https://www.gwtoolbox.com';

/** Non-hidden docs in nav order, with any pages missing from the nav appended. */
export async function orderedDocs(): Promise<Doc[]> {
  const entries = await getCollection('docs', ({ data }) => !data.hidden);
  const byId = new Map(entries.map((e) => [e.id, e]));
  const ordered: Doc[] = [];
  const seen = new Set<string>();

  for (const group of navGroups) {
    for (const item of group.items) {
      const entry = byId.get(item.slug);
      if (entry && !seen.has(entry.id)) {
        ordered.push(entry);
        seen.add(entry.id);
      }
    }
  }
  for (const entry of entries) {
    if (!seen.has(entry.id)) ordered.push(entry);
  }
  return ordered;
}

/** Replace inline <ToolboxPath steps={[...]} /> chips with a `A › B › C` breadcrumb. */
function inlineToolboxPaths(md: string): string {
  return md.replace(/<ToolboxPath\b[\s\S]*?\/>/g, (tag) => {
    const stepsBlock = tag.match(/steps=\{\[([\s\S]*?)\]\}/)?.[1] ?? '';
    const steps = [...stepsBlock.matchAll(/['"]([^'"]+)['"]/g)].map((m) => m[1]);
    return steps.length ? `\`${steps.join(' › ')}\`` : '';
  });
}

/** Raw markdown body with MDX imports, components, and `[back]` boilerplate stripped for clean ingestion. */
export function cleanBody(entry: Doc): string {
  return inlineToolboxPaths(entry.body ?? '')
    .split('\n')
    .filter((line) => !/^\s*import\s.+from\s.+;?\s*$/.test(line))
    .join('\n')
    .replace(/^\s*\[back\]\(\.\/\)\s*$/gm, '')
    .replace(/\n{3,}/g, '\n\n')
    .trim();
}

/** A page rendered as standalone markdown, with a source link for provenance. */
export function docToMarkdown(entry: Doc): string {
  return [`# ${entry.data.title}`, '', `Source: ${SITE}/docs/${entry.id}/`, '', cleanBody(entry), ''].join('\n');
}
