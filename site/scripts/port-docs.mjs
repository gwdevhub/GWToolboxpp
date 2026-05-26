#!/usr/bin/env node
/**
 * One-shot script that ports the legacy Jekyll markdown under ../docs/
 * into the Astro content collection. Idempotent — safe to re-run.
 *
 * For each source .md:
 *   1. Strip the `layout: default` Jekyll front matter.
 *   2. Derive a sensible title (first H1 in the file, falling back to slug).
 *   3. Rewrite relative links from `foo` and `foo#anchor` to
 *      `/docs/foo/` and `/docs/foo/#anchor` so they resolve under the new routes.
 *   4. Drop in a new front matter with title + section.
 *   5. Write to ../src/content/docs/<slug>.md.
 */
import { readdir, readFile, writeFile, mkdir } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const SRC_DIR = resolve(__dirname, '../../docs');
const DEST_DIR = resolve(__dirname, '../src/content/docs');

// Bucket each known page into a section for the schema. Anything not listed
// falls into 'features' by default.
const SECTION_MAP = {
  index: 'overview',
  faq: 'meta',
  credits: 'meta',
  history: 'meta',
  windows: 'windows',
  builds: 'windows',
  herobuilds: 'windows',
  dialogs: 'windows',
  hotkeys: 'windows',
  info: 'windows',
  materials: 'windows',
  pcons: 'windows',
  settings: 'windows',
  travel: 'windows',
  trade: 'windows',
  hallofmonuments: 'windows',
  completion: 'windows',
  armory_window: 'windows',
  duping_window: 'windows',
  widgets: 'widgets',
  damage_monitor: 'widgets',
  minimap: 'widgets',
  misc_widgets: 'widgets',
  party_window: 'widgets',
  target_widgets: 'widgets',
  enemy_window: 'widgets',
  items: 'widgets',
  quest: 'widgets',
};

// All recognised slugs — used to scope link rewrites so we don't accidentally
// linkify arbitrary tokens.
const KNOWN_SLUGS = new Set([
  'index', 'faq', 'credits', 'history',
  'windows', 'builds', 'herobuilds', 'dialogs', 'hotkeys', 'info', 'materials',
  'pcons', 'settings', 'travel', 'trade', 'hallofmonuments', 'completion',
  'armory_window', 'duping_window',
  'widgets', 'damage_monitor', 'minimap', 'misc_widgets', 'party_window',
  'target_widgets', 'enemy_window', 'items', 'quest',
  'camera', 'chat', 'chatfilter', 'chatlog', 'commands', 'text-to-speech',
  'theme', 'reroll', 'integrations', 'input_modules', 'plugins',
]);

function stripJekyllFrontMatter(text) {
  // Matches a leading `---\n...\n---\n` block.
  return text.replace(/^---\n[\s\S]*?\n---\n+/, '');
}

function extractTitle(body, slug) {
  const m = body.match(/^#\s+(.+?)\s*$/m);
  if (m) return m[1].trim();
  return slug
    .replace(/_/g, ' ')
    .replace(/\b\w/g, (c) => c.toUpperCase());
}

/**
 * Rewrite legacy Jekyll-style relative links. We handle three shapes:
 *   - Markdown:  [text](slug)           → [text](/docs/slug/)
 *   - Markdown:  [text](slug#anchor)    → [text](/docs/slug/#anchor)
 *   - HTML:      href="slug"            → href="/docs/slug/"
 *   - HTML:      href="slug#anchor"     → href="/docs/slug/#anchor"
 *
 * Only rewrites when the bare slug is in KNOWN_SLUGS — leaves external URLs
 * (http://...), anchor-only links (#foo), and unknown tokens alone.
 */
function rewriteLinks(text) {
  const slugAlternation = [...KNOWN_SLUGS].sort((a, b) => b.length - a.length).join('|');
  const mdLink = new RegExp(`\\]\\((${slugAlternation})(#[^)]*)?\\)`, 'g');
  const htmlLink = new RegExp(`href=["'](${slugAlternation})(#[^"']*)?["']`, 'g');
  text = text.replace(mdLink, (_m, slug, hash = '') => `](/docs/${slug}/${hash})`);
  text = text.replace(htmlLink, (_m, slug, hash = '') => `href="/docs/${slug}/${hash}"`);
  return text;
}

function buildFrontMatter({ title, section }) {
  return `---\ntitle: ${JSON.stringify(title)}\nsection: ${section}\n---\n\n`;
}

async function main() {
  if (!existsSync(SRC_DIR)) {
    console.error(`Source directory not found: ${SRC_DIR}`);
    process.exit(1);
  }
  await mkdir(DEST_DIR, { recursive: true });

  const files = (await readdir(SRC_DIR)).filter((f) => f.endsWith('.md'));
  let count = 0;
  for (const file of files) {
    if (file === 'index.md') continue; // landing page is hand-written
    const slug = file.replace(/\.md$/, '');
    const section = SECTION_MAP[slug] ?? 'features';
    const raw = await readFile(join(SRC_DIR, file), 'utf8');
    const stripped = stripJekyllFrontMatter(raw);
    const title = extractTitle(stripped, slug);

    // Remove the in-body H1 since the layout already renders one.
    const bodyNoH1 = stripped.replace(/^#\s+.+?\s*\n+/m, '');
    const rewritten = rewriteLinks(bodyNoH1);

    const out = buildFrontMatter({ title, section }) + rewritten.trimEnd() + '\n';
    await writeFile(join(DEST_DIR, file), out, 'utf8');
    count++;
  }
  console.log(`Ported ${count} doc pages → ${DEST_DIR}`);
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
