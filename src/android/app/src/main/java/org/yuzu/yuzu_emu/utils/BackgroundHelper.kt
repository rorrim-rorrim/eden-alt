// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.Context
import android.view.View
import androidx.core.content.edit
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.Settings

object BackgroundHelper {
    fun getBackgroundStyle(context: Context): Int {
        return PreferenceManager.getDefaultSharedPreferences(context).getInt(
            Settings.PREF_HOME_BACKGROUND_STYLE,
            Settings.HOME_BACKGROUND_STYLE_DEFAULT
        )
    }

    fun setBackgroundStyle(context: Context, style: Int) {
        PreferenceManager.getDefaultSharedPreferences(context).edit {
            putInt(Settings.PREF_HOME_BACKGROUND_STYLE, style)
        }
    }

    fun getBackgroundAlpha(context: Context): Float {
        val alphaPercent = PreferenceManager.getDefaultSharedPreferences(context).getInt(
            Settings.PREF_HOME_BACKGROUND_ALPHA,
            Settings.HOME_BACKGROUND_ALPHA_DEFAULT
        ).coerceIn(0, 100)
        return alphaPercent / 100f
    }

    fun setBackgroundAlpha(context: Context, alphaPercent: Int) {
        PreferenceManager.getDefaultSharedPreferences(context).edit {
            putInt(Settings.PREF_HOME_BACKGROUND_ALPHA, alphaPercent.coerceIn(0, 100))
        }
    }

    fun applyBackground(view: View, context: Context) {
        val isEnabled = getBackgroundStyle(context) != Settings.HOME_BACKGROUND_STYLE_NONE
        val visibilityStrength = getBackgroundAlpha(context)

        val parent = view.parent as? View
        val scrim = parent?.findViewById<View>(R.id.background_scrim)

        view.visibility = if (isEnabled) View.VISIBLE else View.GONE

        if (!isEnabled) {
            scrim?.visibility = View.GONE
            return
        }

        if (scrim != null) {
            // Keep background image opaque; control perceived intensity through a cheap flat-color scrim.
            view.alpha = 1f
            scrim.visibility = View.VISIBLE
            scrim.alpha = (1f - visibilityStrength).coerceIn(0f, 1f)
        } else {
            view.alpha = visibilityStrength
        }
    }
}
