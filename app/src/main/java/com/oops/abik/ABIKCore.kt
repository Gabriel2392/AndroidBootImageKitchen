package com.oops.abik

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.res.Configuration
import android.content.res.Resources
import android.net.Uri
import android.os.Bundle
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.util.DisplayMetrics
import android.view.WindowManager
import android.widget.ScrollView
import android.widget.TextView
import androidx.activity.OnBackPressedCallback
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.core.widget.NestedScrollView
import androidx.fragment.app.FragmentActivity
import com.google.android.material.button.MaterialButton
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.progressindicator.LinearProgressIndicator
import com.google.android.material.switchmaterial.SwitchMaterial
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File


class ABIKCore : FragmentActivity(), DataHelper.ConsoleUpdateListener {
    private lateinit var abikBridge: ABIKBridge
    private var backPressedCallback: OnBackPressedCallback? = null

    private lateinit var selectedImageCallback: (Uri) -> Unit
    private lateinit var workingDirTextView: TextView
    private lateinit var extractImageBtn: MaterialButton
    private lateinit var buildImageBtn: MaterialButton
    private lateinit var cleanBtn: MaterialButton
    private lateinit var consoleTextView: TextView
    private lateinit var consoleSV: NestedScrollView
    private lateinit var decompressRamdiskSwitch: SwitchMaterial

    override fun onResume() {
        super.onResume()
        onConsoleTextUpdated(DataHelper.console)
    }

