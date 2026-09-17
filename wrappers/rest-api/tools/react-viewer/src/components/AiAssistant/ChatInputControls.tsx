// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
//
// Text input + optional file-attach button + send/stop button, in one row. Split out of
// ChatInput.tsx to keep that component focused on submission logic.

import { useRef, type RefObject } from 'react'
import { Send, Square, Paperclip } from 'lucide-react'

interface ChatInputControlsProps {
  inputRef: RefObject<HTMLInputElement>
  value: string
  onChange: (value: string) => void
  placeholder: string
  isLoading: boolean
  hasContent: boolean
  attachmentsEnabled: boolean
  onFileChange: (e: React.ChangeEvent<HTMLInputElement>) => void
  onStop: () => void
  accentButtonClass: string
  accentFocusClass: string
}

export function ChatInputControls({
  inputRef,
  value,
  onChange,
  placeholder,
  isLoading,
  hasContent,
  attachmentsEnabled,
  onFileChange,
  onStop,
  accentButtonClass,
  accentFocusClass,
}: ChatInputControlsProps) {
  const fileInputRef = useRef<HTMLInputElement>(null)

  return (
    <div className="flex items-center gap-2">
      <input
        ref={inputRef}
        type="text"
        value={value}
        onChange={(e) => onChange(e.target.value)}
        placeholder={placeholder}
        disabled={isLoading}
        className={`flex-1 px-3 py-2 border rounded-lg focus:outline-none text-sm bg-gray-800 border-gray-600 text-white placeholder-gray-500 ${accentFocusClass}`}
      />
      {attachmentsEnabled && (
        <>
          <input
            ref={fileInputRef}
            type="file"
            multiple
            onChange={onFileChange}
            className="hidden"
            accept="image/*,.pdf,.doc,.docx,.txt"
          />
          <button
            type="button"
            onClick={() => fileInputRef.current?.click()}
            title="Attach a file"
            className="p-2 rounded-lg transition-colors text-gray-400 hover:text-white hover:bg-gray-700"
          >
            <Paperclip className="w-4 h-4" />
          </button>
        </>
      )}
      {isLoading ? (
        <button
          type="button"
          onClick={onStop}
          title="Stop generating"
          className="p-2 bg-gray-700 text-white rounded-lg hover:bg-gray-600 transition-colors"
        >
          <Square className="w-4 h-4" />
        </button>
      ) : (
        <button
          type="submit"
          disabled={!hasContent}
          className={`p-2 text-white rounded-lg disabled:opacity-50 disabled:cursor-not-allowed transition-colors ${accentButtonClass}`}
        >
          <Send className="w-4 h-4" />
        </button>
      )}
    </div>
  )
}
