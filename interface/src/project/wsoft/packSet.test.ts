import { describe, expect, it } from '@jest/globals';

import { DEFAULT_PACK_SET, formatPackTargets, parsePackTargets, toleranceProblem } from './packSet';

describe('packSet helpers', () => {
  it('defaults to disabled with empty targets and zero tolerance', () => {
    expect(DEFAULT_PACK_SET).toEqual({ enabled: false, tolerance_g: 0, targets_g: [] });
    expect(formatPackTargets(DEFAULT_PACK_SET.targets_g)).toBe('');
  });

  it('parses up to 8 whole-gram targets', () => {
    expect(parsePackTargets('')).toEqual({ targets: [] });
    expect(parsePackTargets('500, 1000, 2000')).toEqual({ targets: [500, 1000, 2000] });
    expect(parsePackTargets(' 250  500  1000 ')).toEqual({ targets: [250, 500, 1000] });
    expect(parsePackTargets('1, 2, 3, 4, 5, 6, 7, 8').targets).toHaveLength(8);
  });

  it('rejects too many, zero, negative or fractional targets', () => {
    expect(parsePackTargets('1, 2, 3, 4, 5, 6, 7, 8, 9').error).toBeDefined();
    expect(parsePackTargets('0, 500').error).toBeDefined();
    expect(parsePackTargets('-500').error).toBeDefined();
    expect(parsePackTargets('500.5').error).toBeDefined();
    expect(parsePackTargets('abc').error).toBeDefined();
  });

  it('validates tolerance in whole grams', () => {
    expect(toleranceProblem(0)).toBeUndefined();
    expect(toleranceProblem(20)).toBeUndefined();
    expect(toleranceProblem(-1)).toBeDefined();
    expect(toleranceProblem(12.5)).toBeDefined();
  });
});
