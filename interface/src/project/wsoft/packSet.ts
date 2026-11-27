import { WSoftPackSet } from './types';

export const PACK_SET_MAX = 8;
export const PACK_SET_GRAMS_MAX = 1000000000;

export const DEFAULT_PACK_SET: WSoftPackSet = {
  enabled: false,
  tolerance_g: 0,
  targets_g: []
};

export interface PackTargetsParse {
  targets: number[];
  error?: string;
}

/** Parses comma- or whitespace-separated whole-gram targets (1..1000000000, max 8). */
export const parsePackTargets = (text: string, maxTargets = PACK_SET_MAX): PackTargetsParse => {
  const trimmed = text.trim();
  if (trimmed === '') return { targets: [] };
  const parts = trimmed.split(/[\s,]+/).filter((part) => part.length > 0);
  if (parts.length > maxTargets) {
    return { targets: [], error: `Enter at most ${maxTargets} targets.` };
  }
  const targets: number[] = [];
  for (const part of parts) {
    if (!/^\d+$/.test(part)) {
      return { targets: [], error: 'Targets must be whole grams, separated by commas (e.g. 500, 1000, 2000).' };
    }
    const g = Number(part);
    if (!Number.isSafeInteger(g) || g <= 0 || g > PACK_SET_GRAMS_MAX) {
      return { targets: [], error: `Each target must be between 1 and ${PACK_SET_GRAMS_MAX} g.` };
    }
    targets.push(g);
  }
  return { targets };
};

export const formatPackTargets = (targets?: number[]): string => (targets && targets.length > 0 ? targets.join(', ') : '');

export const toleranceProblem = (tolerance_g: number): string | undefined => {
  if (!Number.isInteger(tolerance_g) || tolerance_g < 0 || tolerance_g > PACK_SET_GRAMS_MAX) {
    return `Tolerance must be a whole number from 0 to ${PACK_SET_GRAMS_MAX} g.`;
  }
  return undefined;
};
