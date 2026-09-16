import { useEffect, useState } from 'react'
import { apiClient } from '../../api/client'
import type { UpdatesReport, UpdateSection } from '../../api/types'

interface UpdatesDialogProps {
  deviceId: string
  deviceName: string
  onClose: () => void
  onInstallFirmware?: () => void
}

function Badge({ verdict }: { verdict: UpdateSection['verdict'] }) {
  const map: Record<UpdateSection['verdict'], [string, string]> = {
    essential: ['ESSENTIAL', 'bg-red-700 text-white'],
    recommended: ['RECOMMENDED', 'bg-yellow-600 text-black'],
    up_to_date: ['UP TO DATE', 'bg-green-700 text-white'],
    unknown: ['UNKNOWN', 'bg-gray-700 text-gray-200'],
  }
  const [label, cls] = map[verdict]
  return <span className={`px-2 py-0.5 rounded text-[10px] font-bold ${cls}`}>{label}</span>
}

function Section({ title, section, install }: { title: string; section: UpdateSection; install?: () => void }) {
  const candidate = section.verdict === 'essential' ? section.essential : section.recommended ?? section.essential
  return (
    <div className="border border-gray-700 rounded-lg p-3 space-y-2">
      <div className="flex items-center justify-between">
        <h3 className="font-semibold text-white">{title}</h3>
        <Badge verdict={section.verdict} />
      </div>
      <div className="grid grid-cols-2 gap-x-4 text-sm">
        <span className="text-gray-400">Current</span><span className="font-mono text-white">{section.current ?? '—'}</span>
        <span className="text-gray-400">Recommended</span><span className="font-mono text-white">{section.recommended?.version ?? '—'}</span>
        <span className="text-gray-400">Essential</span><span className="font-mono text-white">{section.essential?.version ?? '—'}</span>
      </div>
      {candidate?.description && <p className="text-xs text-gray-400">{candidate.description}</p>}
      <div className="flex gap-3 text-sm">
        {candidate?.release_notes && <a href={candidate.release_notes} target="_blank" rel="noopener noreferrer" className="text-rs-blue hover:underline">Release notes</a>}
        {candidate?.link && !install && <a href={candidate.link} target="_blank" rel="noopener noreferrer" className="text-rs-blue hover:underline">Download</a>}
        {install && section.verdict !== 'up_to_date' && section.verdict !== 'unknown' && (
          <button onClick={install} className="px-3 py-1 rounded bg-rs-blue text-white hover:bg-blue-600">Install</button>
        )}
      </div>
    </div>
  )
}

/** The legacy updates window (updates-model.cpp): SOFTWARE and FIRMWARE, each with badges. */
export function UpdatesDialog({ deviceId, deviceName, onClose, onInstallFirmware }: UpdatesDialogProps) {
  const [report, setReport] = useState<UpdatesReport | null>(null)
  const [error, setError] = useState<string | null>(null)

  useEffect(() => {
    let cancelled = false
    apiClient.checkUpdates(deviceId)
      .then((r) => { if (!cancelled) setReport(r) })
      .catch((e) => { if (!cancelled) setError(e instanceof Error ? e.message : 'Failed to check for updates') })
    return () => { cancelled = true }
  }, [deviceId])

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm" role="dialog" aria-label="Updates">
      <div className="bg-rs-dark border border-gray-600 rounded-xl shadow-2xl w-full max-w-lg mx-4 flex flex-col">
        <div className="px-6 py-4 border-b border-gray-700">
          <h2 className="text-lg font-bold text-white">Software &amp; Firmware Updates</h2>
          <p className="text-sm text-gray-400">{deviceName}</p>
        </div>
        <div className="p-6 space-y-4">
          {!report && !error && <p className="text-gray-400 text-sm">Checking the versions database…</p>}
          {error && <p className="text-red-400 text-sm">{error}</p>}
          {report && !report.reachable && <p className="text-yellow-300 text-sm">The versions database could not be reached ({report.source}).</p>}
          {report && (
            <>
              <Section title="SOFTWARE (librealsense)" section={report.software} />
              <Section title="FIRMWARE" section={report.firmware} install={onInstallFirmware} />
            </>
          )}
        </div>
        <div className="px-6 py-4 bg-gray-800/50 flex justify-end">
          <button onClick={onClose} className="px-4 py-2 bg-rs-blue text-white rounded-lg hover:bg-blue-600 text-sm">Close</button>
        </div>
      </div>
    </div>
  )
}
