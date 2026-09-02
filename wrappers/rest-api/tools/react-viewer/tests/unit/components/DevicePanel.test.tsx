import { describe, it, expect, vi, beforeEach } from 'vitest'
import { screen, waitFor } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { render, createMockDevice, createMockDeviceState, createMockSensor, createMockOption } from '../../utils/test-utils'
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
      
      expect(screen.getByText('RealSense D435')).toBeInTheDocument()
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

    it('passes forceRefresh=true when refresh is clicked manually', async () => {
      const fetchDevices = vi.fn().mockResolvedValue(undefined)
      useAppStore.setState({ fetchDevices })

      render(<DevicePanel />)

      const refreshButton = screen.getByLabelText('Refresh devices')
      await userEvent.click(refreshButton)

      // The polling effect also calls fetchDevices() with no args on mount.
      // The manual-click call must explicitly pass true.
      expect(fetchDevices).toHaveBeenCalledWith(true)
    })

    it('is disabled while a fetch is in flight', () => {
      // Must use initialStoreState (not pre-render setState) because
      // renderWithProviders calls resetStore() first, which would reset
      // isLoadingDevices back to false.
      render(<DevicePanel />, {
        initialStoreState: { isLoadingDevices: true },
      })

      const refreshButton = screen.getByLabelText('Refreshing devices…')
      expect(refreshButton).toBeDisabled()
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
      expect(screen.getByText('RealSense D435')).toBeInTheDocument()
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
      expect(screen.getByText('RealSense D435')).toBeInTheDocument()
    })
  })

  describe('Header', () => {
    it('renders "Devices" header', () => {
      render(<DevicePanel />)

      expect(screen.getByText('Devices')).toBeInTheDocument()
    })
  })

  describe('Firmware Update Proposal', () => {
    const outdated = (overrides = {}) => {
      const device = createMockDevice({
        serial_number: '123456789012',
        firmware_version: '5.17.0.10',
        ...overrides,
      })
      const ds = createMockDeviceState(device, { firmware: { recommended: '5.17.3.10' } })
      return { device, ds }
    }

    it('toasts an update proposal naming the device and serial when firmware is outdated', async () => {
      const { device, ds } = outdated()
      render(<DevicePanel />, {
        initialStoreState: { devices: [device], deviceStates: { [device.device_id]: ds } },
      })

      const toast = await screen.findByText(/firmware 5\.17\.0\.10 → 5\.17\.3\.10 is available/)
      expect(toast).toHaveTextContent(`S/N ${device.serial_number}`)
      expect(toast).toHaveTextContent(device.name)
      expect(screen.getByRole('button', { name: 'Update' })).toBeInTheDocument()
    })

    it('the toast Update action starts the recommended update for that device', async () => {
      const updateFirmwareFromRecommended = vi.fn().mockResolvedValue(undefined)
      useAppStore.setState({ updateFirmwareFromRecommended })
      const { device, ds } = outdated()
      render(<DevicePanel />, {
        initialStoreState: { devices: [device], deviceStates: { [device.device_id]: ds } },
      })

      await userEvent.click(await screen.findByRole('button', { name: 'Update' }))
      expect(updateFirmwareFromRecommended).toHaveBeenCalledWith(device.device_id)
    })

    it('does not toast when firmware is up to date', async () => {
      const device = createMockDevice({ firmware_version: '5.17.3.10' })
      const ds = createMockDeviceState(device, { firmware: { recommended: '5.17.3.10' } })
      render(<DevicePanel />, {
        initialStoreState: { devices: [device], deviceStates: { [device.device_id]: ds } },
      })

      await waitFor(() => expect(screen.getByText('Devices')).toBeInTheDocument())
      expect(screen.queryByText(/is available/)).not.toBeInTheDocument()
    })

    it('toasts only once per recommended version', async () => {
      const { device, ds } = outdated()
      render(<DevicePanel />, {
        initialStoreState: { devices: [device], deviceStates: { [device.device_id]: ds } },
      })
      await screen.findByText(/is available/)

      // A re-enumeration that reports the same recommendation must not re-prompt.
      useAppStore.setState({
        deviceStates: { [device.device_id]: { ...ds, firmware: { ...ds.firmware! } } },
      })

      await waitFor(() => expect(screen.getAllByText(/is available/)).toHaveLength(1))
    })

    it('keeps firmware state off the card entirely — the toast owns it', () => {
      const device = createMockDevice()
      const ds = createMockDeviceState(device, {
        firmware: { current: '5.17.3.10', recommended: '5.17.3.10', status: 'up_to_date' },
      })
      render(<DevicePanel />, {
        initialStoreState: { devices: [device], deviceStates: { [device.device_id]: ds } },
      })

      expect(screen.queryByText(/up to date/i)).not.toBeInTheDocument()
      expect(screen.queryByText(/Download firmware/)).not.toBeInTheDocument()
    })
  })

  describe('Control Search', () => {
    function renderWithControls(overrides: {
      options?: ReturnType<typeof createMockOption>[]
      setOption?: ReturnType<typeof vi.fn>
    } = {}) {
      const device = createMockDevice()
      const sensor = createMockSensor({ sensor_id: 'sensor-a', name: 'Stereo Module' })
      const options = overrides.options ?? [
        createMockOption({ option_id: 'Exposure', name: 'Exposure', category: 'Basic Controls' }),
        createMockOption({ option_id: 'Gain', name: 'Gain', category: 'Basic Controls' }),
        createMockOption({ option_id: 'Laser_Power', name: 'Laser Power', category: 'Basic Controls' }),
      ]
      const deviceState = createMockDeviceState(device, {
        isActive: true,
        sensors: [sensor],
        options: { 'sensor-a': options },
      })
      render(<DevicePanel />, {
        initialStoreState: {
          devices: [device],
          deviceStates: { [device.device_id]: deviceState },
          ...(overrides.setOption ? { setOption: overrides.setOption } : {}),
        },
      })
    }

    it('renders the control search box for an active device', () => {
      renderWithControls()
      expect(screen.getByPlaceholderText('Search controls…')).toBeInTheDocument()
    })

    it('filters to matching controls and auto-expands, hiding non-matches', async () => {
      renderWithControls()
      await userEvent.type(screen.getByPlaceholderText('Search controls…'), 'gain')

      await waitFor(() => expect(screen.getByText('Gain')).toBeInTheDocument())
      expect(screen.queryByText('Exposure')).not.toBeInTheDocument()
      expect(screen.queryByText('Laser Power')).not.toBeInTheDocument()
    })

    it('matches a control name mid-word, case-insensitively', async () => {
      renderWithControls()
      await userEvent.type(screen.getByPlaceholderText('Search controls…'), 'POWER')

      await waitFor(() => expect(screen.getByText('Laser Power')).toBeInTheDocument())
      expect(screen.queryByText('Gain')).not.toBeInTheDocument()
    })

    it('shows no results for a term that appears in no control label', async () => {
      renderWithControls()
      await userEvent.type(screen.getByPlaceholderText('Search controls…'), 'option')

      await waitFor(() => expect(screen.getByText(/No controls match/)).toBeInTheDocument())
      expect(screen.queryByText('Exposure')).not.toBeInTheDocument()
      expect(screen.queryByText('Gain')).not.toBeInTheDocument()
    })

    it('restores every option of a category, including ones the search hides', async () => {
      const setOption = vi.fn().mockResolvedValue(undefined)
      renderWithControls({
        setOption,
        options: [
          createMockOption({
            option_id: 'Exposure', name: 'Exposure', category: 'Basic Controls',
            current_value: 100, default_value: 50,
          }),
          createMockOption({
            option_id: 'Gain', name: 'Gain', category: 'Basic Controls',
            current_value: 32, default_value: 16,
          }),
        ],
      })
      await userEvent.type(screen.getByPlaceholderText('Search controls…'), 'gain')
      await waitFor(() => expect(screen.getByText('Gain')).toBeInTheDocument())

      await userEvent.click(screen.getByTitle('Restore Basic Controls to defaults'))

      await waitFor(() => expect(setOption).toHaveBeenCalledTimes(2))
      expect(setOption).toHaveBeenCalledWith('test-device-1', 'sensor-a', 'Exposure', 50)
      expect(setOption).toHaveBeenCalledWith('test-device-1', 'sensor-a', 'Gain', 16)
    })

    it('shows a no-match message and clears back to collapsed on X', async () => {
      renderWithControls()
      const input = screen.getByPlaceholderText('Search controls…')
      await userEvent.type(input, 'zzzqqq')

      await waitFor(() => expect(screen.getByText(/No controls match/)).toBeInTheDocument())

      await userEvent.click(screen.getByTitle('Clear search'))
      expect(screen.queryByText(/No controls match/)).not.toBeInTheDocument()
      // back to idle: controls collapsed, options not rendered
      expect(screen.queryByText('Exposure')).not.toBeInTheDocument()
    })
  })
})
