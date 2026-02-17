# Weight Forwarder Implementation - Lessons Learned

## Overview

This document captures critical lessons learned during the Weight Stream Forwarder implementation. These issues caused significant debugging time and should be avoided in future implementations.

## Critical Issues Encountered

### 1. Authentication: Using fetch() Instead of AXIOS

**Issue**: API calls returned `401 Unauthorized` despite user being logged in.

**Root Cause**: Used plain `fetch()` API instead of the project's `AXIOS` instance, bypassing JWT authentication.

**Wrong Implementation**:
```typescript
// ❌ WRONG - No JWT token sent
export function readWeightForwarder(signal?: AbortSignal) {
  return fetch('/rest/weightForwarder', { signal })
    .then((response) => {
      if (response.status === 200) {
        return response.json();
      }
      throw Error('Unexpected response code: ' + response.status);
    });
}
```

**Correct Implementation**:
```typescript
// ✅ CORRECT - AXIOS includes JWT automatically
import { AxiosPromise } from 'axios';
import { AXIOS } from './endpoints';

export function readWeightForwarder(): AxiosPromise<WeightForwarderData> {
  return AXIOS.get('weightForwarder');
}
```

**Why This Matters**:
- `AXIOS` is pre-configured with JWT interceptor
- Every request automatically includes `Authorization: Bearer <token>` header
- Plain `fetch()` requires manual token handling
- Without auth token, all protected endpoints return 401

**Prevention**:
- ALWAYS use `AXIOS` for API calls
- Check existing examples (Serial, LED) for correct pattern
- Never use `fetch()` for backend API communication

### 2. Feature Flags: Using import.meta.env Instead of FeaturesContext

**Issue**: TypeScript compilation error: `Property 'env' does not exist on type 'ImportMeta'`

**Root Cause**: Tried to read feature flags from `import.meta.env` (Vite-specific), but this project loads flags from backend API at runtime.

**Wrong Implementation**:
```typescript
// ❌ WRONG - Vite syntax not used here
const hasMqtt = import.meta.env.FT_MQTT;
const hasBle = import.meta.env.VITE_FT_BLE;
```

**Correct Implementation**:
```typescript
// ✅ CORRECT - Use FeaturesContext
import { useContext } from 'react';
import { FeaturesContext } from '../../contexts/features';

const WeightForwarderConfig: FC = () => {
  const { features } = useContext(FeaturesContext);
  
  const getAvailableProtocols = () => {
    const protocols = [
      { value: ForwardProtocol.HTTP, label: 'HTTP POST' },
      { value: ForwardProtocol.WS, label: 'WebSocket Client' },
    ];
    if (features.mqtt) {
      protocols.push({ value: ForwardProtocol.MQTT, label: 'MQTT' });
    }
    if (features.ble) {
      protocols.push({ value: ForwardProtocol.BLE, label: 'BLE Client' });
    }
    return protocols;
  };
  // ...
};
```

**Why This Matters**:
- Feature flags are device-specific and loaded from `/rest/features` API
- `import.meta.env` is for build-time environment variables (not used in this project)
- Using wrong approach causes compilation errors

**Prevention**:
- ALWAYS use `FeaturesContext` for runtime feature detection
- Check `features.mqtt`, `features.ble`, etc.
- Reference LED or Serial examples for correct usage

### 3. Hook Import Path: useWs from components/ Instead of utils/

**Issue**: Module not found error: `Can't resolve '../../hooks/useWs'` or `'../../components'`

**Root Cause**: Hooks are in `utils/` directory, not `components/` or `hooks/`.

**Wrong Implementations**:
```typescript
// ❌ WRONG - Hooks are not in components
import { useWs } from '../../components';

// ❌ WRONG - Hooks are not in a hooks directory
import { useWs } from '../../hooks/useWs';

// ❌ WRONG - Don't import from individual files
import { useWs } from '../../utils/useWs';
```

**Correct Implementation**:
```typescript
// ✅ CORRECT - Import from utils index
import { useWs, useRest } from '../../utils';
```

