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
    // Two-pass lookup over recent releases:
    //   - Latest entry overall → the "version" label users see.
    //   - Most recent entry that ships an .exe asset → the actual download URL,
    //     because DLL-only `*_Release` tags don't carry the installer.
    const res = await fetch(
      'https://api.github.com/repos/gwdevhub/GWToolboxpp/releases?per_page=30',
      { headers },
    );
    if (!res.ok) return FALLBACK;
    const releases = (await res.json()) as {
      tag_name?: string;
      name?: string;
      published_at?: string;
      draft?: boolean;
      prerelease?: boolean;
      assets?: { name: string; browser_download_url: string }[];
    }[];
    const published = releases.filter((r) => !r.draft && !r.prerelease);
    if (published.length === 0) return FALLBACK;
    const latest = published[0];
    const exeRelease = published.find((r) =>
      r.assets?.some((a) => a.name.toLowerCase().endsWith('.exe')),
    );
    const exe = exeRelease?.assets?.find((a) =>
      a.name.toLowerCase().endsWith('.exe'),
    );
    if (!exe) return FALLBACK;
    // Strip the conventional "_Release" / "_Exe" suffix from the tag for display.
    const version = (latest.name ?? latest.tag_name ?? FALLBACK.version)
      .replace(/_Release$/i, '')
      .replace(/_Exe$/i, '');
    return {
      version,
      url: exe.browser_download_url,
      publishedAt: latest.published_at ?? null,
      source: 'github',
    };
  } catch {
    return FALLBACK;
  }
}
