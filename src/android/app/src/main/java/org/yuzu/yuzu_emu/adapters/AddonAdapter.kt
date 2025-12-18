// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.constraintlayout.widget.ConstraintLayout
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.ListItemAddonBinding
import org.yuzu.yuzu_emu.model.Patch
import org.yuzu.yuzu_emu.model.PatchType
import org.yuzu.yuzu_emu.model.AddonViewModel
import org.yuzu.yuzu_emu.viewholder.AbstractViewHolder

class AddonAdapter(val addonViewModel: AddonViewModel) :
    AbstractDiffAdapter<Patch, AddonAdapter.AddonViewHolder>() {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AddonViewHolder {
        ListItemAddonBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            .also { return AddonViewHolder(it) }
    }

    inner class AddonViewHolder(val binding: ListItemAddonBinding) :
        AbstractViewHolder<Patch>(binding) {
        override fun bind(model: Patch) {
            binding.addonCard.setOnClickListener {
                binding.addonSwitch.performClick()
            }
            binding.title.text = model.name
            binding.addonSwitch.isChecked = model.enabled

            binding.addonSwitch.setOnCheckedChangeListener { _, checked ->
                model.enabled = checked
            }

            val isCheat = model.isCheat()
            val indentPx = if (isCheat) {
                binding.root.context.resources.getDimensionPixelSize(R.dimen.spacing_large)
            } else {
                0
            }
            (binding.addonCard.layoutParams as? ViewGroup.MarginLayoutParams)?.let {
                it.marginStart = indentPx
                binding.addonCard.layoutParams = it
            }

            if (isCheat) {
                binding.version.visibility = View.GONE
                binding.deleteCard.visibility = View.GONE
                binding.buttonDelete.visibility = View.GONE

                binding.addonSwitch.scaleX = 0.7f
                binding.addonSwitch.scaleY = 0.7f

                val compactPaddingVertical = binding.root.context.resources.getDimensionPixelSize(R.dimen.spacing_small)
                binding.root.setPadding(
                    binding.root.paddingLeft,
                    compactPaddingVertical / 2,
                    binding.root.paddingRight,
                    compactPaddingVertical / 2
                )

                val innerLayout = binding.addonCard.getChildAt(0) as? ViewGroup
                innerLayout?.let {
                    val leftPadding = binding.root.context.resources.getDimensionPixelSize(R.dimen.spacing_large)
                    val smallPadding = binding.root.context.resources.getDimensionPixelSize(R.dimen.spacing_med)
                    it.setPadding(leftPadding, smallPadding, smallPadding, smallPadding)
                }

                binding.title.textSize = 14f
                binding.title.gravity = Gravity.CENTER_VERTICAL

                (binding.title.layoutParams as? ConstraintLayout.LayoutParams)?.let { params ->
                    params.topToTop = ConstraintLayout.LayoutParams.PARENT_ID
                    params.bottomToBottom = ConstraintLayout.LayoutParams.PARENT_ID
                    params.topMargin = 0
                    binding.title.layoutParams = params
                }
            } else {
                binding.version.visibility = View.VISIBLE
                binding.version.text = model.version
                binding.deleteCard.visibility = View.VISIBLE
                binding.buttonDelete.visibility = View.VISIBLE

                binding.addonSwitch.scaleX = 1.0f
                binding.addonSwitch.scaleY = 1.0f

                val normalPadding = binding.root.context.resources.getDimensionPixelSize(R.dimen.spacing_med)
                binding.root.setPadding(
                    binding.root.paddingLeft,
                    normalPadding,
                    binding.root.paddingRight,
                    normalPadding
                )

                val innerLayout = binding.addonCard.getChildAt(0) as? ViewGroup
                innerLayout?.let {
                    val normalInnerPadding = binding.root.context.resources.getDimensionPixelSize(R.dimen.spacing_medlarge)
                    it.setPadding(normalInnerPadding, normalInnerPadding, normalInnerPadding, normalInnerPadding)
                }

                binding.title.textSize = 16f
                binding.title.gravity = Gravity.START

                (binding.title.layoutParams as? ConstraintLayout.LayoutParams)?.let { params ->
                    params.topToTop = ConstraintLayout.LayoutParams.PARENT_ID
                    params.bottomToBottom = ConstraintLayout.LayoutParams.UNSET
                    params.topMargin = 0
                    binding.title.layoutParams = params
                }

                val deleteAction = {
                    addonViewModel.setAddonToDelete(model)
                }
                binding.deleteCard.setOnClickListener { deleteAction() }
                binding.buttonDelete.setOnClickListener { deleteAction() }
            }
        }
    }
}
