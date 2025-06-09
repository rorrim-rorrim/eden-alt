package org.yuzu.yuzu_emu.ui

import android.content.Context
import android.util.AttributeSet
import androidx.recyclerview.widget.RecyclerView

class JukeboxRecyclerView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0
) : RecyclerView(context, attrs, defStyle) {
    var flingMultiplier: Float = 2.0f

    init {
        setChildrenDrawingOrderEnabled(true)
    }

    var useCustomDrawingOrder: Boolean = false
        set(value) {
            field = value
            setChildrenDrawingOrderEnabled(value)
            invalidate()
        }

    override fun fling(velocityX: Int, velocityY: Int): Boolean {
        val newVelocityX = (velocityX * flingMultiplier).toInt()
        val newVelocityY = (velocityY * flingMultiplier).toInt()
        return super.fling(newVelocityX, newVelocityY)
    }

    override fun getChildDrawingOrder(childCount: Int, i: Int): Int {
        if (!useCustomDrawingOrder || childCount == 0) return i

        val center = width / 2
        // Build a list of (index, distance from center)
        val children = (0 until childCount).map { idx ->
            val child = getChildAt(idx)
            val childCenter = (child.left + child.right) / 2
            val distance = kotlin.math.abs(childCenter - center)
            Pair(idx, distance)
        }

        // Sort by distance: furthest first, center last
        val sorted = children.sortedWith(compareByDescending<Pair<Int, Int>> { it.second })
        // The last one in sorted (smallest distance) is the center card (drawn last/on top)
        return sorted[i].first
    }
}