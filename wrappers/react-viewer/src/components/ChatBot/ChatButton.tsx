import { MessageSquare, X } from 'lucide-react'
import { useAppStore } from '../../store'
import { useEffect } from 'react'

/**
 * Floating chat button that opens the AI assistant panel
 */
export function ChatButton() {
  const { isChatOpen, isChatAvailable, toggleChat, checkChatAvailability, chatMessages } = useAppStore()

  // Check availability on mount
  useEffect(() => {
    checkChatAvailability()
  }, [checkChatAvailability])

  const unreadCount = chatMessages.filter(m => m.role === 'assistant').length

  return (
    <button
      onClick={toggleChat}
      disabled={!isChatAvailable && !isChatOpen}
      className={`
        fixed bottom-6 right-6 z-50
        w-14 h-14 rounded-full
        flex items-center justify-center
        shadow-lg transition-all duration-200
        ${isChatAvailable 
          ? 'bg-rs-blue hover:bg-blue-600 text-white' 
          : 'bg-gray-600 text-gray-400 cursor-not-allowed'}
        ${isChatOpen ? 'scale-90' : 'scale-100 hover:scale-105'}
      `}
      title={isChatAvailable ? 'Open AI Assistant' : 'AI Assistant unavailable (API key not configured)'}
    >
      {isChatOpen ? (
        <X className="w-6 h-6" />
      ) : (
        <>
          <MessageSquare className="w-6 h-6" />
          {unreadCount > 0 && (
            <span className="absolute -top-1 -right-1 w-5 h-5 bg-red-500 text-white text-xs rounded-full flex items-center justify-center">
              {unreadCount > 9 ? '9+' : unreadCount}
            </span>
          )}
        </>
      )}
      
      {/* Offline badge */}
      {!isChatAvailable && !isChatOpen && (
        <span className="absolute -bottom-1 -right-1 px-1.5 py-0.5 bg-gray-800 text-gray-400 text-[10px] rounded-full border border-gray-600">
          offline
        </span>
      )}
    </button>
  )
}
