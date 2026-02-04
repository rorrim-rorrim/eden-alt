// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.AsyncDifferConfig
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import org.yuzu.yuzu_emu.databinding.CardExternalContentDirBinding
import org.yuzu.yuzu_emu.model.ExternalContentViewModel

class ExternalContentAdapter(
    private val viewModel: ExternalContentViewModel
) : ListAdapter<String, ExternalContentAdapter.DirectoryViewHolder>(
    AsyncDifferConfig.Builder(DiffCallback()).build()
) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): DirectoryViewHolder {
        val binding = CardExternalContentDirBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false
        )
        return DirectoryViewHolder(binding)
    }

    override fun onBindViewHolder(holder: DirectoryViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    inner class DirectoryViewHolder(val binding: CardExternalContentDirBinding) :
        RecyclerView.ViewHolder(binding.root) {
        fun bind(path: String) {
            binding.textPath.text = path
            binding.buttonRemove.setOnClickListener {
                viewModel.removeDirectory(path)
            }
        }
    }

    private class DiffCallback : DiffUtil.ItemCallback<String>() {
        override fun areItemsTheSame(oldItem: String, newItem: String): Boolean {
            return oldItem == newItem
        }

        override fun areContentsTheSame(oldItem: String, newItem: String): Boolean {
            return oldItem == newItem
        }
    }
}
