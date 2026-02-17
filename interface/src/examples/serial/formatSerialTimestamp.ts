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
  return line
    .replace(/[\u0000-\u001F\u007F-\u009F]/g, '\u00B7') // control chars → middle dot
    .replace(/\uFFFD/g, '\u00B7'); // replacement char → middle dot
}
