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

const LINK_CLASSES = 'text-rs-blue hover:underline break-all'

function formatInline(text: string, isLight: boolean): (string | ReactNode)[] {
  const parts: (string | ReactNode)[] = []
  let remaining = text
  let key = 0

  while (remaining) {
    const candidates: { match: RegExpMatchArray; type: 'bold' | 'code' | 'link' | 'url' }[] = []
    const boldMatch = remaining.match(/\*\*(.+?)\*\*/)
    if (boldMatch) candidates.push({ match: boldMatch, type: 'bold' })
    const codeMatch = remaining.match(/`([^`]+)`/)
    if (codeMatch) candidates.push({ match: codeMatch, type: 'code' })
    // Markdown links are checked before bare URLs so "[label](url)" wins the tie over the
    // bare-url match that would otherwise fire on the URL portion inside it.
    const linkMatch = remaining.match(/\[([^\]]+)\]\((https?:\/\/[^\s)]+)\)/)
    if (linkMatch) candidates.push({ match: linkMatch, type: 'link' })
    const urlMatch = remaining.match(/https?:\/\/[^\s<>()[\]"']+/)
    if (urlMatch) candidates.push({ match: urlMatch, type: 'url' })

    if (candidates.length === 0) {
      parts.push(remaining)
      break
    }

    const { match, type } = candidates.reduce((earliest, c) => (c.match.index! < earliest.match.index! ? c : earliest))

    if (match.index! > 0) {
      parts.push(remaining.slice(0, match.index))
    }
    if (type === 'bold') {
      parts.push(<strong key={key++} className="font-semibold">{match[1]}</strong>)
    } else if (type === 'code') {
      parts.push(<code key={key++} className={`px-1 py-0.5 rounded text-xs ${isLight ? 'bg-gray-200' : 'bg-gray-800'}`}>{match[1]}</code>)
    } else if (type === 'link') {
      parts.push(
        <a key={key++} href={match[2]} target="_blank" rel="noopener noreferrer" className={LINK_CLASSES}>
          {match[1]}
        </a>
      )
    } else {
      parts.push(
        <a key={key++} href={match[0]} target="_blank" rel="noopener noreferrer" className={LINK_CLASSES}>
          {match[0]}
        </a>
      )
    }
    remaining = remaining.slice(match.index! + match[0].length)
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
