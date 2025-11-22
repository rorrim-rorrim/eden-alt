// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.util.Log
import androidx.documentfile.provider.DocumentFile

class SAFWriter(private val context: Context) {
    fun refreshGamesFolder(
        folders: List<Any>,
        requestPermission: (Uri) -> Unit,
        reloadGames: () -> Unit
    ) {
        if (folders.isNotEmpty()) {
            val gamesDirUriString = try {
                val uriField = folders[0]::class.java.getDeclaredField("uriString")
                uriField.isAccessible = true
                uriField.get(folders[0]) as? String
            } catch (e: Exception) {
                Log.e("YuzuDebug", "[SAFWriter] [SAF] Error accessing uriString: $e")
                null
            }
            if (gamesDirUriString != null) {
                val gamesRootDocFile = DocumentFile.fromTreeUri(context, Uri.parse(gamesDirUriString))
                Log.i("YuzuDebug", "[SAFWriter] [SAF] Refresh triggered. gamesDirUri: $gamesDirUriString, gamesRootDocFile: $gamesRootDocFile")
                if (gamesRootDocFile != null && gamesRootDocFile.isDirectory) {
                    val dirtreeDocFile = gamesRootDocFile.findFile("dirtree")
                    if (dirtreeDocFile != null && dirtreeDocFile.isDirectory) {
                        createRecursiveTimestampFilesInSAF(dirtreeDocFile, requestPermission)
                    } else {
                        Log.e("YuzuDebug", "[SAFWriter] [SAF] 'dirtree' subfolder not found or not a directory.")
                    }
                } else {
                    Log.e("YuzuDebug", "[SAFWriter] [SAF] Invalid games folder DocumentFile.")
                }
            } else {
                Log.e("YuzuDebug", "[SAFWriter] [SAF] Could not get gamesDirUriString from folders.")
            }
        } else {
            Log.e("YuzuDebug", "[SAFWriter] [SAF] No games folder found in gamesViewModel.folders.")
        }
        reloadGames()
    }
    private var pendingSAFAction: (() -> Unit)? = null
    private val REQUEST_CODE_OPEN_DOCUMENT_TREE = 1001

    // Recursively create a timestamp file in each subfolder using SAF
    fun createRecursiveTimestampFilesInSAF(root: DocumentFile, requestPermission: (Uri) -> Unit, onPermissionGranted: (() -> Unit)? = null) {
        Log.i("YuzuDebug", "[SAFWriter] [SAF] Processing folder: ${root.uri}, name: ${root.name}, canWrite: ${root.canWrite()}, isDirectory: ${root.isDirectory}, mimeType: ${root.type}")
        if (!root.isDirectory) return
        if (!root.canWrite()) {
            Log.w("YuzuDebug", "[SAFWriter] [SAF] Cannot write to folder: ${root.uri}, requesting permission...")
            // Save the action to retry after permission
            pendingSAFAction = { createRecursiveTimestampFilesInSAF(root, requestPermission, onPermissionGranted) }
            requestPermission(root.uri)
            return
        }
        val timestamp = java.text.SimpleDateFormat("yyyyMMddHHmmss", java.util.Locale.US).format(java.util.Date())
        try {
            val exists = root.findFile(timestamp)
            if (exists == null) {
                val file = root.createFile("text/plain", timestamp)
                Log.i("YuzuDebug", "[SAFWriter] [SAF] Attempted to create file: $timestamp, result: ${file?.uri}")
            } else {
                Log.i("YuzuDebug", "[SAFWriter] [SAF] File already exists: ${exists.uri}")
            }
        } catch (e: Exception) {
            Log.e("YuzuDebug", "[SAFWriter] [SAF] Error creating file in ${root.uri}: $e")
        }
        root.listFiles()?.filter { it.isDirectory }?.forEach { createRecursiveTimestampFilesInSAF(it, requestPermission, onPermissionGranted) }
    }

    fun handlePermissionResult(uri: Uri) {
        context.contentResolver.takePersistableUriPermission(
            uri,
            Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION
        )
        Log.i("YuzuDebug", "[SAFWriter] [SAF] Permission granted for uri: $uri")
        // Retry the pending action if any
        pendingSAFAction?.invoke()
        pendingSAFAction = null
    }
}