**Why This Matters**:
- Project structure uses `utils/` for custom hooks
- Importing from wrong location causes module resolution errors
- Wastes time debugging import paths

**Prevention**:
- ALWAYS import hooks from `../../utils`
- Check existing examples for correct import patterns
- Use auto-complete to verify import paths

### 4. Component Usage: ValidatedForm Not Available

**Issue**: `ValidatedForm` component doesn't exist in the project.

**Root Cause**: Assumed component availability without checking project components.

**Wrong Implementation**:
```typescript
// ❌ WRONG - ValidatedForm doesn't exist
import { ValidatedForm } from '../../components';

<ValidatedForm onSubmit={handleSubmit}>
  {/* form fields */}
</ValidatedForm>
```

**Correct Implementation**:
```typescript
// ✅ CORRECT - Use SectionContent, FormLoader, ButtonRow
import { SectionContent, FormLoader, ButtonRow } from '../../components';
import { useRest } from '../../utils';

const { data, setData, saveData, saving } = useRest<T>({ read });

if (!data) {
  return (
    <SectionContent title="Configuration" titleGutter>
      <FormLoader onRetry={loadData} errorMessage={errorMessage} />
    </SectionContent>
  );
}

return (
  <SectionContent title="Configuration" titleGutter>
    {/* form fields */}
    <ButtonRow>
      <Button startIcon={<SaveIcon />} onClick={saveData} disabled={saving}>
        Save
      </Button>
    </ButtonRow>
  </SectionContent>
);
```

**Why This Matters**:
- Using non-existent components causes compilation errors
- Each project has specific component patterns
- Need to follow project conventions

**Prevention**:
- Check `interface/src/components/` for available components
- Reference existing examples (LED, Serial) for correct patterns
- Use `SectionContent`, `FormLoader`, `ButtonRow` for forms

### 5. Deployment: Menu Not Appearing After Upload

**Issue**: Successfully uploaded firmware but new menu item didn't appear in UI.

**Root Causes**:
1. **PROGMEM_WWW**: Web interface embedded in firmware, not SPIFFS filesystem
2. **No Hard Reset**: Device was running old firmware after upload
3. **Browser Cache**: Old interface cached in browser

**Wrong Assumptions**:
- Upload automatically restarts device with new firmware
- Filesystem upload affects the web interface
- Browser refresh is sufficient

**Correct Process**:
```powershell
# 1. Kill Python processes (COM port busy prevention)
Get-Process python* | Stop-Process -Force

# 2. Build and upload firmware
python -m platformio run -e esp32s3 -t upload

# 3. CRITICAL: Physically press EN/RST button on ESP32-S3
# (New firmware won't load without hard reset!)

# 4. Monitor serial output to verify new firmware boots
# Look for service initialization messages

# 5. Hard refresh browser (Ctrl+Shift+R)
```

**Why This Matters**:
- `PROGMEM_WWW` means interface is part of firmware binary
- Auto-reset after upload doesn't always trigger firmware reload
- Physical reset ensures new firmware loads
- 401 errors indicate old firmware still running (no endpoint)

**Prevention**:
- ALWAYS physically reset device after firmware upload
- Check serial output for service initialization
- Verify service counter in boot logs
- Hard refresh browser to clear cache

### 6. Upload Process: COM Port Busy

**Issue**: Upload failed with `PermissionError(13, 'Access is denied.')` or "COM10 port is busy".

**Root Cause**: Python processes (serial monitors, scripts) holding COM port open.

**Wrong Approach**:
- Try upload multiple times hoping it works
- Manually close suspected applications
- Reboot computer

**Correct Process**:
```powershell
# ALWAYS kill Python processes before upload
Get-Process python* | Stop-Process -Force

# Then upload
python -m platformio run -e esp32s3 -t upload
```

**Why This Matters**:
- Windows COM ports can only be accessed by one process
- Serial monitors and scripts prevent uploads
- Systematic approach saves time

