package org.yuzu.yuzu_emu.ui

import android.graphics.Rect
import android.view.View
import androidx.recyclerview.widget.RecyclerView

class OverlappingDecoration(private val overlapPx: Int) : RecyclerView.ItemDecoration() {
    override fun getItemOffsets(
        outRect: Rect, view: View, parent: RecyclerView, state: RecyclerView.State
    ) {
        val position = parent.getChildAdapterPosition(view)
        if (position != 0) {
            outRect.left = -overlapPx
        }
    }
}