    @SuppressLint("SourceLockedOrientationActivity")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.abik_main)

        consoleTextView = findViewById(R.id.console)
        consoleTextView.textSize = 10.0F
        consoleSV = findViewById(R.id.consoleSV)

        DataHelper.setConsoleUpdateListener(this)

        decompressRamdiskSwitch = findViewById(R.id.decompressRamdiskSwitch)

        backPressedCallback = object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                if (DataHelper.isABIKRunning) {
                    abikBridge.showToast(getString(R.string.app_running))
                } else {
                    moveTaskToBack(true)
                }
            }
        }
        onBackPressedDispatcher.addCallback(this, backPressedCallback as OnBackPressedCallback)

        abikBridge = ABIKBridge(application)

        val workingDir = "${Environment.getExternalStorageDirectory().absolutePath}/ABIK"

        workingDirTextView = findViewById(R.id.workingDirText)
        workingDirTextView.text =
            getString(R.string.working_dir, workingDir)

        extractImageBtn = findViewById(R.id.extractImageBtn)
        setupFilePickerButton(extractImageBtn) { uri ->
            val input_name =
                Uri.decode(uri.toString()).substringAfterLast("/").substringAfterLast(":")
                    .substringBeforeLast(".")
            val input_fd = getFd(applicationContext, uri)
            if (input_fd == null) {
                abikBridge.showToast(getString(R.string.input_file_error))
                return@setupFilePickerButton
            }

            abikBridge.extract(input_fd, input_name, workingDir, decompressRamdiskSwitch.isChecked)
        }

        buildImageBtn = findViewById(R.id.buildImageBtn)
        buildImageBtn.setOnClickListener {
            synchronized(this) {
                if (DataHelper.isABIKRunning) {
                    abikBridge.showToast(getString(R.string.app_running))
                    return@setOnClickListener
                }
                usingSelectedProject(workingDir) { selectedFolder ->
                    abikBridge.build(selectedFolder.absolutePath)
                }.`else` {
                    abikBridge.showToast(getString(R.string.no_projects))
                }
            }
        }

        cleanBtn = findViewById(R.id.clearWorkDirBtn)
        cleanBtn.setOnClickListener {
            synchronized(this) {
                if (DataHelper.isABIKRunning) {
                    abikBridge.showToast(getString(R.string.app_running))
                    return@setOnClickListener
                }
                withABIKRunning {
                    if (!showDeletionSelectionDialog(workingDir)) {
                        abikBridge.showToast(getString(R.string.nothing_to_remove))
                    }
                }
            }
        }

        this.window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }

    private fun showDeletionSelectionDialog(dir: String): Boolean {
        val fileList = File(dir).listFiles()?.toList() ?: emptyList()
        if (fileList.isEmpty()) {
            return false
        }

        val parentDirName = File(dir).name
        val items = fileList.map { it.name }.toTypedArray()
        val checkedItems = BooleanArray(items.size)

        val alertDialog = AlertDialog.Builder(this)
            .setTitle(getString(R.string.select_path_to_remove))
            .setMultiChoiceItems(items, checkedItems) { _, which, isChecked ->
                checkedItems[which] = isChecked
            }
            .setNeutralButton(getString(android.R.string.selectAll), null)
            .setPositiveButton(getString(android.R.string.ok)) { _, _ ->
                val progressDialog = MaterialAlertDialogBuilder(this).apply {
                    setTitle(getString(R.string.cleaning_title, parentDirName))
                    setMessage(getString(R.string.deleting_items))
                    setView(LinearProgressIndicator(this.context).apply {
                        isIndeterminate = true
                        setPadding(32.dp, 0, 32.dp, 0)
                    })
                    setCancelable(false)
                }.create()

                progressDialog.show()

                CoroutineScope(Dispatchers.IO).launch {
                    var deletedCount = 0

                    fileList.forEachIndexed { index, file ->
                        if (checkedItems[index]) {
                            withContext(Dispatchers.Main) {
                                progressDialog.setMessage(getString(R.string.deleting_title, file.name))
                            }
                            file.deleteRecursively()
                            deletedCount++
                        }
                    }

                    withContext(Dispatchers.Main) {
                        progressDialog.dismiss()
                    }
                }
            }
            .setNegativeButton(getString(android.R.string.cancel), null)
            .create()

        alertDialog.setOnShowListener {
            val neutralButton = alertDialog.getButton(AlertDialog.BUTTON_NEUTRAL)
            neutralButton.setOnClickListener {
                val allSelected = checkedItems.all { it }
                val newState = !allSelected
                checkedItems.fill(newState)

                val listView = alertDialog.listView
                for (i in checkedItems.indices) {
                    listView.setItemChecked(i, newState)
                }
            }
        }

        alertDialog.show()

        return true
    }

    private val Int.dp: Int
        get() = (this * Resources.getSystem().displayMetrics.density).toInt()

    private fun setupFilePickerButton(
        button: MaterialButton,
        updateFile: (Uri) -> Unit
    ) {
        button.setOnClickListener {
            synchronized(this) {
                startFilePicker { uri ->
                    updateFile(uri)
                }
            }
        }
    }

    private fun startFilePicker(callback: (Uri) -> Unit) {
        if (DataHelper.isABIKRunning) {
            abikBridge.showToast(getString(R.string.app_running))
            return
        }
        selectedImageCallback = callback
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
        }
        activityResultLauncher.launch(intent)
    }

    private fun showActivitiesProblem() {
        val builder = AlertDialog.Builder(this)
        builder.setMessage(getString(R.string.disable_dont_keep_activities))
            .setPositiveButton(android.R.string.ok) { dialog, _ ->
                try {
                    val intent = Intent(Settings.ACTION_APPLICATION_DEVELOPMENT_SETTINGS)
                    startActivity(intent)
                } catch (e: Exception) {
                    abikBridge.showToast(getString(R.string.disable_dont_keep_activities))
                }
                dialog.dismiss()
                android.os.Process.killProcess(android.os.Process.myPid())
            }
            .setCancelable(false)

        val dialog = builder.create()

        dialog.setOnShowListener {
            dialog.window?.setDimAmount(0.5f)
        }

        dialog.show()
    }

    private val activityResultLauncher = registerForActivityResult(ActivityResultContracts.StartActivityForResult()) { result ->
        if (result.resultCode == Activity.RESULT_OK) {
            result.data?.data?.let { uri ->
                try {
                    selectedImageCallback.invoke(uri)
                } catch (e: Exception) {
                    e.printStackTrace()
                    showActivitiesProblem()
                }
            }
        }
    }

    private fun getFd(context: Context, uri: Uri): Int? {
        val pfd = context.contentResolver.openFileDescriptor(uri, "rw") ?: return null
        return pfd.detachFd()
    }

    private fun scrollToBottom() {
        consoleSV.post {
            consoleSV.fullScroll(ScrollView.FOCUS_DOWN)
        }
    }

    override fun onConsoleTextUpdated(newText: String) {
        try {
            consoleTextView.post {
                consoleTextView.text = DataHelper.console
            }
            scrollToBottom()
        } catch (t: Throwable) {
            // Romania kangers
        }
    }

    class SelectedProjectSelector(
        private val context: Context,
        inputDir: String,
        private val onSelected: (File) -> Unit
    ) {
        private val folderList: List<File> =
            File(inputDir).listFiles { file -> file.isDirectory }?.toList() ?: emptyList()

        private var elseAction: (() -> Unit)? = null

        init {
            Handler(Looper.getMainLooper()).post {
                if (folderList.isNotEmpty()) {
                    showSelectionDialog()
                } else {
                    elseAction?.invoke()
                }
            }
        }

        fun `else`(action: () -> Unit): SelectedProjectSelector {
            elseAction = action
            if (folderList.isEmpty()) {
                Handler(Looper.getMainLooper()).post {
                    elseAction?.invoke()
                }
            }
            return this
        }

        private fun showSelectionDialog() {
            var selectedIndex = -1
            val folderNames = folderList.map { it.name }.toTypedArray()
            if (folderNames.size == 1) {
                onSelected(folderList[0])
                return
            }

            AlertDialog.Builder(context)
                .setTitle("Select a project")
                .setSingleChoiceItems(folderNames, -1) { _, which ->
                    selectedIndex = which
                }
                .setPositiveButton(android.R.string.ok) { _, _ ->
                    if (selectedIndex != -1) {
                        onSelected(folderList[selectedIndex])
                    }
                }
                .setNegativeButton(android.R.string.cancel, null)
                .show()
        }
    }

    private fun Context.usingSelectedProject(inputDir: String, onSelected: (File) -> Unit): SelectedProjectSelector {
        return SelectedProjectSelector(this, inputDir, onSelected)
    }

    private fun <T> withABIKRunning(block: () -> T): T {
        DataHelper.isABIKRunning = true
        return try {
            block()
        } finally {
            DataHelper.isABIKRunning = false
        }
    }
}
