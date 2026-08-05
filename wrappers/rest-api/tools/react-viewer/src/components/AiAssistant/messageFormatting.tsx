// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
//
// Pure formatting helpers for rendering an assistant message's streamed text: strips the
// assistant's raw citation placeholders and does light markdown-ish rendering (bold, inline
// code, code blocks). No component state — just string/content in, JSX out.

import type { ReactNode } from 'react'
import type { AssistantCitation } from '../../api/assistantChat'

/**
 * The assistant embeds raw citation placeholders (e.g. "【4:1†source】") in the streamed text,
 * meant to be spliced into a rendered link. We show citations as chips below the bubble instead
 * (see AssistantMessage's citations block), so just strip the placeholders here rather than
 * leaving them visible as noise.
 */
export function stripCitationMarkers(content: string, citations?: AssistantCitation[]): string {
  let stripped = content
  for (const citation of citations || []) {
    if (citation.textToReplace) stripped = stripped.split(citation.textToReplace).join('')
  }
  return stripped.replace(/【\d+(?::\d+)?†[^】]*】/g, '')
}

function formatInline(text: string, isLight: boolean): (string | ReactNode)[] {
  const parts: (string | ReactNode)[] = []
  let remaining = text
  let key = 0

  while (remaining) {
    const boldMatch = remaining.match(/\*\*(.+?)\*\*/)
    const codeMatch = remaining.match(/`([^`]+)`/)

    let earliestMatch: RegExpMatchArray | null = null
    let type: 'bold' | 'code' | null = null

    if (boldMatch && (!codeMatch || boldMatch.index! < codeMatch.index!)) {
      earliestMatch = boldMatch
      type = 'bold'
    } else if (codeMatch) {
      earliestMatch = codeMatch
      type = 'code'
    }

    if (earliestMatch && type) {
      if (earliestMatch.index! > 0) {
        parts.push(remaining.slice(0, earliestMatch.index))
      }
      if (type === 'bold') {
        parts.push(<strong key={key++} className="font-semibold">{earliestMatch[1]}</strong>)
      } else {
        parts.push(<code key={key++} className={`px-1 py-0.5 rounded text-xs ${isLight ? 'bg-gray-200' : 'bg-gray-800'}`}>{earliestMatch[1]}</code>)
      }
      remaining = remaining.slice(earliestMatch.index! + earliestMatch[0].length)
    } else {
      parts.push(remaining)
      break
    }
  }

  return parts
}

/** Renders code blocks (```...```) and inline bold/code formatting for a message's content. */
export function renderMessageContent(content: string, isLight: boolean): ReactNode {
  const parts = content.split(/(```[\s\S]*?```)/g)

  return parts.map((part, i) => {
    if (part.startsWith('```')) {
      const match = part.match(/```(\w+)?\n?([\s\S]*?)```/)
      if (match) {
        const [, lang, code] = match
        return (
          <pre key={i} className={`mt-2 p-2 rounded text-xs whitespace-pre-wrap break-words ${isLight ? 'bg-gray-100' : 'bg-gray-900'}`}>
            <code className={`language-${lang || 'text'}`}>{code.trim()}</code>
          </pre>
        )
      }
    }

    return (
      <span key={i}>
        {part.split('\n').map((line, j) => (
          <span key={j}>
            {j > 0 && <br />}
            {formatInline(line, isLight)}
          </span>
        ))}
      </span>
    )
  })
}
