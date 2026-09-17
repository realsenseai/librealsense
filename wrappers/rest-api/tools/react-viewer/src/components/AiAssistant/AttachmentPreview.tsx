// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
//
// Thumbnail row for images/files staged for the next message, with per-item remove buttons.
// Split out of ChatInput.tsx to keep that component focused on submission logic.

import { X, FileText } from 'lucide-react'
import type { AssistantFileAttachment } from '../../api/assistantChat'

interface AttachmentPreviewProps {
  pendingImages: string[]
  pendingFiles: AssistantFileAttachment[]
  onRemoveImage: (index: number) => void
  onRemoveFile: (index: number) => void
}

export function AttachmentPreview({ pendingImages, pendingFiles, onRemoveImage, onRemoveFile }: AttachmentPreviewProps) {
  if (pendingImages.length === 0 && pendingFiles.length === 0) return null

  return (
    <div className="flex flex-wrap gap-1.5 mb-2">
      {pendingImages.map((dataUri, i) => (
        <div key={i} className="relative w-12 h-12 rounded overflow-hidden border border-gray-600 shrink-0">
          <img src={dataUri} alt="" className="w-full h-full object-cover" />
          <button
            type="button"
            onClick={() => onRemoveImage(i)}
            className="absolute top-0 right-0 bg-black/60 text-white rounded-bl p-0.5"
            title="Remove image"
          >
            <X className="w-3 h-3" />
          </button>
        </div>
      ))}
      {pendingFiles.map((file, i) => (
        <div
          key={i}
          className="flex items-center gap-1 pl-2 pr-1 py-1 rounded border text-xs bg-gray-800 border-gray-600 text-gray-300"
        >
          <FileText className="w-3.5 h-3.5 shrink-0" />
          <span className="max-w-[120px] truncate">{file.fileName}</span>
          <button type="button" onClick={() => onRemoveFile(i)} className="text-gray-400 hover:text-white" title="Remove file">
            <X className="w-3.5 h-3.5" />
          </button>
        </div>
      ))}
    </div>
  )
}
