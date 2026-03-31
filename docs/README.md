# ESP8266-React Framework Documentation

## Welcome

This documentation provides comprehensive architecture, design patterns, API references, and guides for the ESP8266-React IoT framework. It is designed for developers extending the framework and AI agents working with the codebase.

## Quick Start

### For Developers

1. **New to the framework?** Start with [ARCHITECTURE.md](ARCHITECTURE.md)
2. **Need to know which branch to use?** Read [INTEGRATION-WORKFLOW.md](INTEGRATION-WORKFLOW.md)
3. **Need to understand the system?** Review the C4 diagrams
4. **Want to add features?** Follow the [EXTENSION-GUIDE.md](EXTENSION-GUIDE.md)
5. **Need API docs?** See [API-REFERENCE.md](API-REFERENCE.md)

### For AI Agents

1. Read [ARCHITECTURE.md](ARCHITECTURE.md) for system overview
2. Study [DESIGN-PATTERNS.md](DESIGN-PATTERNS.md) for implementation patterns
3. Check [INTEGRATION-WORKFLOW.md](INTEGRATION-WORKFLOW.md) before assuming integration code is on `master`
4. Reference [API-REFERENCE.md](API-REFERENCE.md) for endpoint contracts
5. Check [FILE-REFERENCE.md](FILE-REFERENCE.md) for file locations

## Documentation Structure

### Architecture Documentation (C4 Model)

| Document | Level | Description |
|----------|-------|-------------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Overview | System overview, technology stack, key decisions |
| [C4-CONTEXT.md](C4-CONTEXT.md) | Level 1 | System context, external actors, boundaries |
| [C4-CONTAINER.md](C4-CONTAINER.md) | Level 2 | Containers, communication protocols |
| [C4-COMPONENT-BACKEND.md](C4-COMPONENT-BACKEND.md) | Level 3 | Backend components, services, infrastructure |
| [C4-COMPONENT-FRONTEND.md](C4-COMPONENT-FRONTEND.md) | Level 3 | Frontend components, routing, state management |
| [C4-CODE-PATTERNS.md](C4-CODE-PATTERNS.md) | Level 4 | Code-level patterns, class diagrams |

### Behavior Documentation

| Document | Description |
|----------|-------------|
| [SEQUENCE-DIAGRAMS.md](SEQUENCE-DIAGRAMS.md) | Interaction flows, initialization, state updates, authentication |
| [DATA-FLOWS.md](DATA-FLOWS.md) | Data movement, state propagation, serialization |

### Reference Documentation

| Document | Description |
|----------|-------------|
| [API-REFERENCE.md](API-REFERENCE.md) | Complete REST, WebSocket, and MQTT API contracts |
| [DESIGN-PATTERNS.md](DESIGN-PATTERNS.md) | Pattern catalog with implementation examples |
| [FILE-REFERENCE.md](FILE-REFERENCE.md) | Complete file structure guide |
| [CONFIGURATION.md](CONFIGURATION.md) | Build system, feature flags, factory settings |
| [OTA-UPLOAD.md](OTA-UPLOAD.md) | OTA firmware upload (ESP32-S3): setup, port 8266, commands |
| [PIN-CONFIGURATION.md](PIN-CONFIGURATION.md) | GPIO and hardware pin reference for ESP32-S3, Serial2, I2C, etc. |
| [PLATFORM-GPIO.md](PLATFORM-GPIO.md) | Platform-specific GPIO notes, partition schemes |
| [SECURITY.md](SECURITY.md) | Security architecture, JWT, authentication |

### Practical Guides

| Document | Description |
|----------|-------------|
| [INTEGRATION-WORKFLOW.md](INTEGRATION-WORKFLOW.md) | Branch strategy for `master` and long-lived integration branches |
| [EXTENSION-GUIDE.md](EXTENSION-GUIDE.md) | Step-by-step guide to add custom features |
| [DEVICE-TEMPLATE-GUIDE.md](DEVICE-TEMPLATE-GUIDE.md) | Quick start checklist for new device projects |
| [LED-EXAMPLE.md](LED-EXAMPLE.md) | LED control example - template for controllable devices |
| [DIAGNOSTICS-EXAMPLE.md](DIAGNOSTICS-EXAMPLE.md) | UART diagnostics example with loopback and baud detection |
| [WEIGHT-FORWARDER-LESSONS.md](WEIGHT-FORWARDER-LESSONS.md) | Multi-protocol weight forwarding patterns (WebSocket, SSE, UDP, MQTT) |

