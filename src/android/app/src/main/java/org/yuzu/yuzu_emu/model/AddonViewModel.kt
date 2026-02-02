// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.utils.NativeConfig
import java.util.concurrent.atomic.AtomicBoolean

class AddonViewModel : ViewModel() {
    private val _patchList = MutableStateFlow(mutableListOf<Patch>())
    val addonList get() = _patchList.asStateFlow()

    private val _showModInstallPicker = MutableStateFlow(false)
    val showModInstallPicker get() = _showModInstallPicker.asStateFlow()

    private val _showModNoticeDialog = MutableStateFlow(false)
    val showModNoticeDialog get() = _showModNoticeDialog.asStateFlow()

    private val _addonToDelete = MutableStateFlow<Patch?>(null)
    val addonToDelete = _addonToDelete.asStateFlow()

    var game: Game? = null
        private set

    private val isRefreshing = AtomicBoolean(false)

    fun onOpenAddons(game: Game) {
        if (this.game?.programId == game.programId && _patchList.value.isNotEmpty()) {
            return
        }
        this.game = game
        refreshAddons()
    }

    fun refreshAddons() {
        if (isRefreshing.get() || game == null) {
            return
        }
        isRefreshing.set(true)
        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                val patchList = (
                    NativeLibrary.getPatchesForFile(game!!.path, game!!.programId)
                        ?: emptyArray()
                    ).toMutableList()
                _patchList.value = sortPatchesWithCheatsGrouped(patchList)
                isRefreshing.set(false)
            }
        }
    }

    fun setAddonToDelete(patch: Patch?) {
        _addonToDelete.value = patch
    }

    fun onDeleteAddon(patch: Patch) {
        when (PatchType.from(patch.type)) {
            PatchType.Update -> NativeLibrary.removeUpdate(patch.programId)
            PatchType.DLC -> NativeLibrary.removeDLC(patch.programId)
            PatchType.Mod -> NativeLibrary.removeMod(patch.programId, patch.name)
            PatchType.Cheat -> {}
        }
        _patchList.value.clear()
        refreshAddons()
    }

    fun onCloseAddons() {
        if (_patchList.value.isEmpty()) {
            return
        }

        NativeConfig.setDisabledAddons(
            game!!.programId,
            _patchList.value.mapNotNull {
                if (it.enabled) {
                    null
                } else {
                    it.getStorageKey()
                }
            }.toTypedArray()
        )
        NativeConfig.saveGlobalConfig()
        _patchList.value.clear()
        game = null
    }

    fun showModInstallPicker(install: Boolean) {
        _showModInstallPicker.value = install
    }

    fun showModNoticeDialog(show: Boolean) {
        _showModNoticeDialog.value = show
    }

    private fun sortPatchesWithCheatsGrouped(patches: MutableList<Patch>): MutableList<Patch> {
        val individualCheats = patches.filter { it.isCheat() }
        val nonCheats = patches.filter { !it.isCheat() }.sortedBy { it.name }

        val cheatsByParent = individualCheats.groupBy { it.parentName }

        val result = mutableListOf<Patch>()
        for (patch in nonCheats) {
            result.add(patch)
            cheatsByParent[patch.name]?.sortedBy { it.name }?.let { childCheats ->
                result.addAll(childCheats)
            }
        }

        val knownParents = nonCheats.map { it.name }.toSet()
        for ((parentName, orphanCheats) in cheatsByParent) {
            if (parentName !in knownParents) {
                result.addAll(orphanCheats.sortedBy { it.name })
            }
        }

        return result
    }
}
