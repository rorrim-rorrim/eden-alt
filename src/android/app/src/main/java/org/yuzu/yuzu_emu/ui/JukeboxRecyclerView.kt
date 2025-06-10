package org.yuzu.yuzu_emu.ui

import android.util.Log
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

    fun cleanupCarousel(
        scrollListener: RecyclerView.OnScrollListener?,
        snapHelper: androidx.recyclerview.widget.SnapHelper?
    ) {
        // Remove carousel-specific listeners and decorations
        scrollListener?.let { removeOnScrollListener(it) }
        snapHelper?.attachToRecyclerView(null)
        useCustomDrawingOrder = false

        // Remove all previous decorations to avoid stacking
        while (itemDecorationCount > 0) {
            removeItemDecorationAt(0)
        }

        Log.d("Padding", "resetting padding.")
        setPadding(0, 0, 0, 0)
        clipToPadding = true // or whatever your grid/list default is

        // Reset fling multiplier
        flingMultiplier = 1.0f
    }

    override fun fling(velocityX: Int, velocityY: Int): Boolean {
        val newVelocityX = (velocityX * flingMultiplier).toInt()
        val newVelocityY = (velocityY * flingMultiplier).toInt()
        return super.fling(newVelocityX, newVelocityY)
    }

    override fun getChildDrawingOrder(childCount: Int, i: Int): Int {
        if (!useCustomDrawingOrder || childCount == 0) return i

        val center = width / 2f
        val children = (0 until childCount).map { idx ->
            val child = getChildAt(idx)
            val childCenter = (child.left + child.right) / 2f
            val distance = kotlin.math.abs(childCenter - center)
            Pair(idx, distance)
        }
        // Sort by distance (furthest first), then by index (stable)
        val sorted = children.sortedWith(
            compareByDescending<Pair<Int, Float>> { it.second }
                .thenBy { it.first }
        )
        return sorted[i].first
    }
}