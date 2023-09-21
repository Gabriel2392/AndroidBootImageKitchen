@file:OptIn(DelicateCoroutinesApi::class)

package com.oops.abik

import android.app.Application
import android.widget.Toast
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class ABIKBridge(private val application: Application) {
    private var currentToast: Toast? = null
    private external fun jniExtract(input_fd: Int, input_name: String, dir: String, extract_ramdisk: Boolean): Boolean
    private external fun jniBuild(input_dir: String): Boolean

    fun showToast(str: String) {
        currentToast?.cancel()
        currentToast = Toast.makeText(application, str, Toast.LENGTH_SHORT)
        currentToast?.show()
    }

    fun extract(input_fd: Int, input_name: String, dir: String, extract_ramdisk: Boolean) {
        DataHelper.isABIKRunning = true
        GlobalScope.launch(Dispatchers.IO) {
           jniExtract(input_fd, input_name, dir, extract_ramdisk)
           withContext(Dispatchers.Main) {
               DataHelper.isABIKRunning = false
           }
        }
    }

    fun build(input_dir: String) {
        DataHelper.isABIKRunning = true
        GlobalScope.launch(Dispatchers.IO) {
            jniBuild(input_dir)
            withContext(Dispatchers.Main) {
                DataHelper.isABIKRunning = false
            }
        }
    }

   init {
       System.loadLibrary("abik")
   }
}