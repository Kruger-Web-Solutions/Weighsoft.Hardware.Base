import { FC } from 'react';

import { Box, Switch, Slider, Typography, Stack } from '@mui/material';

import { WEB_SOCKET_ROOT } from '../../api/endpoints';
import { BlockFormControlLabel, FormLoader, MessageBox, SectionContent } from '../../components';
import { updateValue, useWs } from '../../utils';

import { LedExampleState } from './types';

export const LED_EXAMPLE_WEBSOCKET_URL = WEB_SOCKET_ROOT + "ledExample";

const LedControlWebSocket: FC = () => {
  const { connected, updateData, data } = useWs<LedExampleState>(LED_EXAMPLE_WEBSOCKET_URL);

  const updateFormValue = updateValue(updateData);

  const handleColorChange = (color: 'red' | 'green' | 'blue') => (event: Event, value: number | number[]) => {
    if (data) {
      updateData({ ...data, [color]: value as number });
    }
  };

  const content = () => {
    if (!connected || !data) {
      return (<FormLoader message="Connecting to WebSocket…" />);
    }
    
    const rgbColor = `rgb(${data.red}, ${data.green}, ${data.blue})`;
    
    return (
      <>
        <MessageBox
          level="info"
          message="Control the LED via WebSocket. Changes are instant and bidirectional - updates from other channels appear automatically."
          my={2}
        />
        <BlockFormControlLabel
          control={
            <Switch
              name="led_on"
              checked={data.led_on}
              onChange={updateFormValue}
              color="primary"
            />
          }
          label="LED On?"
        />
        
        {data.has_rgb && (
          <Box sx={{ mt: 3 }}>
            <Typography variant="subtitle2" gutterBottom>
              RGB Color Control
            </Typography>
            
            {/* Color Preview */}
            <Box
              sx={{
                width: '100%',
                height: 60,
                borderRadius: 2,
                backgroundColor: data.led_on ? rgbColor : '#1e1e1e',
                border: '2px solid',
                borderColor: 'divider',
                mb: 3,
                transition: 'background-color 0.2s',
                boxShadow: data.led_on ? `0 0 20px ${rgbColor}` : 'none',
              }}
            />
            
            {/* RGB Sliders */}
            <Stack spacing={2}>
              {/* Red Slider */}
              <Box>
                <Typography variant="caption" color="text.secondary">
                  Red: {data.red}
                </Typography>
                <Slider
                  value={data.red}
                  onChange={handleColorChange('red')}
                  min={0}
                  max={255}
                  disabled={!data.led_on}
                  sx={{
                    color: '#ff0000',
                    '& .MuiSlider-thumb': {
                      backgroundColor: `rgb(${data.red}, 0, 0)`,
                    },
                  }}
                />
              </Box>
              
              {/* Green Slider */}
              <Box>
                <Typography variant="caption" color="text.secondary">
                  Green: {data.green}
                </Typography>
                <Slider
                  value={data.green}
                  onChange={handleColorChange('green')}
                  min={0}
                  max={255}
                  disabled={!data.led_on}
                  sx={{
                    color: '#00ff00',
                    '& .MuiSlider-thumb': {
                      backgroundColor: `rgb(0, ${data.green}, 0)`,
                    },
                  }}
                />
              </Box>
              
              {/* Blue Slider */}
              <Box>
                <Typography variant="caption" color="text.secondary">
                  Blue: {data.blue}
                </Typography>
                <Slider
                  value={data.blue}
                  onChange={handleColorChange('blue')}
                  min={0}
                  max={255}
                  disabled={!data.led_on}
                  sx={{
                    color: '#0000ff',
                    '& .MuiSlider-thumb': {
                      backgroundColor: `rgb(0, 0, ${data.blue})`,
                    },
                  }}
                />
              </Box>
            </Stack>
            
            <Typography variant="caption" color="text.secondary" sx={{ display: 'block', mt: 2 }}>
              Current Color: {rgbColor.toUpperCase()}
            </Typography>
          </Box>
        )}
      </>
    );
  };

  return (
    <SectionContent title='WebSocket Control' titleGutter>
      {content()}
    </SectionContent>
  );
};

export default LedControlWebSocket;
