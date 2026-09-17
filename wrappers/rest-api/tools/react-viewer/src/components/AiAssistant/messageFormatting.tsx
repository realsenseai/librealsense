// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
//
// Pure formatting helpers for rendering an assistant message's streamed text: strips the
// assistant's raw citation placeholders and renders the remaining markdown (GFM tables,
// headers, lists, links, images, code) via react-markdown. No component state — just
// string/content in, JSX out.

import type { ReactNode } from 'react'
import ReactMarkdown, { type Components } from 'react-markdown'
import remarkGfm from 'remark-gfm'
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

const components: Components = {
  a: ({ href, children }) => (
    <a href={href} target="_blank" rel="noopener noreferrer" className={LINK_CLASSES}>
      {children}
    </a>
  ),
  img: ({ src, alt }) => (
    <a href={typeof src === 'string' ? src : undefined} target="_blank" rel="noopener noreferrer" className="block mt-2">
      <img
        src={typeof src === 'string' ? src : undefined}
        alt={alt}
        className="max-w-full rounded-lg"
        // An unreachable image (404, CORS block) would otherwise show a broken-image icon
        // with no explanation — hide the whole link (not just the <img>) instead.
        onError={(e) => {
          const link = e.currentTarget.parentElement
          if (link) link.style.display = 'none'
        }}
      />
    </a>
  ),
  // react-markdown no longer tells us "inline" directly — a fenced code block always has
  // a newline (even without a language tag) or a `language-*` className; anything else is
  // inline code. Block code's outer <pre> gets the visible background/padding below.
  code: ({ className, children }) => {
    const text = String(children).replace(/\n$/, '')
    const isBlock = className?.startsWith('language-') || text.includes('\n')
    if (isBlock) return <code className={className}>{children}</code>
    return <code className="px-1 py-0.5 rounded text-xs bg-gray-800">{children}</code>
  },
  pre: ({ children }) => (
    <pre className="mt-2 p-2 rounded text-xs whitespace-pre-wrap break-words bg-gray-900 overflow-x-auto">{children}</pre>
  ),
  p: ({ children }) => <p className="mb-2 last:mb-0">{children}</p>,
  ul: ({ children }) => <ul className="list-disc pl-4 mb-2 space-y-0.5">{children}</ul>,
  ol: ({ children }) => <ol className="list-decimal pl-4 mb-2 space-y-0.5">{children}</ol>,
  h1: ({ children }) => <h3 className="text-sm font-bold mt-2 mb-1">{children}</h3>,
  h2: ({ children }) => <h3 className="text-sm font-bold mt-2 mb-1">{children}</h3>,
  h3: ({ children }) => <h4 className="text-sm font-semibold mt-2 mb-1">{children}</h4>,
  table: ({ children }) => (
    <div className="overflow-x-auto my-2">
      <table className="text-xs border-collapse w-full">{children}</table>
    </div>
  ),
  th: ({ children }) => <th className="border border-gray-700 px-2 py-1 bg-gray-800 text-left">{children}</th>,
  td: ({ children }) => <td className="border border-gray-700 px-2 py-1">{children}</td>,
}

/** Renders an assistant message's markdown content (GFM tables, headers, lists, links, images, code). */
export function renderMessageContent(content: string): ReactNode {
  return (
    <ReactMarkdown remarkPlugins={[remarkGfm]} components={components}>
      {content}
    </ReactMarkdown>
  )
}
