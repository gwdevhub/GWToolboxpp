/**
 * Fetches the latest GWToolbox++ release from GitHub at build time so the
 * download links always point at the freshest assets. The page picks the
 * Windows or Linux link for each visitor in the browser (see download-links.ts).
 *
 * Falls back to the pinned URL in site.config.json if the API call fails
 * (rate limit, network) so the build never breaks.
 */
import siteConfig from '../../site.config.json' with { type: 'json' };

export type Downloads = {
  windows: string;
  linux: string;
};

export type LatestRelease = {
  version: string;
  downloads: Downloads;
  publishedAt: string | null;
  source: 'github' | 'fallback';
};

const RELEASES_PAGE = 'https://github.com/gwdevhub/GWToolboxpp/releases/latest';

const FALLBACK: LatestRelease = {
  version: siteConfig.toolboxVersion,
  downloads: {
    windows: siteConfig.fallbackDownloadUrl,
    linux: RELEASES_PAGE,
  },
  publishedAt: null,
  source: 'fallback',
};

const WINDOWS_ASSET = 'GWToolbox.exe';
const LINUX_ASSET = 'GWToolbox.AppImage';

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
      html_url?: string;
      published_at?: string;
      assets?: { name: string; browser_download_url: string }[];
    };
    const assetUrl = (name: string) =>
      latest.assets?.find((a) => a.name === name)?.browser_download_url;
    // A missing asset sends that platform to the release page instead. DLL-only
    // `*_Release` tags don't carry the installer, so Windows keeps the stable URL.
    const releasePage = latest.html_url ?? RELEASES_PAGE;
    // Strip the conventional "_Release" / "_Exe" suffix from the tag for display.
    const version = (latest.name ?? latest.tag_name ?? FALLBACK.version)
      .replace(/_Release$/i, '')
      .replace(/_Exe$/i, '');
    return {
      version,
      downloads: {
        windows: assetUrl(WINDOWS_ASSET) ?? FALLBACK.downloads.windows,
        linux: assetUrl(LINUX_ASSET) ?? releasePage,
      },
      publishedAt: latest.published_at ?? null,
      source: 'github',
    };
  } catch {
    return FALLBACK;
  }
}
