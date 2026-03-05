// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.app.Activity
import android.content.Context
import androidx.core.content.edit
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.features.settings.model.Settings

object FullscreenHelper {
    fun isFullscreenEnabled(context: Context): Boolean {
        return PreferenceManager.getDefaultSharedPreferences(context).getBoolean(
            Settings.PREF_APP_FULLSCREEN,
            Settings.APP_FULLSCREEN_DEFAULT
        )
    }

    fun setFullscreenEnabled(context: Context, enabled: Boolean) {
        PreferenceManager.getDefaultSharedPreferences(context).edit {
            putBoolean(Settings.PREF_APP_FULLSCREEN, enabled)
        }
    }

    fun applyToActivity(activity: Activity) {
        val controller = WindowInsetsControllerCompat(activity.window, activity.window.decorView)

        if (isFullscreenEnabled(activity)) {
            controller.hide(WindowInsetsCompat.Type.systemBars())
            controller.systemBarsBehavior =
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        } else {
            controller.show(WindowInsetsCompat.Type.systemBars())
        }
    }
}
