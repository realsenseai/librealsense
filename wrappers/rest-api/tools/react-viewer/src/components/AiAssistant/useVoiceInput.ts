// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
//
// Thin wrapper around the (still vendor-prefixed) Web Speech API for voice-to-text input.

import { useEffect, useMemo, useRef, useState } from 'react'

// Minimal shape of the API — not in lib.dom.d.ts.
interface MinimalSpeechRecognition extends EventTarget {
  continuous: boolean
  interimResults: boolean
  lang: string
  onresult: ((event: { results: ArrayLike<ArrayLike<{ transcript: string }>> }) => void) | null
  onerror: (() => void) | null
  onend: (() => void) | null
  start: () => void
  stop: () => void
}

function getSpeechRecognitionCtor(): (new () => MinimalSpeechRecognition) | null {
  const w = window as unknown as {
    SpeechRecognition?: new () => MinimalSpeechRecognition
    webkitSpeechRecognition?: new () => MinimalSpeechRecognition
  }
  return w.SpeechRecognition || w.webkitSpeechRecognition || null
}

/**
 * Voice input via the Web Speech API. `onTranscript` is called with the recognized text once
 * a listening session ends; the caller decides how to merge it (e.g. append to existing input).
 */
export function useVoiceInput(onTranscript: (transcript: string) => void) {
  const [isListening, setIsListening] = useState(false)
  const recognitionRef = useRef<MinimalSpeechRecognition | null>(null)
  const micSupported = useMemo(() => getSpeechRecognitionCtor() !== null, [])

  // Stop any active recognition when the owning component unmounts.
  useEffect(() => {
    return () => recognitionRef.current?.stop()
  }, [])

  const toggleListening = () => {
    if (isListening) {
      recognitionRef.current?.stop()
      return
    }
    const SpeechRecognitionCtor = getSpeechRecognitionCtor()
    if (!SpeechRecognitionCtor) return

    const recognition = new SpeechRecognitionCtor()
    recognition.continuous = false
    recognition.interimResults = false
    recognition.lang = 'en-US'
    recognition.onresult = (event) => {
      const transcript = Array.from(event.results).map((r) => r[0]?.transcript || '').join(' ')
      onTranscript(transcript)
    }
    recognition.onerror = () => setIsListening(false)
    recognition.onend = () => setIsListening(false)

    recognitionRef.current = recognition
    setIsListening(true)
    recognition.start()
  }

  return { isListening, micSupported, toggleListening }
}