Integration-specific documents such as serial, display, or weighing guides are maintained on their matching branches rather than on `master`.

## Learning Paths

### Path 1: Understanding the System

For developers who need to understand how the system works:

1. [ARCHITECTURE.md](ARCHITECTURE.md) - Get the big picture
2. [C4-CONTEXT.md](C4-CONTEXT.md) - Understand system boundaries
3. [C4-CONTAINER.md](C4-CONTAINER.md) - See container interactions
4. [C4-COMPONENT-BACKEND.md](C4-COMPONENT-BACKEND.md) - Backend architecture
5. [C4-COMPONENT-FRONTEND.md](C4-COMPONENT-FRONTEND.md) - Frontend architecture
6. [SEQUENCE-DIAGRAMS.md](SEQUENCE-DIAGRAMS.md) - See how it works in action
7. [DATA-FLOWS.md](DATA-FLOWS.md) - Understand data movement

### Path 2: Extending the Framework

For developers building new features:

1. [DESIGN-PATTERNS.md](DESIGN-PATTERNS.md) - Learn the patterns
2. [C4-CODE-PATTERNS.md](C4-CODE-PATTERNS.md) - See pattern diagrams
3. [EXTENSION-GUIDE.md](EXTENSION-GUIDE.md) - Follow the step-by-step guide
4. [API-REFERENCE.md](API-REFERENCE.md) - Design your API
5. [FILE-REFERENCE.md](FILE-REFERENCE.md) - Know where files go

### Path 3: Configuration and Deployment

For developers deploying the system:

1. [CONFIGURATION.md](CONFIGURATION.md) - Build and feature configuration
2. [SECURITY.md](SECURITY.md) - Secure your deployment
3. [API-REFERENCE.md](API-REFERENCE.md) - Test your endpoints

### Path 4: Maintenance and Debugging

For developers maintaining the codebase:

1. [FILE-REFERENCE.md](FILE-REFERENCE.md) - Navigate the codebase
2. [SEQUENCE-DIAGRAMS.md](SEQUENCE-DIAGRAMS.md) - Understand flows
3. [DATA-FLOWS.md](DATA-FLOWS.md) - Debug data issues
4. [SECURITY.md](SECURITY.md) - Security troubleshooting

## Key Concepts

### Backend Concepts

- **StatefulService<T>**: Base class for all services managing state
- **Service Composition**: Services compose infrastructure (HttpEndpoint, FSPersistence, etc.)
- **Update Handlers**: Event-driven callbacks when state changes
- **Origin Tracking**: Prevents circular updates across interfaces
- **JSON Serialization**: JsonStateReader and JsonStateUpdater functions

### Frontend Concepts

- **React Context**: Global state (auth, features)
- **useRest Hook**: REST API state management
- **useWs Hook**: WebSocket real-time sync
- **Router Tabs**: Tab-based navigation
- **Feature Flags**: Conditional UI rendering

### Communication Concepts

- **REST API**: Configuration and CRUD operations
- **WebSocket**: Real-time bidirectional updates
- **MQTT**: Home automation integration
- **Origin ID**: Tracks update source ("http", "mqtt", "websocket:{id}")

## Common Tasks

### Task: Add a New REST Endpoint

1. Define state class with read/update methods
2. Create service inheriting StatefulService<T>
3. Compose HttpEndpoint<T>
4. Register in main.cpp
5. See [EXTENSION-GUIDE.md](EXTENSION-GUIDE.md) for details

### Task: Add WebSocket Support

1. Add WebSocketTxRx<T> to service
2. Implement state read/update methods
3. Create useWs hook on frontend
4. See [C4-CODE-PATTERNS.md](C4-CODE-PATTERNS.md) for pattern

