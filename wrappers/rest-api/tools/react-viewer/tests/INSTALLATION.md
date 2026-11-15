# Testing Framework Installation

> **Quickest path:** from `wrappers/rest-api/`, run `python run_tests.py` to execute the
> backend pytest suite and the React viewer Vitest suite together. The steps below cover
> the full per-tool setup and the optional Playwright E2E configuration.

## Step 1: Install Dependencies

```bash
cd wrappers/rest-api/tools/react-viewer
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

# E2E tests (Playwright starts the server; build the viewer first)
npm run build && npm run bundle
npm run test:e2e -- --grep-invert @real-device
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

### Debug E2E Tests

```bash
npm run test:e2e -- --debug
```

Opens the Playwright Inspector for stepping through E2E tests.

## Next Steps

1. Review existing tests in `tests/unit/components/Header.test.tsx`
2. Review E2E smoke tests in `tests/e2e/smoke.spec.ts`
3. Add new tests following the patterns in `tests/README.md`

## Troubleshooting

### "Cannot find module '@/components/...'"

The `@/` alias is configured in both `vite.config.ts` and `vitest.config.ts`. If tests fail to resolve imports, verify the alias configuration matches.

### E2E tests fail to connect

Ensure the viewer is built, so the server has something to serve:
```bash
npm run build && npm run bundle
```

Playwright starts the server itself; override its address with `API_URL`.

### MSW warnings in console

MSW will warn about unhandled requests. Add handlers in `tests/mocks/api-handlers.ts` for any new API endpoints.