**Prevention**:
- Created `.cursor/rules/platformio-upload.mdc` to enforce this
- ALWAYS kill Python processes before upload
- Make it a habit, not an afterthought

## Best Practices Summary

### API Layer
1. ✅ Use `AXIOS` for all API calls (never `fetch()`)
2. ✅ Import from `'./endpoints'` for API files
3. ✅ Use relative paths: `'mydevice'` not `'/rest/mydevice'`
4. ✅ Follow AxiosPromise return type signature

### Feature Detection
1. ✅ Use `FeaturesContext` for runtime flags
2. ✅ Never use `import.meta.env` for feature flags
3. ✅ Check `features.mqtt`, `features.ble`, etc.
4. ✅ Reference existing examples for patterns

### Component Imports
1. ✅ Import hooks from `'../../utils'`
2. ✅ Import shared components from `'../../components'`
3. ✅ Use `SectionContent`, `FormLoader`, `ButtonRow`
4. ✅ Check available components before using

### Deployment Process
1. ✅ Kill Python processes before upload
2. ✅ Upload firmware with `python -m platformio`
3. ✅ Physically reset device (EN/RST button)
4. ✅ Monitor serial output to verify boot
5. ✅ Hard refresh browser (Ctrl+Shift+R)

### Debugging Checklist
- [ ] Using `AXIOS` not `fetch()`?
- [ ] Using `FeaturesContext` not `import.meta.env`?
- [ ] Importing hooks from `../../utils`?
- [ ] Using project components correctly?
- [ ] Physically reset device after upload?
- [ ] Checking serial output for errors?

## Time Cost Analysis

**Issues Caused Total Delay**: ~2 hours of debugging

**Breakdown**:
- Authentication issue: 30 minutes
- Feature flags issue: 15 minutes
- Import path issues: 20 minutes
- Component availability: 20 minutes
- Deployment/reset issues: 35 minutes

**With Proper Documentation**: Would have taken ~10 minutes to reference patterns

**Return on Investment**: Creating this document saves 2 hours per future implementation

## Documentation Updates

The following documents were updated to prevent these issues:

1. **FRONTEND-PATTERNS.md** (Critical updates)
   - Added "Authentication and API Security" section
   - Added "Feature Flag Detection" section
   - Added troubleshooting for 401 errors
   - Added deployment issues section
   - Added COM port busy solution

2. **.cursor/rules/platformio-upload.mdc** (New rule)
   - Always kill Python processes before upload
   - Standardized upload command
   - Prevents COM port busy errors

3. **API-REFERENCE.md** (Existing)
   - Already documented WeightForwarder endpoints

## Recommendations for Future Work

### Before Implementation
1. Read `FRONTEND-PATTERNS.md` - Authentication and Feature Flags sections
2. Check existing examples (LED, Serial) for correct patterns
3. Review available components in `interface/src/components/`
4. Understand deployment process (PROGMEM_WWW implications)

### During Implementation
1. Use `AXIOS` for all API calls (check examples)
2. Use `FeaturesContext` for feature detection (never `import.meta.env`)
3. Import hooks from `../../utils`
4. Use `SectionContent`, `FormLoader`, `ButtonRow` for forms

### After Implementation
1. Kill Python processes before upload
2. Upload firmware
3. Physically reset device (EN/RST)
4. Monitor serial output
5. Hard refresh browser

### Testing Checklist
- [ ] Backend compiles without errors
- [ ] Frontend compiles without TypeScript errors
- [ ] API endpoints return data (no 401)
- [ ] WebSocket connects and streams
- [ ] Menu item appears in UI
- [ ] All protocols work (HTTP, WS, MQTT, BLE)
- [ ] Settings persist across reboot

## Conclusion

Most issues were caused by:
1. Not using project-specific patterns (AXIOS, FeaturesContext)
2. Incorrect import paths
3. Assuming component availability
4. Not understanding deployment process

**Solution**: Follow existing examples and updated documentation.

**Key Takeaway**: ALWAYS check existing implementations before assuming patterns. The LED and Serial examples are the source of truth for correct implementation patterns.
