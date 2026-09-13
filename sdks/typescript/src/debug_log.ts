export type SdkLogLevel = 'off' | 'normal' | 'verbose';

type SdkLogSink = (leg: number, event: string, detail: string) => void;

let sink: SdkLogSink = () => {};
let level: SdkLogLevel = 'off';

export function setSdkLogSink(next: SdkLogSink): void {
  sink = next;
}

export function setSdkLogLevel(next: SdkLogLevel): void {
  level = next;
}

export function getBoothLogLevel(): SdkLogLevel {
  return level;
}

export function boothLog(leg: number, event: string, detail = ''): void {
  sink(leg, event, detail);
}
