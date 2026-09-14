interface BrandLockupProps {
  align?: 'start' | 'center';
}

export function BrandLockup({ align = 'start' }: BrandLockupProps) {
  return (
    <div className={`brand-lockup brand-lockup-${align}`}>
      <div className="icon-box" aria-hidden>
        <img className="title-logo" src="/rocketbox-mark.png" alt="" />
      </div>
      <div className="brand-block">
        <img className="brand-wordmark" src="/rocketbox-wordmark.png" alt="ROCKETBOX" />
        <div className="brand">Transfer</div>
      </div>
    </div>
  );
}
