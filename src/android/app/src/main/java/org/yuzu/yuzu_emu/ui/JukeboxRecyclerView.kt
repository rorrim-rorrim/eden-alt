package org.yuzu.yuzu_emu.ui

import android.content.Context
import android.graphics.Rect
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import android.view.ViewParent
import android.widget.LinearLayout
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import kotlin.math.abs

/**
 * JukeboxRecyclerView encapsulates all carousel/grid/list logic for the games UI.
 * It manages overlapping cards, center snapping, custom drawing order, and mid-screen swipe-to-refresh.
 * Use setCarouselMode(enabled, overlapPx) to toggle carousel features.
 */
class JukeboxRecyclerView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0
) : RecyclerView(context, attrs, defStyle) {

    // Carousel/overlap/snap state
    private var overlapPx: Int = 0
    private var overlapDecoration: OverlappingDecoration? = null
    private var snapHelper: OverlappingPagerSnap? = null
    private var scalingScrollListener: OnScrollListener? = null

    var flingMultiplier: Float = 2.0f

    var useCustomDrawingOrder: Boolean = false
        set(value) {
            field = value
            setChildrenDrawingOrderEnabled(value)
            invalidate()
        }

    init {
        setChildrenDrawingOrderEnabled(true)
    }

    /**
     * Enable or disable carousel mode.
     * When enabled, applies overlap, snap, and custom drawing order.
     */
    fun setCarouselMode(enabled: Boolean, overlapPx: Int = 0, cardSize: Int = 0) {
        this.overlapPx = overlapPx
        if (enabled) {
            // Add overlap decoration if not present
            if (overlapDecoration == null) {
                overlapDecoration = OverlappingDecoration(overlapPx)
                addItemDecoration(overlapDecoration!!)
            }
            // Attach custom snap helper
            if (snapHelper == null) {
                snapHelper = OverlappingPagerSnap(overlapPx)
                snapHelper!!.attachToRecyclerView(this)
            }
            useCustomDrawingOrder = true
            flingMultiplier = 2.0f
            // Center first/last card
            post {
                if (cardSize > 0) {
                    val sidePadding = (width - cardSize) / 2
                    setPadding(sidePadding, 0, sidePadding, 0)
                    clipToPadding = false
                }
            }
            // Gradual scaling
            if (scalingScrollListener == null) {
                scalingScrollListener = object : OnScrollListener() {
                    override fun onScrolled(rv: RecyclerView, dx: Int, dy: Int) {
                        val center = rv.width / 2f
                        for (i in 0 until rv.childCount) {
                            val child = rv.getChildAt(i)
                            val childCenter = (child.left + child.right) / 2f
                            val distance = kotlin.math.abs(center - childCenter)
                            val scale = 1f - 0.6f * (distance / center).coerceAtMost(1f)
                            child.scaleX = scale
                            child.scaleY = scale
                        }
                    }
                }
                addOnScrollListener(scalingScrollListener!!)
            }
            // Initial scale update
            post {
                scalingScrollListener?.onScrolled(this, 0, 0)
            }
        } else {
            // Remove overlap decoration
            overlapDecoration?.let { removeItemDecoration(it) }
            overlapDecoration = null
            // Detach snap helper
            snapHelper?.attachToRecyclerView(null)
            snapHelper = null
            useCustomDrawingOrder = false
            // Reset padding and fling
            setPadding(0, 0, 0, 0)
            clipToPadding = true
            flingMultiplier = 1.0f
            // Remove scaling scroll listener
            scalingScrollListener?.let { removeOnScrollListener(it) }
            scalingScrollListener = null
            // Reset scaling
            for (i in 0 until childCount) {
                val child = getChildAt(i)
                child?.scaleX = 1f
                child?.scaleY = 1f
            }
        }
    }

    // Custom fling multiplier for carousel
    override fun fling(velocityX: Int, velocityY: Int): Boolean {
        val newVelocityX = (velocityX * flingMultiplier).toInt()
        val newVelocityY = (velocityY * flingMultiplier).toInt()
        return super.fling(newVelocityX, newVelocityY)
    }

    // Custom drawing order for carousel
    override fun getChildDrawingOrder(childCount: Int, i: Int): Int {
        if (!useCustomDrawingOrder || childCount == 0) return i
        val center = width / 2f
        val children = (0 until childCount).map { idx ->
            val child = getChildAt(idx)
            val childCenter = (child.left + child.right) / 2f
            val distance = abs(childCenter - center)
            val maxDistance = width / 2f
            val norm = (distance / maxDistance).coerceIn(0f, 1f)
            val minAlpha = 0.6f
            val alpha = minAlpha + (1f - minAlpha) * kotlin.math.cos(norm * Math.PI).toFloat()
            child.alpha = alpha
            Pair(idx, distance)
        }
        val sorted = children.sortedWith(
            compareByDescending<Pair<Int, Float>> { it.second }
                .thenBy { it.first }
        )
        return sorted[i].first
    }

    // --- OverlappingDecoration (inner class) ---
    inner class OverlappingDecoration(private val overlapPx: Int) : ItemDecoration() {
        override fun getItemOffsets(
            outRect: Rect, view: View, parent: RecyclerView, state: State
        ) {
            val position = parent.getChildAdapterPosition(view)
            if (position > 0) {
                outRect.left = -overlapPx
            }
        }
    }

    // --- OverlappingPagerSnap (inner class) ---
    inner class OverlappingPagerSnap(private val overlapPx: Int) : OnFlingListener() {
        private var attachedRecyclerView: RecyclerView? = null

        fun attachToRecyclerView(rv: RecyclerView?) {
            attachedRecyclerView?.onFlingListener = null
            attachedRecyclerView = rv
            rv?.onFlingListener = this
            rv?.addOnScrollListener(object : OnScrollListener() {
                override fun onScrollStateChanged(recyclerView: RecyclerView, newState: Int) {
                    if (newState == SCROLL_STATE_IDLE) {
                        snapToCenter()
                    }
                }
            })
        }

        override fun onFling(velocityX: Int, velocityY: Int): Boolean {
            // Let the default fling happen, we'll snap in onScrollStateChanged
            return false
        }

        private fun snapToCenter() {
            val rv = attachedRecyclerView ?: return
            val lm = rv.layoutManager as? LinearLayoutManager ?: return
            val center = rv.paddingLeft + (rv.width - rv.paddingLeft - rv.paddingRight) / 2
            var minDistance = Int.MAX_VALUE
            var closestView: View? = null
            for (i in 0 until rv.childCount) {
                val child = rv.getChildAt(i)
                val childCenter = (child.left + child.right) / 2
                val distance = abs(childCenter - center)
                if (distance < minDistance) {
                    minDistance = distance
                    closestView = child
                }
            }
            closestView?.let {
                val position = rv.getChildAdapterPosition(it)
                rv.smoothScrollToPosition(position)
            }
        }
    }
}