import { useEffect, useRef, useState } from 'react';

/**
 * Thin React wrapper around Pagefind's vanilla JS UI. We initialise on mount,
 * inside a dialog that opens with `/` or `⌘K`. Search itself is fully static —
 * Pagefind index lives under /pagefind/ and is built by `pagefind --site dist`.
 *
 * Two trigger styles:
 *   - `variant="button"` (default) — compact pill, used in the doc header.
 *   - `variant="hero"` — full-width fake input, used prominently on the home
 *     page where finding the right doc is the primary user task.
 */
type Props = {
  variant?: 'button' | 'hero';
  placeholder?: string;
};

export default function Search({ variant = 'button', placeholder = 'Search…' }: Props) {
  const [open, setOpen] = useState(false);
  const dialogRef = useRef<HTMLDivElement | null>(null);
  const mountRef = useRef<HTMLDivElement | null>(null);
  const initialised = useRef(false);

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      const isCmdK = (e.key === 'k' || e.key === 'K') && (e.metaKey || e.ctrlKey);
      const isSlash =
        e.key === '/' &&
        !(e.target instanceof HTMLInputElement) &&
        !(e.target instanceof HTMLTextAreaElement);
      if (isCmdK || isSlash) {
        e.preventDefault();
        setOpen((v) => !v);
      } else if (e.key === 'Escape') {
        setOpen(false);
      }
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, []);

  useEffect(() => {
    if (!open || initialised.current || !mountRef.current) return;
    initialised.current = true;
    (async () => {
      try {
        // Path is hidden from Vite's static analyser; the file only exists in
        // the deployed dist/ after `pagefind --site dist` runs.
        await import(/* @vite-ignore */ `${import.meta.env.BASE_URL}pagefind/pagefind-ui.js`);
        // @ts-expect-error — PagefindUI is attached to window by the import above
        new window.PagefindUI({
          element: mountRef.current,
          showImages: false,
          showSubResults: true,
          resetStyles: false,
        });
      } catch (err) {
        if (mountRef.current) {
          mountRef.current.innerHTML =
            '<p class="text-sm text-(--color-fg-meta) p-4">Search index not built yet. Run <code>npm run build</code>.</p>';
        }
        console.warn('Pagefind not available', err);
      }
    })();
  }, [open]);

  const searchIcon = (
    <svg viewBox="0 0 16 16" className="fill-current" aria-hidden="true">
      <path d="M11.742 10.344a6.5 6.5 0 1 0-1.397 1.398h-.001c.03.04.062.078.098.115l3.85 3.85a1 1 0 0 0 1.415-1.414l-3.85-3.85a1.007 1.007 0 0 0-.115-.1zM12 6.5a5.5 5.5 0 1 1-11 0 5.5 5.5 0 0 1 11 0z" />
    </svg>
  );

  return (
    <>
      {variant === 'hero' ? (
        <button
          type="button"
          onClick={() => setOpen(true)}
          aria-label="Search documentation"
          className="group flex w-full max-w-2xl items-center gap-3 rounded-lg border border-(--color-border-glass) bg-(--color-bg-elevated)/80 px-4 py-3 text-left text-base text-(--color-fg-meta) shadow-lg shadow-black/30 backdrop-blur-sm transition-colors hover:border-(--color-accent-deep) hover:bg-(--color-bg-elevated) hover:text-(--color-fg-sub)"
        >
          <span className="h-4 w-4 shrink-0 text-(--color-fg-meta) group-hover:text-(--color-accent)">{searchIcon}</span>
          <span className="flex-1 truncate">{placeholder}</span>
          <kbd className="hidden shrink-0 rounded border border-(--color-border-glass) bg-(--color-glass) px-2 py-0.5 font-mono text-xs text-(--color-fg-faint) sm:inline">
            /
          </kbd>
        </button>
      ) : (
        <button
          type="button"
          onClick={() => setOpen(true)}
          className="inline-flex items-center gap-2 rounded border border-(--color-border-glass) bg-(--color-glass) px-3 py-1.5 text-sm text-(--color-fg-meta) transition-colors hover:bg-(--color-glass-hover) hover:text-(--color-fg-sub)"
          aria-label="Search documentation"
        >
          <span className="h-3.5 w-3.5">{searchIcon}</span>
          <span className="hidden sm:inline">Search…</span>
          <kbd className="hidden rounded border border-(--color-border-glass) bg-(--color-bg) px-1.5 py-0.5 font-mono text-[0.65rem] sm:inline">
            /
          </kbd>
        </button>
      )}

      {open ? (
        <div
          className="fixed inset-0 z-50 flex items-start justify-center bg-black/70 p-4 pt-[10vh] backdrop-blur-md"
          onClick={(e) => e.target === e.currentTarget && setOpen(false)}
          ref={dialogRef}
          role="dialog"
          aria-modal="true"
          aria-label="Search documentation"
        >
          <div className="w-full max-w-2xl overflow-hidden rounded-xl border border-(--color-border-glass) bg-(--color-card) shadow-2xl shadow-black/60">
            <div className="max-h-[70vh] overflow-y-auto p-4">
              <div ref={mountRef} />
            </div>
            <div className="flex items-center justify-between gap-2 border-t border-(--color-border-glass-subtle) bg-(--color-bg-elevated) px-4 py-2 text-[0.7rem] text-(--color-fg-meta)">
              <span>
                <kbd className="rounded border border-(--color-border-glass) bg-(--color-glass) px-1.5 py-0.5 font-mono">↵</kbd>
                {' to open'}
              </span>
              <span>
                <kbd className="rounded border border-(--color-border-glass) bg-(--color-glass) px-1.5 py-0.5 font-mono">esc</kbd>
                {' to close'}
              </span>
            </div>
          </div>
        </div>
      ) : null}
    </>
  );
}