### Task: Integrate with Home Assistant

1. Use MqttPubSub<T> in service
2. Implement haRead/haUpdate for HA format
3. Publish discovery config on connect
4. See [SEQUENCE-DIAGRAMS.md](SEQUENCE-DIAGRAMS.md) MQTT flow

### Task: Add Filesystem Persistence

1. Add FSPersistence<T> to service
2. Choose file path in /config/
3. Call readFromFS() in begin()
4. Auto-save happens automatically

### Task: Change Default Settings

1. Edit factory_settings.ini
2. Use placeholders for dynamic values
3. Rebuild firmware
4. See [CONFIGURATION.md](CONFIGURATION.md) for reference

### Task: Enable/Disable Features

1. Edit features.ini
2. Set feature flag to 0 (disable) or 1 (enable)
3. Rebuild firmware and frontend
4. See [CONFIGURATION.md](CONFIGURATION.md) for impact

## Troubleshooting

### Can't find component interaction flow?

→ See [SEQUENCE-DIAGRAMS.md](SEQUENCE-DIAGRAMS.md)

### Don't understand data movement?

→ See [DATA-FLOWS.md](DATA-FLOWS.md)

### Need API contract details?

→ See [API-REFERENCE.md](API-REFERENCE.md)

### How do I implement a pattern?

→ See [DESIGN-PATTERNS.md](DESIGN-PATTERNS.md) and [EXTENSION-GUIDE.md](EXTENSION-GUIDE.md)

### Where does this file belong?

→ See [FILE-REFERENCE.md](FILE-REFERENCE.md)

### How do I secure an endpoint?

→ See [SECURITY.md](SECURITY.md)

### How do I change build settings?

→ See [CONFIGURATION.md](CONFIGURATION.md)

## Contributing

When adding new features:

1. **Follow existing patterns** - See [DESIGN-PATTERNS.md](DESIGN-PATTERNS.md)
2. **Document your API** - Update [API-REFERENCE.md](API-REFERENCE.md)
3. **Update file reference** - Update [FILE-REFERENCE.md](FILE-REFERENCE.md) if adding files
4. **Test thoroughly** - REST, WebSocket, MQTT, persistence

## Documentation Maintenance

### When to Update

- **New feature added**: Update EXTENSION-GUIDE.md, API-REFERENCE.md
- **Pattern changed**: Update DESIGN-PATTERNS.md, C4-CODE-PATTERNS.md
- **API modified**: Update API-REFERENCE.md, SEQUENCE-DIAGRAMS.md
- **Security change**: Update SECURITY.md
- **Build process changed**: Update CONFIGURATION.md

### Documentation Principles

1. **Accuracy**: Keep docs in sync with code
2. **Completeness**: Cover all public APIs and patterns
3. **Clarity**: Use diagrams, examples, and clear language
4. **AI-Friendly**: Precise, unambiguous descriptions to prevent hallucination

## Version History

- **Initial Version**: Complete C4 architecture documentation
- **Last Updated**: 2026-02-09

## Additional Resources

- **Main README**: [../README.md](../README.md) - Project overview
- **Repo workflow**: [INTEGRATION-WORKFLOW.md](INTEGRATION-WORKFLOW.md) - which branch to use before you start
- **Framework Source**: `lib/framework/` - Implementation reference
- **Example Projects**:
  - [LED-EXAMPLE.md](LED-EXAMPLE.md) - Simple LED control
  - [DIAGNOSTICS-EXAMPLE.md](DIAGNOSTICS-EXAMPLE.md) - UART diagnostics

## Feedback

For documentation improvements, please:

1. Check if information exists elsewhere first
2. Submit clear, specific suggestions
3. Include examples where helpful

## Next Steps

Choose your path:

- **Learn**: Start with [ARCHITECTURE.md](ARCHITECTURE.md)
- **Build**: Jump to [EXTENSION-GUIDE.md](EXTENSION-GUIDE.md)
- **Reference**: Use [API-REFERENCE.md](API-REFERENCE.md)
- **Secure**: Read [SECURITY.md](SECURITY.md)
