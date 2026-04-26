interface LoadingSplashProps {
  message?: string
}

export function LoadingSplash({ message = 'Loading...' }: LoadingSplashProps) {
  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm">
      <div className="bg-rs-dark border border-gray-600 rounded-xl shadow-2xl p-8 max-w-sm">
        <div className="text-center">
          {/* RealSense Logo */}
          <div className="mb-6">
            <img 
              src="/realsense-logo.png" 
              alt="RealSense" 
              className="h-10 w-auto mx-auto"
            />
          </div>
          
          {/* Loading Animation */}
          <div className="flex justify-center mb-4">
            <div className="flex space-x-2">
              <div className="w-2 h-2 bg-rs-blue rounded-full animate-bounce" style={{ animationDelay: '0ms' }}></div>
              <div className="w-2 h-2 bg-rs-blue rounded-full animate-bounce" style={{ animationDelay: '150ms' }}></div>
              <div className="w-2 h-2 bg-rs-blue rounded-full animate-bounce" style={{ animationDelay: '300ms' }}></div>
            </div>
          </div>
          
          {/* Loading Message */}
          <p className="text-sm text-gray-300">{message}</p>
          
          {/* Progress Bar */}
          <div className="mt-4 w-48 mx-auto">
            <div className="h-1 bg-gray-700 rounded-full overflow-hidden">
              <div className="h-full bg-gradient-to-r from-rs-blue to-blue-400 rounded-full animate-loading-bar"></div>
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}
