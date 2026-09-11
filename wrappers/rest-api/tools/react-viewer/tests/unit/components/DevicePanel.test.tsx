import { describe, it, expect, vi, beforeEach } from 'vitest'
import { screen, waitFor, fireEvent } from '@testing-library/react'
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

    it('shows a device with state without any activate control', () => {
      const mockDevice = createMockDevice()
      const mockDeviceState = createMockDeviceState(mockDevice)
      
      render(<DevicePanel />, {
        initialStoreState: {
          devices: [mockDevice],
          deviceStates: { [mockDevice.device_id]: mockDeviceState },
        },
      })
      
      expect(screen.getByText('RealSense D435')).toBeInTheDocument()
      expect(screen.queryByTitle(/Activate device|Deactivate device/)).not.toBeInTheDocument()
    })
  })

  describe('Record and playback', () => {
    it('offers Record while streaming and switches to pause/stop once recording', async () => {
      const device = createMockDevice()
      const ds = createMockDeviceState(device, { isStreaming: true })
      render(<DevicePanel />, { initialStoreState: { devices: [device], deviceStates: { [device.device_id]: ds } } })

      await userEvent.click(screen.getByRole('button', { name: 'Record' }))

      await waitFor(() => expect(screen.getByRole('button', { name: 'Pause recording' })).toBeInTheDocument())
      await userEvent.click(screen.getByRole('button', { name: 'Stop recording' }))
      await waitFor(() => expect(screen.getByRole('button', { name: 'Record' })).toBeInTheDocument())
    })

    it('disables Record while nothing streams', () => {
      const device = createMockDevice()
      render(<DevicePanel />, { initialStoreState: { devices: [device], deviceStates: { [device.device_id]: createMockDeviceState(device) } } })
      expect(screen.getByRole('button', { name: 'Record' })).toBeDisabled()
    })

    it('shows the transport, a Playback badge and no Record button for a recording', async () => {
      const device = createMockDevice({ device_id: 'playback-clip.db3', is_playback: true, file_name: 'C:/recs/clip.db3' })
      render(<DevicePanel />, { initialStoreState: { devices: [device], deviceStates: { [device.device_id]: createMockDeviceState(device) } } })
      expect(screen.getByText('Playback')).toBeInTheDocument()
      expect(await screen.findByTestId('playback-transport')).toBeInTheDocument()
      expect(screen.queryByRole('button', { name: 'Record' })).not.toBeInTheDocument()
    })

    it('has a Load Recorded Sequence button in the header', () => {
      render(<DevicePanel />)
      expect(screen.getByRole('button', { name: 'Load recorded sequence' })).toBeInTheDocument()
    })
  })

  describe('Device details', () => {
    it('lists every camera info field behind a toggle', async () => {
      const device = createMockDevice({ info: { name: 'RealSense D455', serial_number: '123', usb_type_descriptor: '3.2', product_line: 'D400' } })
      render(<DevicePanel />, { initialStoreState: { devices: [device], deviceStates: { [device.device_id]: createMockDeviceState(device) } } })

      expect(screen.queryByTestId('device-details')).not.toBeInTheDocument()
      await userEvent.click(screen.getByText('Show Device Details'))
      const table = screen.getByTestId('device-details')
      expect(table).toHaveTextContent('Usb Type Descriptor')
      expect(table).toHaveTextContent('3.2')
      expect(table).toHaveTextContent('Product Line')
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

  describe('Advanced Mode', () => {
    const withAdvancedMode = (overrides: Partial<ReturnType<typeof createMockDeviceState>> = {}) => {
      const device = createMockDevice()
      const ds = createMockDeviceState(device, {
        advancedMode: { supported: true, enabled: false },
        ...overrides,
      })
      return { device, ds }
    }

    const openMenu = async () =>
      userEvent.click(await screen.findByTitle('Device actions'))

    it('hides the toggle on devices that do not support advanced mode', async () => {
      const { device, ds } = withAdvancedMode({ advancedMode: { supported: false, enabled: false } })
      render(<DevicePanel />, {
        initialStoreState: { devices: [device], deviceStates: { [device.device_id]: ds } },
      })

      await openMenu()
      expect(screen.queryByRole('button', { name: /Advanced Mode/ })).not.toBeInTheDocument()
    })

    it('toggles without a confirmation prompt', async () => {
      const toggleAdvancedMode = vi.fn().mockResolvedValue(undefined)
      const confirmSpy = vi.spyOn(window, 'confirm').mockReturnValue(false)
      useAppStore.setState({ toggleAdvancedMode })
      const { device, ds } = withAdvancedMode()
      render(<DevicePanel />, {
        initialStoreState: { devices: [device], deviceStates: { [device.device_id]: ds } },
      })

      await openMenu()
      await userEvent.click(screen.getByRole('button', { name: 'Enable Advanced Mode' }))

      expect(confirmSpy).not.toHaveBeenCalled()
      expect(toggleAdvancedMode).toHaveBeenCalledWith(device.device_id, true)
      confirmSpy.mockRestore()
    })

    it('disables the toggle while streaming, as the C++ viewer does', async () => {
      const toggleAdvancedMode = vi.fn().mockResolvedValue(undefined)
      useAppStore.setState({ toggleAdvancedMode })
      const { device, ds } = withAdvancedMode({ isStreaming: true })
      render(<DevicePanel />, {
        initialStoreState: { devices: [device], deviceStates: { [device.device_id]: ds } },
      })

      await openMenu()
      const item = screen.getByRole('button', { name: 'Enable Advanced Mode' })
      expect(item).toBeDisabled()

      await userEvent.click(item)
      expect(toggleAdvancedMode).not.toHaveBeenCalled()
    })

    it('offers to disable once advanced mode is on', async () => {
      const toggleAdvancedMode = vi.fn().mockResolvedValue(undefined)
      useAppStore.setState({ toggleAdvancedMode })
      const { device, ds } = withAdvancedMode({ advancedMode: { supported: true, enabled: true } })
      render(<DevicePanel />, {
        initialStoreState: { devices: [device], deviceStates: { [device.device_id]: ds } },
      })

      await openMenu()
      await userEvent.click(screen.getByRole('button', { name: 'Disable Advanced Mode' }))
      expect(toggleAdvancedMode).toHaveBeenCalledWith(device.device_id, false)
    })
  })

  describe('Control Search', () => {
    // The search covers one sensor's controls and lives inside its panel, so the sensor
    // has to be expanded before there is a box to type in.
    async function renderWithControls(overrides: {
      options?: ReturnType<typeof createMockOption>[]
      setControl?: ReturnType<typeof vi.fn>
    } = {}) {
      const device = createMockDevice()
      const sensor = createMockSensor({ sensor_id: 'sensor-a', name: 'Stereo Module' })
      const options = overrides.options ?? [
        createMockOption({ option_id: 'Exposure', name: 'Exposure' }),
        createMockOption({ option_id: 'Gain', name: 'Gain' }),
        createMockOption({ option_id: 'Laser_Power', name: 'Laser Power' }),
      ]
      const deviceState = createMockDeviceState(device, {
        sensors: [sensor],
        controls: {
          'sensors/sensor-a/options': {
            section: 'Controls', sensorId: 'sensor-a', name: '', options,
          },
        },
      })
      render(<DevicePanel />, {
        initialStoreState: {
          devices: [device],
          deviceStates: { [device.device_id]: deviceState },
          ...(overrides.setControl ? { setControl: overrides.setControl } : {}),
        },
      })
      await userEvent.click(screen.getByText('Stereo Module'))
    }

    it('renders the control search box for an active device', async () => {
      await renderWithControls()
      expect(screen.getByPlaceholderText('Search controls…')).toBeInTheDocument()
    })

    it('filters to matching controls and auto-expands, hiding non-matches', async () => {
      await renderWithControls()
      await userEvent.type(screen.getByPlaceholderText('Search controls…'), 'gain')

      await waitFor(() => expect(screen.getByText('Gain')).toBeInTheDocument())
      expect(screen.queryByText('Exposure')).not.toBeInTheDocument()
      expect(screen.queryByText('Laser Power')).not.toBeInTheDocument()
    })

    it('matches a control name mid-word, case-insensitively', async () => {
      await renderWithControls()
      await userEvent.type(screen.getByPlaceholderText('Search controls…'), 'POWER')

      await waitFor(() => expect(screen.getByText('Laser Power')).toBeInTheDocument())
      expect(screen.queryByText('Gain')).not.toBeInTheDocument()
    })

    it('shows no results for a term that appears in no control label', async () => {
      await renderWithControls()
      await userEvent.type(screen.getByPlaceholderText('Search controls…'), 'option')

      await waitFor(() => expect(screen.getByText(/No controls match/)).toBeInTheDocument())
      expect(screen.queryByText('Exposure')).not.toBeInTheDocument()
      expect(screen.queryByText('Gain')).not.toBeInTheDocument()
    })

    it('restores every option of a category, including ones the search hides', async () => {
      const setControl = vi.fn().mockResolvedValue(undefined)
      await renderWithControls({
        setControl,
        options: [
          createMockOption({
            option_id: 'Exposure', name: 'Exposure',
            current_value: 100, default_value: 50,
          }),
          createMockOption({
            option_id: 'Gain', name: 'Gain',
            current_value: 32, default_value: 16,
          }),
        ],
      })
      await userEvent.type(screen.getByPlaceholderText('Search controls…'), 'gain')
      await waitFor(() => expect(screen.getByText('Gain')).toBeInTheDocument())

      await userEvent.click(screen.getByTitle('Restore Controls to defaults'))

      await waitFor(() => expect(setControl).toHaveBeenCalledTimes(2))
      expect(setControl).toHaveBeenCalledWith('test-device-1', 'sensors/sensor-a/options', 'Exposure', 50)
      expect(setControl).toHaveBeenCalledWith('test-device-1', 'sensors/sensor-a/options', 'Gain', 16)
    })

    it('shows a no-match message and collapses the section again on X', async () => {
      await renderWithControls()
      const input = screen.getByPlaceholderText('Search controls…')
      await userEvent.type(input, 'zzzqqq')

      await waitFor(() => expect(screen.getByText(/No controls match/)).toBeInTheDocument())

      await userEvent.click(screen.getByTitle('Clear search'))
      expect(screen.queryByText(/No controls match/)).not.toBeInTheDocument()
      // The search force-opened the Controls section; clearing it returns the section to
      // the state the user left it in, which is closed.
      expect(screen.queryByText('Exposure')).not.toBeInTheDocument()
    })

    it('types an exact value in text-edit mode and clamps it to the range', async () => {
      const setControl = vi.fn().mockResolvedValue(undefined)
      await renderWithControls({
        setControl,
        options: [createMockOption({ option_id: 'Exposure', current_value: 100, default_value: 50, min_value: 1, max_value: 10000, step: 1 })],
      })
      await userEvent.type(screen.getByPlaceholderText('Search controls…'), 'expo')
      await waitFor(() => expect(screen.getByText('Exposure')).toBeInTheDocument())

      await userEvent.click(screen.getByTitle('Enter text-edit mode'))
      const input = screen.getByLabelText('Exposure value')
      await userEvent.clear(input)
      await userEvent.type(input, '20000{Enter}')

      await waitFor(() => expect(setControl).toHaveBeenCalledWith('test-device-1', 'sensors/sensor-a/options', 'Exposure', 10000))
      expect(screen.queryByLabelText('Exposure value')).not.toBeInTheDocument()
    })

    it('writes a dragged slider value once the interval has passed', async () => {
      const setControl = vi.fn().mockResolvedValue(undefined)
      await renderWithControls({
        setControl,
        options: [createMockOption({ option_id: 'Gain', current_value: 16, default_value: 16, min_value: 0, max_value: 128, step: 1 })],
      })
      await userEvent.type(screen.getByPlaceholderText('Search controls…'), 'gain')
      const slider = await screen.findByRole('slider')

      fireEvent.change(slider, { target: { value: '40' } })
      fireEvent.change(slider, { target: { value: '64' } })

      await waitFor(() => expect(setControl).toHaveBeenCalledTimes(1))
      expect(setControl).toHaveBeenCalledWith('test-device-1', 'sensors/sensor-a/options', 'Gain', 64)
    })

    // Firmware reports no range for some advanced-mode groups, so a flag in one arrives
    // as min == max == its value rather than as 0..1. It is still a flag.
    it('draws a whole-numbered control that cannot leave 0..1 as a checkbox', async () => {
      await renderWithControls({
        options: [createMockOption({
          option_id: 'ignoreSAD', current_value: 0, default_value: 0,
          min_value: 0, max_value: 0, step: 1,
        })],
      })
      await userEvent.type(screen.getByPlaceholderText('Search controls…'), 'ignore')

      await waitFor(() => expect(screen.getByText('Ignore SAD')).toBeInTheDocument())
      expect(screen.getByRole('checkbox')).toBeInTheDocument()
    })
  })
})
