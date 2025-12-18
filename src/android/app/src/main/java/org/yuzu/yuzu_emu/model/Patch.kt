// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.annotation.Keep

@Keep
data class Patch(
    var enabled: Boolean,
    val name: String,
    val version: String,
    val type: Int,
    val programId: String,
    val titleId: String,
    val parentName: String = ""  // For cheats: name of the mod folder containing them
) {
    /**
     * Returns the storage key used for saving enabled/disabled state.
     * For cheats with a parent, returns "ParentName::CheatName".
     */
    fun getStorageKey(): String {
        return if (parentName.isNotEmpty()) {
            "$parentName::$name"
        } else {
            name
        }
    }

    /**
     * Returns true if this patch is an individual cheat entry (not a cheat mod).
     * Individual cheats have type=Cheat and a parent mod name.
     */
    fun isCheat(): Boolean = type == PatchType.Cheat.int && parentName.isNotEmpty()
}
