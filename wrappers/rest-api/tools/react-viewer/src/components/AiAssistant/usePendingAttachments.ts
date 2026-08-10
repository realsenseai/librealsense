// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
//
// Manages the "about to send" image/file attachments for the assistant input: reading files
// into data URIs, capping size, and tracking pending state until the message is sent or cleared.

import { useState } from 'react'
import type { AssistantFileAttachment } from '../../api/assistantChat'

// Keeps a huge/misselected file (e.g. a video) from OOMing the tab as a base64 data URI
// and from bloating the SSE request body.
const MAX_ATTACHMENT_BYTES = 10 * 1024 * 1024

function readFileAsDataUri(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader()
    reader.onload = () => resolve(reader.result as string)
    reader.onerror = () => reject(reader.error)
    reader.readAsDataURL(file)
  })
}

export function usePendingAttachments(onError: (message: string) => void) {
  const [pendingImages, setPendingImages] = useState<string[]>([])
  const [pendingFiles, setPendingFiles] = useState<AssistantFileAttachment[]>([])

  const handleFileChange = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = Array.from(e.target.files || [])
    e.target.value = '' // allow re-selecting the same file later
    for (const file of files) {
      if (file.size > MAX_ATTACHMENT_BYTES) {
        onError(`"${file.name}" is too large to attach (max ${MAX_ATTACHMENT_BYTES / (1024 * 1024)} MB).`)
        continue
      }
      try {
        const dataUri = await readFileAsDataUri(file)
        if (file.type.startsWith('image/')) {
          setPendingImages((prev) => [...prev, dataUri])
        } else {
          setPendingFiles((prev) => [...prev, { dataUri, fileName: file.name, mimeType: file.type || 'application/octet-stream' }])
        }
      } catch {
        onError(`Failed to read "${file.name}".`)
      }
    }
  }

  const removeImage = (index: number) => setPendingImages((prev) => prev.filter((_, i) => i !== index))
  const removeFile = (index: number) => setPendingFiles((prev) => prev.filter((_, i) => i !== index))
  const clear = () => {
    setPendingImages([])
    setPendingFiles([])
  }

  return { pendingImages, pendingFiles, handleFileChange, removeImage, removeFile, clear }
}
