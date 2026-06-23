export interface BoothNetworkInfo {
  ips: string[];
  port: number;
  protocol: 'http:' | 'https:';
}

export async function fetchBoothNetworkInfo(): Promise<BoothNetworkInfo | null> {
  try {
    const response = await fetch('/__booth/network.json', { cache: 'no-store' });
    if (!response.ok) {
      return null;
    }
    const data = (await response.json()) as BoothNetworkInfo;
    if (!Array.isArray(data.ips) || data.ips.length === 0) {
      return null;
    }
    return data;
  } catch {
    return null;
  }
}
