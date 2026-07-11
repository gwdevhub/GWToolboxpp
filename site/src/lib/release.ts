/**
 * Fetches the latest GWToolbox++ release from GitHub at build time so the
 * download button on the landing page always points at the freshest asset.
 *
 * Falls back to the pinned URL in site.config.json if the API call fails
 * (rate limit, network) so the build never breaks.
 */
import siteConfig from '../../site.config.json' with { type: 'json' };

export type LatestRelease = {
  version: string;
  url: string;
  publishedAt: string | null;
  source: 'github' | 'fallback';
};

const FALLBACK: LatestRelease = {
  version: siteConfig.toolboxVersion,
  url: siteConfig.fallbackDownloadUrl,
  publishedAt: null,
  source: 'fallback',
};

export async function getLatestRelease(): Promise<LatestRelease> {
  try {
    const headers: Record<string, string> = {
      Accept: 'application/vnd.github+json',
      'User-Agent': 'gwtoolbox-site-build',
    };
    if (process.env.GITHUB_TOKEN) {
      headers.Authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
    }
    // Use the dedicated latest-published endpoint rather than scanning the
    // paginated list: when the build token has push access GitHub returns the
    // repo's many draft releases first, crowding published ones out of any
    // reasonable page size. `/releases/latest` excludes drafts and prereleases.
    const res = await fetch(
      'https://api.github.com/repos/gwdevhub/GWToolboxpp/releases/latest',
      { headers },
    );
    if (!res.ok) return FALLBACK;
    const latest = (await res.json()) as {
      tag_name?: string;
      name?: string;
      published_at?: string;
      assets?: { name: string; browser_download_url: string }[];
    };
    // Strip the conventional "_Release" / "_Exe" suffix from the tag for display.
    const version = (latest.name ?? latest.tag_name ?? FALLBACK.version)
      .replace(/_Release$/i, '')
      .replace(/_Exe$/i, '');
    // DLL-only `*_Release` tags don't carry the installer; keep the stable
    // fallback download URL when the latest release ships no .exe.
    const exe = latest.assets?.find((a) =>
      a.name.toLowerCase().endsWith('.exe'),
    );
    return {
      version,
      url: exe?.browser_download_url ?? FALLBACK.url,
      publishedAt: latest.published_at ?? null,
      source: 'github',
    };
  } catch {
    return FALLBACK;
  }
}
