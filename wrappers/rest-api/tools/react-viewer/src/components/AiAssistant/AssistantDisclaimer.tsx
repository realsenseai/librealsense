// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
//
// One-time disclaimer overlaid on the assistant body until the user accepts it.

export function AssistantDisclaimer({ onAccept }: { onAccept: () => void }) {
  return (
    <div className="absolute inset-0 z-10 flex items-center justify-center p-3 bg-black/60">
      <div
        role="alertdialog"
        aria-modal="true"
        aria-labelledby="rsai-disclaimer-title"
        className="w-full rounded-lg border border-gray-600 bg-rs-darker p-5 shadow-2xl"
      >
        <h4 id="rsai-disclaimer-title" className="text-lg font-semibold text-white mb-2">Disclaimer</h4>
        <p className="text-sm leading-relaxed text-gray-300">
          RealSense does not guarantee the accuracy, completeness, or up-to-date nature of the information
          provided by the AI Assistant. Users of the AI Assistant bear sole responsibility for their interactions
          and reliance on the information provided. By using the AI Assistant, you acknowledge and accept these
          terms. For any critical, sensitive, or complex inquiries, please{' '}
          <a
            href="https://github.com/realsenseai/librealsense/issues/new"
            target="_blank"
            rel="noopener noreferrer"
            className="text-sky-400 hover:underline"
          >
            contact the RealSense team
          </a>{' '}
          directly for confirmation and further assistance.
        </p>
        <button
          onClick={onAccept}
          className="mt-4 w-full rounded-md bg-rs-blue py-2 text-sm font-semibold text-white transition-colors hover:bg-blue-600"
        >
          Accept &amp; Continue
        </button>
      </div>
    </div>
  )
}
