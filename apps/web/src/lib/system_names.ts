/** Fixed fabric systems: Port 1 = Alice, Port 2 = Bob, Port 3 = Carol, Port 4 = Dave. */

const SYSTEM_NAMES = ['Alice', 'Bob', 'Carol', 'Dave'] as const;

export function systemNameForLeg(leg: number): string {
  if (leg >= 0 && leg < SYSTEM_NAMES.length) {
    return SYSTEM_NAMES[leg]!;
  }
  return `System ${leg + 1}`;
}
