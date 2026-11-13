import { WSoftProductInput } from './types';

// Same rules as the board (lib/wsoft_core/src/wsoft_products.cpp); the board checks again.
export const CODE_RULE = /^[A-Za-z0-9_-]{1,32}$/;
const NAME_RULE = /^[\x20-\x7e]{1,24}$/;
const KG_RULE = /^(\d{1,7})(?:\.(\d{1,3}))?$/;
export const GRAMS_MAX = 1000000000;

/** "5.05" -> 5050 grams, exactly (no floating point). Undefined when it is not a kg value. */
export const kgToGrams = (text: string): number | undefined => {
  const match = KG_RULE.exec(text.trim());
  if (!match) return undefined;
  const grams = Number(match[1]) * 1000 + Number((match[2] ?? '').padEnd(3, '0'));
  return grams > GRAMS_MAX ? undefined : grams;
};

/** 5050 -> "5.050". */
export const gramsToKg = (grams: number): string => {
  const whole = Math.floor(grams / 1000);
  const rest = String(grams % 1000).padStart(3, '0');
  return `${whole}.${rest}`;
};

/** 5050 -> "5.05", 7100000 -> "7100": for lists, no trailing zeros. */
export const gramsToKgShort = (grams: number): string => gramsToKg(grams).replace(/\.?0+$/, '');

export interface ProductForm {
  code: string;
  name: string;
  low: string;
  target: string;
  high: string;
}

export const EMPTY_FORM: ProductForm = { code: '', name: '', low: '', target: '', high: '' };

export type FormProblems = Partial<Record<keyof ProductForm, string>>;

/** Problems to show under each box; an empty object means the form can be saved. */
export const checkForm = (form: ProductForm, hubCodes: string[]): FormProblems => {
  const problems: FormProblems = {};
  if (!CODE_RULE.test(form.code)) problems.code = '1 to 32 letters, numbers, - or _. No spaces.';
  else if (hubCodes.includes(form.code)) problems.code = 'That code belongs to the Hub.';
  if (!NAME_RULE.test(form.name) || form.name.trim() === '') problems.name = '1 to 24 plain letters or numbers.';
  const low = kgToGrams(form.low);
  const target = kgToGrams(form.target);
  const high = kgToGrams(form.high);
  if (low === undefined || low <= 0) problems.low = 'kg, e.g. 5.000';
  if (target === undefined) problems.target = 'kg, e.g. 5.050';
  if (high === undefined) problems.high = 'kg, e.g. 5.200';
  if (low !== undefined && high !== undefined && low >= high) problems.high = 'Must be above Low';
  if (low !== undefined && high !== undefined && target !== undefined && (target < low || target > high)) {
    problems.target = 'Must be between Low and High';
  }
  return problems;
};

/** The product to send. Call only when checkForm found no problems. */
export const formToProduct = (form: ProductForm): WSoftProductInput => ({
  code: form.code,
  name: form.name,
  low_g: kgToGrams(form.low) ?? 0,
  target_g: kgToGrams(form.target) ?? 0,
  high_g: kgToGrams(form.high) ?? 0
});

export const productToForm = (p: WSoftProductInput): ProductForm => ({
  code: p.code,
  name: p.name,
  low: gramsToKg(p.low_g),
  target: gramsToKg(p.target_g),
  high: gramsToKg(p.high_g)
});
