# Testing Framework Installation

## Step 1: Install Dependencies

```bash
cd wrappers/react-viewer
npm install
```

This will install all testing dependencies including:
- Vitest (unit/integration testing)
- React Testing Library
- Playwright (E2E testing)
- MSW (API mocking)
- jsdom (browser environment simulation)

## Step 2: Install Playwright Browsers

```bash
npx playwright install
```

This downloads Chromium, Firefox, and WebKit browsers for E2E testing.

## Step 3: Verify Installation

Run the smoke test:

```bash
# Unit tests
npm test

# E2E tests (requires dev server running)
npm run test:e2e
```

## Step 4: Run with Coverage

```bash
npm run test:coverage
```

Open `coverage/index.html` in your browser to see detailed coverage report.

## Quick Start

### Run Tests in Watch Mode (Development)

```bash
npm test -- --watch
```

Changes to test files or source files will automatically re-run tests.

### Run Tests with UI

```bash
npm run test:ui
```

Opens Vitest UI in browser for interactive test exploration.

### Debug E2E Tests

```bash
npm run test:e2e:ui
```

Opens Playwright UI for debugging E2E tests step-by-step.

## Next Steps

1. Review existing tests in `tests/unit/components/Header.test.tsx`
2. Review E2E smoke tests in `tests/e2e/smoke.spec.ts`
3. Add new tests following the patterns in `tests/README.md`

## Troubleshooting

### "Cannot find module '@/components/...'"

The `@/` alias is configured in both `vite.config.ts` and `vitest.config.ts`. If tests fail to resolve imports, verify the alias configuration matches.

### E2E tests fail to connect

Ensure the dev server is running:
```bash
npm run dev
```

And backend is available on `http://localhost:8000`

### MSW warnings in console

MSW will warn about unhandled requests. Add handlers in `tests/mocks/api-handlers.ts` for any new API endpoints.
