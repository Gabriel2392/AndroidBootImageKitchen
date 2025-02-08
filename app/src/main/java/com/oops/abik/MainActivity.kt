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

    private lateinit var legacyPermissionLauncher: ActivityResultLauncher<Array<String>>
    private lateinit var manageStorageSettingsLauncher: ActivityResultLauncher<Intent>

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        legacyPermissionLauncher =
            registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { permissions ->
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

        manageStorageSettingsLauncher =
            registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
                if (hasAllFilesAccess()) {
                    startCoreIfAllowed()
                } else {
                    Toast.makeText(
                        this,
                        "All files access permission is required to run the core functionality.",
                        Toast.LENGTH_LONG
                    ).show()
                    finishAffinity()
                }
            }

        requestAppropriateFileAccessPermission()
    }

    override fun onResume() {
        super.onResume()
        if (hasAllFilesAccess()) {
            startCoreIfAllowed()
        }
    }

    private fun hasAllFilesAccess(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Environment.isExternalStorageManager()
        } else {
            val readGranted = ContextCompat.checkSelfPermission(
                this,
                Manifest.permission.READ_EXTERNAL_STORAGE
            ) == PackageManager.PERMISSION_GRANTED
            val writeGranted = ContextCompat.checkSelfPermission(
                this,
                Manifest.permission.WRITE_EXTERNAL_STORAGE
            ) == PackageManager.PERMISSION_GRANTED
            readGranted && writeGranted
        }
    }

    private fun requestAppropriateFileAccessPermission() {
        if (hasAllFilesAccess()) {
            startCoreIfAllowed()
        } else {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                AlertDialog.Builder(this)
                    .setTitle("All Files Access Required")
                    .setMessage(
                        "This app requires permission to manage all files. Please allow this " +
                                "permission in the settings screen that will open next. The app " +
                                "will be highlighted so you can find it quickly."
                    )
                    .setCancelable(false)
                    .setPositiveButton("Open Settings") { _, _ ->
                        val intent = Intent(
                            Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                            Uri.parse("package:$packageName")
                        )
                        manageStorageSettingsLauncher.launch(intent)
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
            } else {
                legacyPermissionLauncher.launch(
                    arrayOf(
                        Manifest.permission.READ_EXTERNAL_STORAGE,
                        Manifest.permission.WRITE_EXTERNAL_STORAGE
                    )
                )
            }
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
