// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.utils.NativeConfig

class ExternalContentViewModel : ViewModel() {
    private val _directories = MutableStateFlow(listOf<String>())
    val directories: StateFlow<List<String>> get() = _directories

    init {
        loadDirectories()
    }

    private fun loadDirectories() {
        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                _directories.value = NativeConfig.getExternalContentDirs().toList()
            }
        }
    }

    fun addDirectory(dir: DocumentFile) {
        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                val path = dir.uri.toString()
                val currentDirs = _directories.value.toMutableList()
                if (!currentDirs.contains(path)) {
                    currentDirs.add(path)
                    NativeConfig.setExternalContentDirs(currentDirs.toTypedArray())
                    _directories.value = currentDirs
                }
            }
        }
    }

    fun removeDirectory(path: String) {
        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                val currentDirs = _directories.value.toMutableList()
                currentDirs.remove(path)
                NativeConfig.setExternalContentDirs(currentDirs.toTypedArray())
                _directories.value = currentDirs
            }
        }
    }
}
