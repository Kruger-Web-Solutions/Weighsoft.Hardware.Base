import { describe, expect, it } from '@jest/globals';

import { checkForm, EMPTY_FORM, formToProduct, gramsToKg, gramsToKgShort, kgToGrams, productToForm } from './products';

describe('kg and grams', () => {
  it('reads kg exactly, with up to 3 decimals', () => {
    expect(kgToGrams('5')).toBe(5000);
    expect(kgToGrams('5.05')).toBe(5050);
    expect(kgToGrams(' 0.001 ')).toBe(1);
    expect(kgToGrams('7100')).toBe(7100000);
    expect(kgToGrams('1000000')).toBe(1000000000);
    expect(kgToGrams('1000000.001')).toBeUndefined();
    expect(kgToGrams('5.0505')).toBeUndefined();
    expect(kgToGrams('-1')).toBeUndefined();
    expect(kgToGrams('5,05')).toBeUndefined();
    expect(kgToGrams('')).toBeUndefined();
  });

  it('shows grams as kg with 3 decimals', () => {
    expect(gramsToKg(5050)).toBe('5.050');
    expect(gramsToKg(1)).toBe('0.001');
    expect(gramsToKg(7100000)).toBe('7100.000');
    expect(gramsToKgShort(7100000)).toBe('7100');
    expect(gramsToKgShort(5050)).toBe('5.05');
    expect(gramsToKgShort(1)).toBe('0.001');
  });
});

describe('product form', () => {
  const good = { code: 'BEET2', name: 'Beetroot', low: '2', target: '2.02', high: '2.08' };

  it('accepts a good product and sends grams', () => {
    expect(checkForm(good, ['MAIZE'])).toEqual({});
    expect(formToProduct(good)).toEqual({ code: 'BEET2', name: 'Beetroot', low_g: 2000, target_g: 2020, high_g: 2080 });
    expect(productToForm(formToProduct(good)).target).toBe('2.020');
  });

  it('refuses a Hub code, a bad code and an empty form', () => {
    expect(checkForm({ ...good, code: 'MAIZE' }, ['MAIZE']).code).toBe('That code belongs to the Hub.');
    expect(checkForm({ ...good, code: 'BEET 2' }, []).code).toBeDefined();
    expect(Object.keys(checkForm(EMPTY_FORM, []))).toEqual(['code', 'name', 'low', 'target', 'high']);
  });

  it('needs Low < High and Target between them', () => {
    expect(checkForm({ ...good, high: '2' }, []).high).toBe('Must be above Low');
    expect(checkForm({ ...good, target: '2.09' }, []).target).toBe('Must be between Low and High');
    expect(checkForm({ ...good, target: '2' }, [])).toEqual({});
    expect(checkForm({ ...good, low: '0', target: '0' }, []).low).toBeDefined();
  });
});
