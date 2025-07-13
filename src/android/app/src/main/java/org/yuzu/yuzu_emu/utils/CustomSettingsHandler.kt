// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.Context
import android.widget.Toast
import androidx.fragment.app.FragmentActivity
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.model.DriverViewModel
import org.yuzu.yuzu_emu.model.Game
import java.io.File
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine

object CustomSettingsHandler {
    const val CUSTOM_CONFIG_ACTION = "dev.eden.eden_emulator.LAUNCH_WITH_CUSTOM_CONFIG"
    const val EXTRA_TITLE_ID = "title_id"
    const val EXTRA_CUSTOM_SETTINGS = "custom_settings"

    /**
     * Apply custom settings from a string instead of loading from file
     * @param titleId The game title ID (16-digit hex string)
     * @param customSettings The complete INI file content as string
     * @param context Application context
     * @return Game object created from title ID, or null if not found
     */
    fun applyCustomSettings(titleId: String, customSettings: String, context: Context): Game? {
        // For synchronous calls without driver checking
        Log.info("[CustomSettingsHandler] Applying custom settings for title ID: $titleId")
        // Find the game by title ID
        val game = findGameByTitleId(titleId, context)
        if (game == null) {
            Log.error("[CustomSettingsHandler] Game not found for title ID: $titleId")
            return null
        }

        // Check if config already exists - this should be handled by the caller
        val configFile = getConfigFile(titleId)
        if (configFile.exists()) {
            Log.warning("[CustomSettingsHandler] Config file already exists for title ID: $titleId")
            // The caller should have already asked the user about overwriting
        }

        // Write the config file
        if (!writeConfigFile(titleId, customSettings)) {
            Log.error("[CustomSettingsHandler] Failed to write config file")
            return null
        }

        // Initialize per-game config
        try {
            NativeConfig.initializePerGameConfig(game.programId, configFile.nameWithoutExtension)
            Log.info("[CustomSettingsHandler] Successfully applied custom settings")
            return game
        } catch (e: Exception) {
            Log.error("[CustomSettingsHandler] Failed to apply custom settings: ${e.message}")
            return null
        }
    }

    /**
     * Apply custom settings with automatic driver checking and installation
     * @param titleId The game title ID (16-digit hex string)
     * @param customSettings The complete INI file content as string
     * @param context Application context
     * @param activity Fragment activity for driver installation dialogs (optional)
     * @param driverViewModel DriverViewModel for driver management (optional)
     * @return Game object created from title ID, or null if not found
     */
    suspend fun applyCustomSettingsWithDriverCheck(
        titleId: String,
        customSettings: String,
        context: Context,
        activity: FragmentActivity?,
        driverViewModel: DriverViewModel?
    ): Game? {
        Log.info("[CustomSettingsHandler] Applying custom settings for title ID: $titleId")
        // Find the game by title ID
        val game = findGameByTitleId(titleId, context)
        if (game == null) {
            Log.error("[CustomSettingsHandler] Game not found for title ID: $titleId")
            // This will be handled by the caller to show appropriate error message
            return null
        }

        // Check if config already exists
        val configFile = getConfigFile(titleId)
        if (configFile.exists() && activity != null) {
            Log.info("[CustomSettingsHandler] Config file already exists, asking user for confirmation")
            val shouldOverwrite = askUserToOverwriteConfig(activity, game.title)
            if (!shouldOverwrite) {
                Log.info("[CustomSettingsHandler] User chose not to overwrite existing config")
                return null
            }
        }

        // Check for driver requirements if activity and driverViewModel are provided
        if (activity != null && driverViewModel != null) {
            val driverPath = DriverResolver.extractDriverPath(customSettings)
            if (driverPath != null) {
                Log.info("[CustomSettingsHandler] Custom settings specify driver: $driverPath")
                val driverExists = DriverResolver.ensureDriverExists(driverPath, activity, driverViewModel)
                if (!driverExists) {
                    Log.error("[CustomSettingsHandler] Required driver not available: $driverPath")
                    // Don't write config if driver installation failed
                    return null
                }
            }
        }

        // Only write the config file after all checks pass
        if (!writeConfigFile(titleId, customSettings)) {
            Log.error("[CustomSettingsHandler] Failed to write config file")
            return null
        }

        // Initialize per-game config
        try {
            NativeConfig.initializePerGameConfig(game.programId, configFile.nameWithoutExtension)
            Log.info("[CustomSettingsHandler] Successfully applied custom settings")
            return game
        } catch (e: Exception) {
            Log.error("[CustomSettingsHandler] Failed to apply custom settings: ${e.message}")
            return null
        }
    }

