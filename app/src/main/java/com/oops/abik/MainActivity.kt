package com.oops.abik

import android.Manifest
import android.app.AlertDialog
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
import android.widget.Toast
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.oops.abik.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {
    private lateinit var binding: ActivityMainBinding

    // Some Android 11 TVs behave like Android 10, do not remove legacy launcher yet
    private lateinit var legacyPermissionLauncher: ActivityResultLauncher<Array<String>>
    private lateinit var manageStorageSettingsLauncher: ActivityResultLauncher<Intent>

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        legacyPermissionLauncher =
            registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { permissions ->
                handleLegacyPermissionResult(permissions)
            }

        manageStorageSettingsLauncher =
            registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
                checkPermissionsAndProceed()
            }

        requestAppropriateFileAccessPermission()
    }

    override fun onResume() {
        super.onResume()
        checkPermissionsAndProceed()
    }

    private fun checkPermissionsAndProceed() {
        if (hasAllFilesAccess()) {
            startCoreIfAllowed()
        }
    }

    private fun hasAllFilesAccess(): Boolean {
        return Environment.isExternalStorageManager() || hasLegacyPermissions()
    }

    // Some Android 11 TVs behave like Android 10, do not remove yet
    private fun hasLegacyPermissions(): Boolean {
        val readGranted = ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.READ_EXTERNAL_STORAGE
        ) == PackageManager.PERMISSION_GRANTED

        val writeGranted = ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.WRITE_EXTERNAL_STORAGE
        ) == PackageManager.PERMISSION_GRANTED

        return readGranted && writeGranted
    }

    private fun requestAppropriateFileAccessPermission() {
        if (hasAllFilesAccess()) {
            startCoreIfAllowed()
            return
        }

        val newPermissionIntent = Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION).apply {
            data = Uri.parse("package:$packageName")
        }

        if (newPermissionIntent.resolveActivity(packageManager) != null) {
            showNewPermissionDialog(newPermissionIntent)
        } else {
            requestLegacyPermissions()
        }
    }

    private fun showNewPermissionDialog(intent: Intent) {
        AlertDialog.Builder(this)
            .setTitle("All Files Access Required")
            .setMessage(
                "This app requires permission to manage all files. Please allow this " +
                        "permission in the settings screen that will open next. The app " +
                        "will be highlighted so you can find it quickly."
            )
            .setCancelable(false)
            .setPositiveButton("Open Settings") { _, _ ->
                try {
                    manageStorageSettingsLauncher.launch(intent)
                } catch (e: Exception) {
                    requestLegacyPermissions()
                }
            }
            .setNegativeButton("Cancel") { _, _ ->
                Toast.makeText(
                    this,
                    "All files access permission is required.",
                    Toast.LENGTH_LONG
                ).show()
                finishAffinity()
            }
            .show()
    }

    private fun requestLegacyPermissions() {
        legacyPermissionLauncher.launch(
            arrayOf(
                Manifest.permission.READ_EXTERNAL_STORAGE,
                Manifest.permission.WRITE_EXTERNAL_STORAGE
            )
        )
    }

    private fun handleLegacyPermissionResult(permissions: Map<String, Boolean>) {
        val readGranted = permissions[Manifest.permission.READ_EXTERNAL_STORAGE] ?: false
        val writeGranted = permissions[Manifest.permission.WRITE_EXTERNAL_STORAGE] ?: false

        if (readGranted && writeGranted) {
            startCoreIfAllowed()
        } else {
            Toast.makeText(
                this,
                "Storage permission is required to run the core functionality.",
                Toast.LENGTH_LONG
            ).show()
            finishAffinity()
        }
    }

    private fun startCoreIfAllowed() {
        if (hasAllFilesAccess()) {
            val intent = Intent(this, ABIKCore::class.java)
            startActivity(intent)
            finish()
        }
    }
}