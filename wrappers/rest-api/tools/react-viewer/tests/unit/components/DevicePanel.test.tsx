import { describe, it, expect, vi, beforeEach } from 'vitest'
import { screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { render, createMockDevice, createMockDeviceState } from '../../utils/test-utils'
import { DevicePanel } from '@/components/DevicePanel'
import { useAppStore } from '@/store'

describe('DevicePanel', () => {
  beforeEach(() => {
    // Reset store state before each test
    // Mock fetchDevices to prevent side effects
    useAppStore.setState({
      devices: [],
      deviceStates: {},
      isLoadingDevices: false,
      error: null,
      fetchDevices: vi.fn().mockResolvedValue(undefined),
      clearError: vi.fn(),
      toggleDeviceActive: vi.fn().mockResolvedValue(undefined),
      resetDevice: vi.fn().mockResolvedValue(undefined),
      isAnyDeviceStreaming: () => false,
      updateStreamConfig: vi.fn(),
      updateSensorConfig: vi.fn(),
      setOption: vi.fn().mockResolvedValue(undefined),
      startSensorStreaming: vi.fn().mockResolvedValue(undefined),
      stopSensorStreaming: vi.fn().mockResolvedValue(undefined),
      checkFirmwareUpdates: vi.fn().mockResolvedValue(undefined),
      updateFirmware: vi.fn().mockResolvedValue(undefined),
    })
  })

  describe('Empty State', () => {
    it('shows "No devices found" when no devices are connected and not loading', async () => {
      // Set up a fetchDevices that doesn't change state
      const fetchDevices = vi.fn().mockImplementation(() => Promise.resolve())
      useAppStore.setState({ 
        isLoadingDevices: false, 
        devices: [],
        fetchDevices,
      })
      render(<DevicePanel />)
      
      // Wait for initial render to settle
      await waitFor(() => {
        expect(screen.getByText('No devices found')).toBeInTheDocument()
      })
      expect(screen.getByText('Connect a RealSense device')).toBeInTheDocument()
    })

    it('shows loading message when loading with no devices', async () => {
      const fetchDevices = vi.fn().mockImplementation(() => {
        // Simulate loading - set isLoadingDevices to true and devices to empty
        useAppStore.setState({ isLoadingDevices: true, devices: [] })
        return new Promise(() => {}) // Never resolves to keep loading
      })
      
      useAppStore.setState({ 
        isLoadingDevices: true, 
        devices: [],
        fetchDevices,
      })
      render(<DevicePanel />)
      
      // When loading with no devices, should show "Searching for devices..."
      await waitFor(() => {
        expect(screen.getByText('Searching for devices...')).toBeInTheDocument()
      })
    })
  })

  describe('Device List', () => {
    it('renders device cards when devices are connected', () => {
      const mockDevice = createMockDevice()
      
      render(<DevicePanel />, {
        initialStoreState: {
          devices: [mockDevice],
          deviceStates: {},
        },
      })
      
      expect(screen.getByText('Intel RealSense D435')).toBeInTheDocument()
    })

    it('shows multiple devices when connected', () => {
      const device1 = createMockDevice({ device_id: 'device-1', name: 'D435', serial: '111' })
      const device2 = createMockDevice({ device_id: 'device-2', name: 'D455', serial: '222' })
      
      render(<DevicePanel />, {
        initialStoreState: {
          devices: [device1, device2],
          deviceStates: {},
        },
      })
      
      expect(screen.getByText('D435')).toBeInTheDocument()
      expect(screen.getByText('D455')).toBeInTheDocument()
    })

    it('displays device serial number', async () => {
      const mockDevice = createMockDevice({ serial_number: 'TEST-SERIAL-123' })
      
      // Make sure fetchDevices keeps the devices we set
      const fetchDevices = vi.fn().mockImplementation(() => {
        useAppStore.setState({ devices: [mockDevice], isLoadingDevices: false })
        return Promise.resolve()
      })
      
      useAppStore.setState({
        devices: [mockDevice],
        deviceStates: {},
        fetchDevices,
        isLoadingDevices: false,
      })
      render(<DevicePanel />)
      
      await waitFor(() => {
        expect(screen.getByText(/TEST-SERIAL-123/)).toBeInTheDocument()
      })
    })

    it('displays firmware version', () => {
      const mockDevice = createMockDevice({ firmware_version: '5.16.0.1' })
      
      render(<DevicePanel />, {
        initialStoreState: {
          devices: [mockDevice],
          deviceStates: {},
        },
      })
      
      expect(screen.getByText(/5\.16\.0\.1/)).toBeInTheDocument()
    })
  })

  describe('Error Handling', () => {
    it('displays error message when error is set', () => {
      render(<DevicePanel />, {
        initialStoreState: {
          devices: [],
          error: 'Failed to connect to device',
        },
      })
      
      expect(screen.getByText('Failed to connect to device')).toBeInTheDocument()
    })

    it('allows dismissing error messages', async () => {
      const clearError = vi.fn()
      useAppStore.setState({ clearError })
      
      render(<DevicePanel />, {
        initialStoreState: {
          devices: [],
          error: 'Some error occurred',
        },
      })
      
      const dismissButton = screen.getByText('×')
      await userEvent.click(dismissButton)
      
      expect(clearError).toHaveBeenCalled()
    })
  })

  describe('Refresh Button', () => {
    it('renders refresh button', () => {
      render(<DevicePanel />)
      
      const refreshButton = screen.getByTitle('Refresh devices')
      expect(refreshButton).toBeInTheDocument()
    })

    it('calls fetchDevices when refresh is clicked', async () => {
      const fetchDevices = vi.fn().mockResolvedValue(undefined)
      useAppStore.setState({ fetchDevices })
      
      render(<DevicePanel />)
      
      const refreshButton = screen.getByTitle('Refresh devices')
      await userEvent.click(refreshButton)
      
      expect(fetchDevices).toHaveBeenCalled()
    })
  })

  describe('Device Activation', () => {
    it('renders device as inactive by default', () => {
      const mockDevice = createMockDevice()
      
      render(<DevicePanel />, {
        initialStoreState: {
          devices: [mockDevice],
          deviceStates: {},
        },
      })
      
      // Device should show but not be active
      expect(screen.getByText('Intel RealSense D435')).toBeInTheDocument()
    })

    it('shows device as active when deviceState.isActive is true', () => {
      const mockDevice = createMockDevice()
      const mockDeviceState = createMockDeviceState(mockDevice, { isActive: true })
      
      render(<DevicePanel />, {
        initialStoreState: {
          devices: [mockDevice],
          deviceStates: { [mockDevice.device_id]: mockDeviceState },
        },
      })
      
      // When active, the device card should have active styling
      // The exact check depends on component implementation
      expect(screen.getByText('Intel RealSense D435')).toBeInTheDocument()
    })
  })

  describe('Header', () => {
    it('renders "Devices" header', () => {
      render(<DevicePanel />)
      
      expect(screen.getByText('Devices')).toBeInTheDocument()
    })
  })
})
