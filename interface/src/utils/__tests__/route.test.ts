import { routeMatches } from '../route';

describe('routeMatches', () => {
  it('matches exact route path', () => {
    expect(routeMatches('/wifi', '/wifi')).toBe(true);
  });

  it('matches when pathname is under route', () => {
    expect(routeMatches('/wifi', '/wifi/status')).toBe(true);
  });

  it('does not match unrelated paths', () => {
    expect(routeMatches('/wifi', '/mqtt')).toBe(false);
    expect(routeMatches('/wifi', '/wifix')).toBe(false);
  });
});
