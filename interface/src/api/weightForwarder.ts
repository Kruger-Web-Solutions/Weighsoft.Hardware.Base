import type { WeightForwarderData } from '../types/weightForwarder';

export const WEIGHT_FORWARDER_ENDPOINT = '/rest/weightForwarder';

export function readWeightForwarder(signal?: AbortSignal) {
  return fetch(WEIGHT_FORWARDER_ENDPOINT, { signal })
    .then((response) => {
      if (response.status === 200) {
        return response.json();
      }
      throw Error('Unexpected response code: ' + response.status);
    });
}

export function updateWeightForwarder(data: WeightForwarderData, signal?: AbortSignal) {
  return fetch(WEIGHT_FORWARDER_ENDPOINT, {
    signal,
    method: 'POST',
    body: JSON.stringify(data),
    headers: {
      'Content-Type': 'application/json',
    },
  })
    .then((response) => {
      if (response.status === 200) {
        return response.json();
      }
      throw Error('Unexpected response code: ' + response.status);
    });
}
