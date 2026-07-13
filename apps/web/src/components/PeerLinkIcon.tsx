import type { LinkUiState } from '../lib/types';

/** Clickable link glyph: linking (attempt) or linked (established). */
export function PeerLinkIcon({
  state,
  title,
  onClick,
}: {
  state: Exclude<LinkUiState, 'none'>;
  title?: string;
  onClick?: () => void;
}) {
  const label =
    title ??
    (state === 'linking' ? 'Linking… — click to release' : 'Linked — click to release');
  return (
    <button
      type="button"
      className={`peer-link-icon peer-link-icon--${state}`}
      title={label}
      aria-label={label}
      onClick={(e) => {
        e.stopPropagation();
        onClick?.();
      }}
    >
      <svg viewBox="0 0 24 24" width="14" height="14" aria-hidden>
        <path
          fill="none"
          stroke="currentColor"
          strokeWidth="2.2"
          strokeLinecap="round"
          strokeDasharray={state === 'linking' ? '3 3' : undefined}
          d="M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71"
        />
        <path
          fill="none"
          stroke="currentColor"
          strokeWidth="2.2"
          strokeLinecap="round"
          strokeDasharray={state === 'linking' ? '3 3' : undefined}
          d="M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71"
        />
      </svg>
    </button>
  );
}
