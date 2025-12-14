import { useMemo } from 'react'
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Legend,
  ResponsiveContainer,
} from 'recharts'
import { useAppStore } from '../store'

export function IMUViewer() {
  const { isIMUViewerExpanded, toggleIMUViewer, imuHistory, clearIMUHistory, isStreaming } =
    useAppStore()

  const hasIMUData = imuHistory.accel.length > 0 || imuHistory.gyro.length > 0

  // Format data for charts
  const accelData = useMemo(() => {
    return imuHistory.accel.map((d, i) => ({
      index: i,
      x: d.x,
      y: d.y,
      z: d.z,
    }))
  }, [imuHistory.accel])

  const gyroData = useMemo(() => {
    return imuHistory.gyro.map((d, i) => ({
      index: i,
      x: d.x,
      y: d.y,
      z: d.z,
    }))
  }, [imuHistory.gyro])

  // Get latest values
  const latestAccel = imuHistory.accel[imuHistory.accel.length - 1]
  const latestGyro = imuHistory.gyro[imuHistory.gyro.length - 1]

  return (
    <div className="border-t border-gray-700 bg-rs-dark">
      {/* Header */}
      <button
        onClick={toggleIMUViewer}
        className="w-full flex items-center justify-between p-3 hover:bg-gray-800 transition-colors"
      >
        <div className="flex items-center gap-3">
          <svg
            className="w-5 h-5 text-orange-400"
            fill="none"
            stroke="currentColor"
            viewBox="0 0 24 24"
          >
            <path
              strokeLinecap="round"
              strokeLinejoin="round"
              strokeWidth={2}
              d="M9 19v-6a2 2 0 00-2-2H5a2 2 0 00-2 2v6a2 2 0 002 2h2a2 2 0 002-2zm0 0V9a2 2 0 012-2h2a2 2 0 012 2v10m-6 0a2 2 0 002 2h2a2 2 0 002-2m0 0V5a2 2 0 012-2h2a2 2 0 012 2v14a2 2 0 01-2 2h-2a2 2 0 01-2-2z"
            />
          </svg>
          <span className="font-semibold">IMU Data</span>
          {hasIMUData && (
            <span className="text-xs text-gray-500">
              ({imuHistory.accel.length} accel, {imuHistory.gyro.length} gyro samples)
            </span>
          )}
        </div>
        <svg
          className={`w-5 h-5 transition-transform ${isIMUViewerExpanded ? 'rotate-180' : ''}`}
          fill="none"
          stroke="currentColor"
          viewBox="0 0 24 24"
        >
          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M5 15l7-7 7 7" />
        </svg>
      </button>

      {/* Expanded Content */}
      {isIMUViewerExpanded && (
        <div className="p-4 border-t border-gray-700">
          {!isStreaming ? (
            <div className="text-center text-gray-500 py-8">
              <p>Start streaming with IMU sensors enabled to see data</p>
            </div>
          ) : !hasIMUData ? (
            <div className="text-center text-gray-500 py-8">
              <p>No IMU data received</p>
              <p className="text-sm mt-1">Make sure accelerometer and gyroscope streams are enabled</p>
            </div>
          ) : (
            <>
              {/* Current Values Display */}
              <div className="grid grid-cols-2 gap-4 mb-4">
                {/* Accelerometer */}
                <div className="bg-gray-800 rounded-lg p-3">
                  <div className="flex items-center justify-between mb-2">
                    <h3 className="font-semibold text-orange-400">Accelerometer</h3>
                    <span className="text-xs text-gray-500">m/s²</span>
                  </div>
                  {latestAccel ? (
                    <div className="grid grid-cols-3 gap-2 text-sm">
                      <div>
                        <span className="text-red-400">X:</span>{' '}
                        <span className="font-mono">{latestAccel.x.toFixed(3)}</span>
                      </div>
                      <div>
                        <span className="text-green-400">Y:</span>{' '}
                        <span className="font-mono">{latestAccel.y.toFixed(3)}</span>
                      </div>
                      <div>
                        <span className="text-blue-400">Z:</span>{' '}
                        <span className="font-mono">{latestAccel.z.toFixed(3)}</span>
                      </div>
                    </div>
                  ) : (
                    <p className="text-gray-500 text-sm">No data</p>
                  )}
                </div>

                {/* Gyroscope */}
                <div className="bg-gray-800 rounded-lg p-3">
                  <div className="flex items-center justify-between mb-2">
                    <h3 className="font-semibold text-red-400">Gyroscope</h3>
                    <span className="text-xs text-gray-500">rad/s</span>
                  </div>
                  {latestGyro ? (
                    <div className="grid grid-cols-3 gap-2 text-sm">
                      <div>
                        <span className="text-red-400">X:</span>{' '}
                        <span className="font-mono">{latestGyro.x.toFixed(3)}</span>
                      </div>
                      <div>
                        <span className="text-green-400">Y:</span>{' '}
                        <span className="font-mono">{latestGyro.y.toFixed(3)}</span>
                      </div>
                      <div>
                        <span className="text-blue-400">Z:</span>{' '}
                        <span className="font-mono">{latestGyro.z.toFixed(3)}</span>
                      </div>
                    </div>
                  ) : (
                    <p className="text-gray-500 text-sm">No data</p>
                  )}
                </div>
              </div>

              {/* Charts */}
              <div className="grid grid-cols-2 gap-4">
                {/* Accelerometer Chart */}
                {accelData.length > 0 && (
                  <div className="bg-gray-800 rounded-lg p-3">
                    <h4 className="text-sm font-semibold mb-2 text-orange-400">
                      Accelerometer History
                    </h4>
                    <div className="h-40">
                      <ResponsiveContainer width="100%" height="100%">
                        <LineChart data={accelData}>
                          <CartesianGrid strokeDasharray="3 3" stroke="#444" />
                          <XAxis dataKey="index" tick={false} stroke="#666" />
                          <YAxis stroke="#666" fontSize={10} />
                          <Tooltip
                            contentStyle={{ backgroundColor: '#1a1a2e', border: 'none' }}
                            labelStyle={{ color: '#888' }}
                          />
                          <Legend wrapperStyle={{ fontSize: '10px' }} />
                          <Line
                            type="monotone"
                            dataKey="x"
                            stroke="#ef4444"
                            dot={false}
                            strokeWidth={1}
                          />
                          <Line
                            type="monotone"
                            dataKey="y"
                            stroke="#22c55e"
                            dot={false}
                            strokeWidth={1}
                          />
                          <Line
                            type="monotone"
                            dataKey="z"
                            stroke="#3b82f6"
                            dot={false}
                            strokeWidth={1}
                          />
                        </LineChart>
                      </ResponsiveContainer>
                    </div>
                  </div>
                )}

                {/* Gyroscope Chart */}
                {gyroData.length > 0 && (
                  <div className="bg-gray-800 rounded-lg p-3">
                    <h4 className="text-sm font-semibold mb-2 text-red-400">Gyroscope History</h4>
                    <div className="h-40">
                      <ResponsiveContainer width="100%" height="100%">
                        <LineChart data={gyroData}>
                          <CartesianGrid strokeDasharray="3 3" stroke="#444" />
                          <XAxis dataKey="index" tick={false} stroke="#666" />
                          <YAxis stroke="#666" fontSize={10} />
                          <Tooltip
                            contentStyle={{ backgroundColor: '#1a1a2e', border: 'none' }}
                            labelStyle={{ color: '#888' }}
                          />
                          <Legend wrapperStyle={{ fontSize: '10px' }} />
                          <Line
                            type="monotone"
                            dataKey="x"
                            stroke="#ef4444"
                            dot={false}
                            strokeWidth={1}
                          />
                          <Line
                            type="monotone"
                            dataKey="y"
                            stroke="#22c55e"
                            dot={false}
                            strokeWidth={1}
                          />
                          <Line
                            type="monotone"
                            dataKey="z"
                            stroke="#3b82f6"
                            dot={false}
                            strokeWidth={1}
                          />
                        </LineChart>
                      </ResponsiveContainer>
                    </div>
                  </div>
                )}
              </div>

              {/* Actions */}
              <div className="mt-4 flex gap-2">
                <button onClick={clearIMUHistory} className="control-button-secondary text-sm">
                  Clear History
                </button>
                <button
                  onClick={() => exportIMUData(imuHistory)}
                  className="control-button-secondary text-sm"
                >
                  Export CSV
                </button>
              </div>
            </>
          )}
        </div>
      )}
    </div>
  )
}

function exportIMUData(history: {
  accel: { timestamp: number; x: number; y: number; z: number }[]
  gyro: { timestamp: number; x: number; y: number; z: number }[]
}) {
  let csvContent = 'type,timestamp,x,y,z\n'

  for (const data of history.accel) {
    csvContent += `accel,${data.timestamp},${data.x},${data.y},${data.z}\n`
  }

  for (const data of history.gyro) {
    csvContent += `gyro,${data.timestamp},${data.x},${data.y},${data.z}\n`
  }

  const blob = new Blob([csvContent], { type: 'text/csv' })
  const url = URL.createObjectURL(blob)
  const link = document.createElement('a')
  link.href = url
  link.download = `imu_data_${Date.now()}.csv`
  document.body.appendChild(link)
  link.click()
  document.body.removeChild(link)
  URL.revokeObjectURL(url)
}
