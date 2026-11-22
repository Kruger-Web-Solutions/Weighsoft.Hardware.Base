/**
 * Backend sends timestamp as millis() (ms since ESP32 boot), not Unix epoch.
 * Format for display as "Boot + X.X s" or "Boot + N ms".
 */
export function formatSerialTimestamp(msSinceBoot: number): string {
  if (msSinceBoot >= 1000) {
    const sec = (msSinceBoot / 1000).toFixed(1);
    return `Boot + ${sec} s`;
  }
  return `Boot + ${msSinceBoot} ms`;
}

/**
 * Sanitize last_line for safe display when it contains binary or non-printable UTF-8.
 * Replaces control chars and replacement chars so the UI shows that data is present.
 */
export function sanitizeLastLineForDisplay(line: string): string {
  if (!line) return line;
  let out = '';
  for (let i = 0; i < line.length; i++) {
    const code = line.charCodeAt(i);
    // C0/C1 controls and the Unicode replacement character become a middle dot.
    const hidden = code <= 31 || (code >= 127 && code <= 159) || code === 0xfffd;
    out += hidden ? '\u00B7' : line.charAt(i);
  }
  return out;
}
