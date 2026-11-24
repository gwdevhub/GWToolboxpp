export type VisitorOs = 'windows' | 'linux' | 'other';

export function visitorOs(): VisitorOs {
  const nav = navigator as Navigator & { userAgentData?: { platform?: string } };
  const ua = navigator.userAgent;
  if (/Android/i.test(ua)) return 'other';
  const platform = nav.userAgentData?.platform ?? navigator.platform ?? '';
  if (/^Win/i.test(platform) || /Windows/i.test(ua)) return 'windows';
  if (/^Linux|X11/i.test(platform) || /Linux|X11/i.test(ua)) return 'linux';
  return 'other';
}

export function applyDownloadLinks(): void {
  const os = visitorOs();
  const isLinux = os === 'linux';

  document.querySelectorAll<HTMLAnchorElement>('[data-download-primary]').forEach((link) => {
    link.href = isLinux ? (link.dataset.linux ?? '') : (link.dataset.windows ?? '');
  });

  document.querySelectorAll<HTMLElement>('[data-download-label]').forEach((label) => {
    const version = label.dataset.version ?? '';
    const base = version ? `Download ${version}` : 'Download';
    label.textContent = isLinux ? `${base} for Linux` : base;
  });

  document.querySelectorAll<HTMLElement>('[data-platform-note]').forEach((note) => {
    note.textContent = isLinux ? 'Native Linux build.' : 'Windows only.';
  });
}
