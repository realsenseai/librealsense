import { ReactElement, ReactNode } from 'react'
import { render, RenderOptions } from '@testing-library/react'
import { useAppStore } from '@/store'

/**
 * Custom render function that wraps components with necessary providers
 */
export function renderWithProviders(
  ui: ReactElement,
  options?: RenderOptions & { initialStoreState?: Partial<ReturnType<typeof useAppStore.getState>> }
) {
  const { initialStoreState, ...renderOptions } = options || {}

  // Set initial store state if provided
  if (initialStoreState) {
    useAppStore.setState(initialStoreState as any)
  }

  function Wrapper({ children }: { children: ReactNode }) {
    return <>{children}</>
  }

  return {
    ...render(ui, { wrapper: Wrapper, ...renderOptions }),
    // Return store for assertions
    store: useAppStore,
  }
}

// Re-export everything from React Testing Library
export * from '@testing-library/react'
export { renderWithProviders as render }
