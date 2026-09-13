import { boothLog, getBoothLogLevel } from './debug_log';
import type { FabricLink } from './fabric_link';
import type { ControlJob, Tier } from './link_types';

export function runUsb<T>(link: FabricLink, tier: Tier, label: string, fn: () => Promise<T>): Promise<T> {
  if (getBoothLogLevel() === 'verbose') {
    boothLog(link.fabricLeg, 'usb_queue', `tier=${tier} ${label}`);
  }
  const run = link.usbTail.then(() => fn());
  link.usbTail = run.then(
    () => undefined,
    () => undefined,
  );
  return run;
}

export function enqueueControl(
  link: FabricLink,
  label: string,
  fn: () => Promise<void>,
  priority: 'listen' | 'session' = 'session',
): Promise<void> {
  return new Promise((resolve, reject) => {
    const job: ControlJob = {
      label,
      priority: priority === 'listen' ? 0 : 1,
      run: fn,
      resolve: () => resolve(),
      reject,
    };
    if (priority === 'session') {
      link.controlQueue.unshift(job);
    } else if (link.controlQueue.some((entry) => entry.priority === 0)) {
      job.resolve();
      return;
    } else {
      link.controlQueue.push(job);
    }
    void drainControlQueue(link);
  });
}

export async function drainControlQueue(link: FabricLink): Promise<void> {
  if (link.controlDrainActive) {
    return;
  }
  link.controlDrainActive = true;
  try {
    while (link.controlQueue.length > 0) {
      const job = link.controlQueue.shift()!;
      try {
        await runUsb(link, 0, job.label, job.run);
        job.resolve();
      } catch (err) {
        job.reject(err);
      }
    }
  } finally {
    link.controlDrainActive = false;
    if (link.controlQueue.length > 0) {
      void drainControlQueue(link);
    }
  }
}
