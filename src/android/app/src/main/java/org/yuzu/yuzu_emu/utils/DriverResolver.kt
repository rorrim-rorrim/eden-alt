// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import androidx.core.net.toUri
import androidx.fragment.app.FragmentActivity
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.yuzu.yuzu_emu.fragments.DriverFetcherFragment
import org.yuzu.yuzu_emu.fragments.DriverFetcherFragment.SortMode
import org.yuzu.yuzu_emu.model.DriverViewModel
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine

object DriverResolver {
    private val client = OkHttpClient()

    // Mirror of the repositories from DriverFetcherFragment
    private val repoList = listOf(
        DriverRepo("Mr. Purple Turnip", "MrPurple666/purple-turnip", 0),
        DriverRepo("GameHub Adreno 8xx", "crueter/GameHub-8Elite-Drivers", 1),
        DriverRepo("KIMCHI Turnip", "K11MCH1/AdrenoToolsDrivers", 2, true),
        DriverRepo("Weab-Chan Freedreno", "Weab-chan/freedreno_turnip-CI", 3),
    )

    private data class DriverRepo(
        val name: String,
        val path: String,
        val sort: Int,
        val useTagName: Boolean = false
    )

    /**
     * Extract driver path from custom settings INI content
     */
    fun extractDriverPath(customSettings: String): String? {
        val lines = customSettings.lines()
        var inGpuDriverSection = false

        for (line in lines) {
            val trimmed = line.trim()
            if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
                inGpuDriverSection = trimmed == "[GpuDriver]"
                continue
            }

            if (inGpuDriverSection && trimmed.startsWith("driver_path=")) {
                return trimmed.substringAfter("driver_path=")
            }
        }

