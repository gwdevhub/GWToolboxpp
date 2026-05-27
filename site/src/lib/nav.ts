/**
 * Navigation manifest — the order pages appear in the sidebar.
 * Each entry is a slug from src/content/docs. Slugs not listed here
 * still resolve, they just don't appear in the sidebar groups.
 */
export type NavGroup = {
  label: string;
  items: { slug: string; label: string }[];
};

export const navGroups: NavGroup[] = [
  {
    label: 'Getting Started',
    items: [
      { slug: 'faq', label: 'FAQ' },
      { slug: 'linux', label: 'Linux Install Guide' },
      { slug: 'history', label: 'Version History' },
      { slug: 'credits', label: 'Credits' },
      { slug: 'donate', label: 'Donate' },
    ],
  },
  {
    label: 'Windows',
    items: [
      { slug: 'windows', label: 'Overview' },
      { slug: 'builds', label: 'Builds' },
      { slug: 'herobuilds', label: 'Hero Builds' },
      { slug: 'hotkeys', label: 'Hotkeys' },
      { slug: 'info', label: 'Info' },
      { slug: 'materials', label: 'Materials' },
      { slug: 'pcons', label: 'Pcons' },
      { slug: 'settings', label: 'Settings' },
      { slug: 'travel', label: 'Travel' },
      { slug: 'trade', label: 'Trade' },
      { slug: 'market_browser', label: 'Market Browser' },
      { slug: 'completion', label: 'Completion' },
      { slug: 'armory_window', label: 'Armory' },
      { slug: 'duping_window', label: 'Duping' },
    ],
  },
  {
    label: 'Widgets',
    items: [
      { slug: 'widgets', label: 'Overview' },
      { slug: 'damage_monitor', label: 'Damage Monitor' },
      { slug: 'minimap', label: 'Minimap' },
      { slug: 'misc_widgets', label: 'Miscellaneous' },
      { slug: 'party_window', label: 'Party' },
      { slug: 'target_widgets', label: 'Target' },
      { slug: 'enemy_window', label: 'Enemy' },
      { slug: 'items', label: 'Items' },
      { slug: 'quest', label: 'Quest' },
      { slug: 'world_map', label: 'World Map' },
      { slug: 'vanquish_overlay', label: 'Vanquish Overlay' },
    ],
  },
  {
    label: 'Features',
    items: [
      { slug: 'camera', label: 'Camera' },
      { slug: 'chat', label: 'Chat' },
      { slug: 'chatfilter', label: 'Chat Filter' },
      { slug: 'chatlog', label: 'Chat Log' },
      { slug: 'commands', label: 'Commands' },
      { slug: 'text-to-speech', label: 'Text-to-Speech' },
      { slug: 'theme', label: 'Theme' },
      { slug: 'reroll', label: 'Reroll' },
      { slug: 'integrations', label: 'Integrations' },
      { slug: 'input_modules', label: 'Input' },
      { slug: 'plugins', label: 'Plugins' },
    ],
  },
];

export function findInNav(slug: string): { group: string; label: string } | null {
  for (const g of navGroups) {
    const item = g.items.find((i) => i.slug === slug);
    if (item) return { group: g.label, label: item.label };
  }
  return null;
}
