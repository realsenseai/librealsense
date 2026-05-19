# React Viewer Test Suite

Automated testing framework for the RealSense React Viewer application.

## Test Structure

```
tests/
├── unit/              # Unit tests for individual components, hooks, utilities
├── integration/       # Integration tests for component interactions
├── e2e/              # End-to-end tests with Playwright
├── mocks/            # Mock data and API handlers
├── setup/            # Test configuration and setup files
└── utils/            # Test utilities and helpers
```

## Running Tests

### Unit & Integration Tests (Vitest)

```bash
# Run all tests
npm test

# Run tests in watch mode
npm test -- --watch

# Run tests with UI
npm run test:ui

# Run tests with coverage
npm run test:coverage

# Run specific test file
npm test Header.test.tsx
```

### E2E Tests (Playwright)

```bash
# Run E2E tests (headless)
npm run test:e2e

# Run E2E tests with UI
npm run test:e2e:ui

# Run specific browser
npm run test:e2e -- --project=chromium

# Debug mode
npm run test:e2e -- --debug
```

## Installation

Install test dependencies:

```bash
npm install
```

Note on coverage provider:

- The Vitest coverage provider (`@vitest/coverage-v8`) is already included in devDependencies; no extra install is required.
- Keep `vitest` and `@vitest/coverage-v8` versions aligned (same major/minor). If you upgrade one, upgrade the other to avoid peer warnings or runtime issues.

Install Playwright browsers (first time only):

```bash
npx playwright install
```

## Writing Tests

### Unit Tests

Use React Testing Library for component tests:

```typescript
import { describe, it, expect } from 'vitest'
import { screen } from '@testing-library/react'
import { render } from '../../utils/test-utils'
import { MyComponent } from '@/components/MyComponent'

describe('MyComponent', () => {
  it('renders correctly', () => {
    render(<MyComponent />)
    expect(screen.getByText('Hello')).toBeInTheDocument()
  })
})
```

### E2E Tests

Use Playwright for full user workflows:

```typescript
import { test, expect } from '@playwright/test'

test('user can start streaming', async ({ page }) => {
  await page.goto('/')
  await page.click('button:has-text("Start")')
  await expect(page.locator('video')).toBeVisible()
})
```

## Mock API

Tests use MSW (Mock Service Worker) to intercept API calls. Handlers are defined in `tests/mocks/api-handlers.ts`.

To add new mock endpoints:

1. Add handler to `api-handlers.ts`
2. Use in tests automatically (MSW intercepts network calls)

## Coverage Goals

- Overall: 70%+
- Critical paths: 90%+
- Components: 80%+
- Utilities: 95%+

## CI/CD

Tests run automatically on:
- Push to any branch
- Pull requests
- Pre-commit hooks (optional)

## Troubleshooting

### Tests fail with WebRTC errors
- WebRTC is mocked in `tests/setup/test-setup.ts`
- Check that mocks are properly initialized

### E2E tests timeout
- Ensure dev server is running: `npm run dev`
- Check that backend is available on `localhost:8000`
- Increase timeout in `playwright.config.ts`

### Coverage seems low
- Run `npm run test:coverage` to see detailed report
- Check `coverage/index.html` for line-by-line coverage

## Future Enhancements

- [ ] Visual regression testing
- [ ] Performance benchmarks
- [ ] Electron/Tauri app testing
- [ ] Accessibility (a11y) tests
- [ ] Component snapshot tests
