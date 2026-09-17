// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
//
// Shared input footer for both the AI Assistant and Chatbot modes. Owns submission state and
// delegates the attachment-preview thumbnails and the input/toolbar row to their own components
// (AttachmentPreview / ChatInputControls) so this file stays focused on submission logic.
// Differences between modes (accent color, placeholder, submit handler, whether attachments are
// enabled) are passed in as props.

import { useEffect, useRef, useState } from 'react'
import { usePendingAttachments } from './usePendingAttachments'
import { AttachmentPreview } from './AttachmentPreview'
import { ChatInputControls } from './ChatInputControls'
import type { AssistantFileAttachment } from '../../api/assistantChat'

export interface ChatInputAttachments {
  imageDataUris?: string[]
  fileDataUris?: AssistantFileAttachment[]
}

interface ChatInputProps {
  placeholder: string
  accent: 'blue' | 'amber'
  isLoading: boolean
  attachmentsEnabled: boolean
  onSend: (message: string, attachments?: ChatInputAttachments) => void
  onStop: () => void
  onAttachmentError?: (message: string) => void
}

const ACCENT_CLASSES = {
  blue: { button: 'bg-rs-blue hover:bg-blue-600', focus: 'focus:border-rs-blue' },
  amber: { button: 'bg-amber-600 hover:bg-amber-500', focus: 'focus:border-amber-500' },
} as const

export function ChatInput({
  placeholder,
  accent,
  isLoading,
  attachmentsEnabled,
  onSend,
  onStop,
  onAttachmentError = () => {},
}: ChatInputProps) {
  const [inputValue, setInputValue] = useState('')
  const inputRef = useRef<HTMLInputElement>(null)

  const { pendingImages, pendingFiles, handleFileChange, removeImage, removeFile, clear: clearAttachments } =
    usePendingAttachments(onAttachmentError)

  useEffect(() => {
    inputRef.current?.focus()
  }, [])

  const hasContent = Boolean(inputValue.trim() || pendingImages.length || pendingFiles.length)

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault()
    if (!hasContent || isLoading) return
    const hasAttachments = Boolean(pendingImages.length || pendingFiles.length)
    const message = inputValue.trim() || (hasAttachments ? 'Please analyze the attached file(s).' : '')
    setInputValue('')
    const attachments = hasAttachments ? { imageDataUris: pendingImages, fileDataUris: pendingFiles } : undefined
    clearAttachments()
    onSend(message, attachments)
  }

  const colors = ACCENT_CLASSES[accent]

  return (
    <form onSubmit={handleSubmit} className="p-3 border-t bg-rs-darker border-gray-700">
      {attachmentsEnabled && (
        <AttachmentPreview
          pendingImages={pendingImages}
          pendingFiles={pendingFiles}
          onRemoveImage={removeImage}
          onRemoveFile={removeFile}
        />
      )}
      <ChatInputControls
        inputRef={inputRef}
        value={inputValue}
        onChange={setInputValue}
        placeholder={placeholder}
        isLoading={isLoading}
        hasContent={hasContent}
        attachmentsEnabled={attachmentsEnabled}
        onFileChange={handleFileChange}
        onStop={onStop}
        accentButtonClass={colors.button}
        accentFocusClass={colors.focus}
      />
    </form>
  )
}