        return null
    }

    /**
     * Check if a driver exists and handle missing drivers
     */
    suspend fun ensureDriverExists(
        driverPath: String,
        activity: FragmentActivity,
        driverViewModel: DriverViewModel
    ): Boolean {
        Log.info("[DriverResolver] Checking driver path: $driverPath")

        val driverFile = File(driverPath)
        if (driverFile.exists()) {
            Log.info("[DriverResolver] Driver exists at: $driverPath")
            return true
        }

        Log.warning("[DriverResolver] Driver not found: $driverPath")

        // Extract driver name from path
        val driverName = extractDriverNameFromPath(driverPath)
        if (driverName == null) {
            Log.error("[DriverResolver] Could not extract driver name from path")
            return false
        }

        Log.info("[DriverResolver] Searching for downloadable driver: $driverName")

        // Check if driver exists locally with different path
        val localDriver = findLocalDriver(driverName)
        if (localDriver != null) {
            Log.info("[DriverResolver] Found local driver: ${localDriver.first}")
            // The game can use this local driver, no need to download
            return true
        }

        // Search for downloadable driver
        val downloadableDriver = findDownloadableDriver(driverName)
        if (downloadableDriver != null) {
            Log.info("[DriverResolver] Found downloadable driver: ${downloadableDriver.name}")

            val shouldInstall = askUserToInstallDriver(activity, downloadableDriver.name)
            if (shouldInstall) {
                return downloadAndInstallDriver(activity, downloadableDriver, driverViewModel)
            }
        } else {
            Log.warning("[DriverResolver] No downloadable driver found for: $driverName")
            showDriverNotFoundDialog(activity, driverName)
        }

        return false
    }

    /**
     * Extract driver name from full path
     */
    private fun extractDriverNameFromPath(driverPath: String): String? {
        val file = File(driverPath)
        val fileName = file.name

        // Remove .zip extension and extract meaningful name
        if (fileName.endsWith(".zip")) {
            return fileName.substring(0, fileName.length - 4)
        }

        return fileName
    }

    /**
     * Find driver in local storage by name matching
     */
    private fun findLocalDriver(driverName: String): Pair<String, GpuDriverMetadata>? {
        val availableDrivers = GpuDriverHelper.getDrivers()

        // Try exact match first
        availableDrivers.find { (_, metadata) ->
            metadata.name?.contains(driverName, ignoreCase = true) == true
        }?.let { return it }

        // Try partial match
        availableDrivers.find { (path, metadata) ->
            path.contains(driverName, ignoreCase = true) ||
                    metadata.name?.contains(
                        extractKeywords(driverName).first(),
                        ignoreCase = true
                    ) == true
        }?.let { return it }

        return null
    }

    /**
     * Extract keywords from driver name for matching
     */
    private fun extractKeywords(driverName: String): List<String> {
        val keywords = mutableListOf<String>()

        // Common driver patterns
        when {
            driverName.contains("turnip", ignoreCase = true) -> keywords.add("turnip")
            driverName.contains("purple", ignoreCase = true) -> keywords.add("purple")
            driverName.contains("kimchi", ignoreCase = true) -> keywords.add("kimchi")
            driverName.contains("freedreno", ignoreCase = true) -> keywords.add("freedreno")
            driverName.contains("gamehub", ignoreCase = true) -> keywords.add("gamehub")
        }

        // Version patterns
        Regex("v?\\d+\\.\\d+\\.\\d+").find(driverName)?.value?.let { keywords.add(it) }

        if (keywords.isEmpty()) {
            keywords.add(driverName)
        }

        return keywords
    }

    /**
     * Find downloadable driver that matches the required driver
     */
    private suspend fun findDownloadableDriver(driverName: String): DriverFetcherFragment.Artifact? {
        val keywords = extractKeywords(driverName)

        for (repo in repoList) {
            // Check if this repo is relevant based on driver name
            val isRelevant = keywords.any { keyword ->
                repo.name.contains(keyword, ignoreCase = true) ||
                        keyword.contains(repo.name.split(" ").first(), ignoreCase = true)
            }

            if (!isRelevant) continue

            try {
                val releases = fetchReleases(repo)
                val latestRelease = releases.firstOrNull { !it.prerelease }

                latestRelease?.artifacts?.forEach { artifact ->
                    if (matchesDriverName(artifact.name, driverName, keywords)) {
                        return artifact
                    }
                }
            } catch (e: Exception) {
                Log.error("[DriverResolver] Failed to fetch releases for ${repo.name}: ${e.message}")
            }
        }

        return null
    }

    /**
     * Check if artifact name matches the required driver
     */
    private fun matchesDriverName(
        artifactName: String,
        requiredName: String,
        keywords: List<String>
    ): Boolean {
        // Exact match
        if (artifactName.equals(requiredName, ignoreCase = true)) return true

        // Keyword matching
        return keywords.any { keyword ->
            artifactName.contains(keyword, ignoreCase = true)
        }
    }

    /**
     * Fetch releases from GitHub repo
     */
    private suspend fun fetchReleases(repo: DriverRepo): List<DriverFetcherFragment.Release> {
        return withContext(Dispatchers.IO) {
            val request = Request.Builder()
                .url("https://api.github.com/repos/${repo.path}/releases")
                .build()

            client.newCall(request).execute().use { response ->
                if (!response.isSuccessful) {
                    throw IOException("Failed to fetch releases: ${response.code}")
                }

                val body = response.body?.string() ?: throw IOException("Empty response")
                DriverFetcherFragment.Release.fromJsonArray(body, repo.useTagName, SortMode.Default)
            }
        }
    }

    /**
     * Ask user if they want to install the missing driver
     */
    private suspend fun askUserToInstallDriver(
        activity: FragmentActivity,
        driverName: String
    ): Boolean {
        return suspendCoroutine { continuation ->
            activity.runOnUiThread {
                MaterialAlertDialogBuilder(activity)
                    .setTitle("Missing GPU Driver")
                    .setMessage(
                        "The custom settings require the GPU driver '$driverName' which is not installed.\n\n" +
                                "Would you like to download and install it automatically?"
                    )
                    .setPositiveButton("Install") { _, _ ->
                        continuation.resume(true)
                    }
                    .setNegativeButton("Cancel") { _, _ ->
                        continuation.resume(false)
                    }
                    .setCancelable(false)
                    .show()
            }
        }
    }

    /**
     * Download and install driver automatically
     */
    private suspend fun downloadAndInstallDriver(
        activity: FragmentActivity,
        artifact: DriverFetcherFragment.Artifact,
        driverViewModel: DriverViewModel
    ): Boolean {
        return try {
            Log.info("[DriverResolver] Downloading driver: ${artifact.name}")

            val cacheDir =
                activity.externalCacheDir ?: throw IOException("Cache directory not available")
            cacheDir.mkdirs()

            val file = File(cacheDir, artifact.name)

            // Download the driver
            withContext(Dispatchers.IO) {
                val request = Request.Builder()
                    .url(artifact.url)
                    .header("Accept", "application/octet-stream")
                    .build()

                client.newBuilder()
                    .followRedirects(true)
                    .followSslRedirects(true)
                    .build()
                    .newCall(request).execute().use { response ->
                        if (!response.isSuccessful) {
                            throw IOException("Download failed: ${response.code}")
                        }

                        response.body?.byteStream()?.use { input ->
                            FileOutputStream(file).use { output ->
                                input.copyTo(output)
                            }
                        } ?: throw IOException("Empty response body")
                    }
            }

            if (file.length() == 0L) {
                throw IOException("Downloaded file is empty")
            }

            // Install the driver on main thread
            withContext(Dispatchers.Main) {
                val driverData = GpuDriverHelper.getMetadataFromZip(file)
                val driverPath = "${GpuDriverHelper.driverStoragePath}${file.name}"

                if (GpuDriverHelper.copyDriverToInternalStorage(file.toUri())) {
                    driverViewModel.onDriverAdded(Pair(driverPath, driverData))
                    Log.info("[DriverResolver] Successfully installed driver: ${driverData.name}")
                    true
                } else {
                    throw IOException("Failed to install driver")
                }
            }
        } catch (e: Exception) {
            Log.error("[DriverResolver] Failed to download/install driver: ${e.message}")
            withContext(Dispatchers.Main) {
                MaterialAlertDialogBuilder(activity)
                    .setTitle("Installation Failed")
                    .setMessage("Failed to download and install the driver: ${e.message}")
                    .setPositiveButton("OK") { dialog, _ -> dialog.dismiss() }
                    .show()
            }
            false
        }
    }

    /**
     * Show dialog when driver cannot be found
     */
    private fun showDriverNotFoundDialog(activity: FragmentActivity, driverName: String) {
        activity.runOnUiThread {
            MaterialAlertDialogBuilder(activity)
                .setTitle("Driver Not Available")
                .setMessage(
                    "The required GPU driver '$driverName' is not available for automatic download.\n\n" +
                            "Please manually install the driver or launch the game with default settings."
                )
                .setPositiveButton("OK") { dialog, _ -> dialog.dismiss() }
                .show()
        }
    }
}
