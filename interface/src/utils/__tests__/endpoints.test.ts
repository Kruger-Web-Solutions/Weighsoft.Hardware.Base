import { AxiosError } from 'axios';

import { extractErrorMessage } from '../endpoints';

describe('extractErrorMessage', () => {
  it('returns response data message when present', () => {
    const err = {
      message: 'Request failed',
      response: { data: { message: 'Device busy' } }
    } as AxiosError<{ message?: string }>;
    expect(extractErrorMessage(err, 'default')).toBe('Device busy');
  });

  it('falls back to axios error message when no server message', () => {
    const err = { message: 'Network Error' } as AxiosError<{ message?: string }>;
    expect(extractErrorMessage(err, 'default')).toBe('Network Error');
  });

  it('falls back to default when message fields are empty', () => {
    const err = {
      message: '',
      response: { data: {} }
    } as AxiosError<{ message?: string }>;
    expect(extractErrorMessage(err, 'Something went wrong')).toBe('Something went wrong');
  });
});
