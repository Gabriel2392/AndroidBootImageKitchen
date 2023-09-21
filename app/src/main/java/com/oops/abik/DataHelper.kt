package com.oops.abik

object DataHelper {
    var isABIKRunning = false
    var console: String = ""

    private var consoleUpdateListener: ConsoleUpdateListener? = null

    fun setConsoleUpdateListener(listener: ConsoleUpdateListener?) {
        consoleUpdateListener = listener
    }

    @JvmStatic
    fun updateConsoleText(newText: String) {
        console += newText + "\n"
        if (consoleUpdateListener != null) {
            consoleUpdateListener?.onConsoleTextUpdated(newText)
        }
    }

    interface ConsoleUpdateListener {
        fun onConsoleTextUpdated(newText: String)
    }
}
