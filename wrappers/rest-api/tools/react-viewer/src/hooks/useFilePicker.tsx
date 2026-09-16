// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

import { useRef, type ReactElement } from 'react'

/**
 * Hidden-file-input hook. Returns the JSX to render once at a stable location in the tree (so
 * the OS file picker callback fires even after the menu that triggered it unmounts) and an
 * `open()` function to trigger it.
 */
export function useFilePicker(onPick: (file: File) => void, accept: string): {
  open: () => void
  input: ReactElement
} {
  const ref = useRef<HTMLInputElement>(null)
  const open = () => ref.current?.click()
  const input = (
    <input
      ref={ref}
      type="file"
      accept={accept}
      className="hidden"
      onClick={(e) => e.stopPropagation()}
      onChange={(e) => {
        const f = e.target.files?.[0]
        if (f) onPick(f)
        // Reset so selecting the same file again still triggers onChange.
        e.target.value = ''
      }}
    />
  )
  return { open, input }
}
