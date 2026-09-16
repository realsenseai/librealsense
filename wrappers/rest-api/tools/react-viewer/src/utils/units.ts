/** Distance formatting per the units setting, matching the legacy viewer's readouts. */

const FEET_PER_METER = 3.28084

/** A distance in the display unit (meters or feet). */
export function toDisplayUnits(meters: number, metric: boolean): number {
  return metric ? meters : meters * FEET_PER_METER
}

export function unitLabel(metric: boolean): string {
  return metric ? 'm' : 'ft'
}

/** "153 mm" under 20 cm, "1.234 m" otherwise; "4.05 ft" in imperial. */
export function formatDistance(meters: number, metric: boolean): string {
  if (!metric) return `${(meters * FEET_PER_METER).toFixed(2)} ft`
  if (meters < 0.2) return `${Math.round(meters * 1000)} mm`
  return `${meters.toFixed(3)} m`
}