    /**
     * Find a game by its title ID in the user's game library
     */
    fun findGameByTitleId(titleId: String, context: Context): Game? {
        Log.info("[CustomSettingsHandler] Searching for game with title ID: $titleId")
        // Convert hex title ID to decimal for comparison with programId
        val programIdDecimal = try {
            titleId.toLong(16).toString()
        } catch (e: NumberFormatException) {
            Log.error("[CustomSettingsHandler] Invalid title ID format: $titleId")
            return null
        }

        // Expected hex format with "0" prefix
        val expectedHex = "0${titleId.uppercase()}"
        // First check cached games for fast lookup
        GameHelper.cachedGameList.find { game ->
            game.programId == programIdDecimal ||
                    game.programIdHex.equals(expectedHex, ignoreCase = true)
        }?.let { foundGame ->
            Log.info("[CustomSettingsHandler] Found game in cache: ${foundGame.title}")
            return foundGame
        }
        // If not in cache, perform full game library scan
        Log.info("[CustomSettingsHandler] Game not in cache, scanning full library...")
        val allGames = GameHelper.getGames()
        val foundGame = allGames.find { game ->
            game.programId == programIdDecimal ||
                    game.programIdHex.equals(expectedHex, ignoreCase = true)
        }
        if (foundGame != null) {
            Log.info("[CustomSettingsHandler] Found game: ${foundGame.title} at ${foundGame.path}")
            Toast.makeText(context, "Found game: ${foundGame.title}", Toast.LENGTH_SHORT).show()
        } else {
            Log.warning("[CustomSettingsHandler] No game found for title ID: $titleId")
            Toast.makeText(context, "Game not found for title ID: $titleId", Toast.LENGTH_SHORT).show()
        }
        return foundGame
    }

    /**
     * Get the config file path for a title ID
     */
    private fun getConfigFile(titleId: String): File {
        val configDir = File(DirectoryInitialization.userDirectory, "config/custom")
        return File(configDir, "$titleId.ini")
    }

    /**
     * Write the config file with the custom settings
     */
    private fun writeConfigFile(titleId: String, customSettings: String): Boolean {
        return try {
            val configDir = File(DirectoryInitialization.userDirectory, "config/custom")
            if (!configDir.exists()) {
                configDir.mkdirs()
            }

            val configFile = File(configDir, "$titleId.ini")
            configFile.writeText(customSettings)

            Log.info("[CustomSettingsHandler] Wrote config file: ${configFile.absolutePath}")
            true
        } catch (e: Exception) {
            Log.error("[CustomSettingsHandler] Failed to write config file: ${e.message}")
            false
        }
    }

    /**
     * Ask user if they want to overwrite existing configuration
     */
    private suspend fun askUserToOverwriteConfig(activity: FragmentActivity, gameTitle: String): Boolean {
        return suspendCoroutine { continuation ->
            activity.runOnUiThread {
                MaterialAlertDialogBuilder(activity)
                    .setTitle("Configuration Already Exists")
                    .setMessage(
                        "Custom settings already exist for '$gameTitle'.\n\n" +
                                "Do you want to overwrite the existing configuration?\n\n" +
                                "This action cannot be undone."
                    )
                    .setPositiveButton("Overwrite") { _, _ ->
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
}
