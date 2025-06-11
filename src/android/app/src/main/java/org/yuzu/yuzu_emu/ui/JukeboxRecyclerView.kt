package org.yuzu.yuzu_emu.ui

import android.content.Context
import android.graphics.Rect
import android.util.AttributeSet
import android.view.View
import android.view.KeyEvent
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.PagerSnapHelper
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
    private var pagerSnapHelper: PagerSnapHelper? = null
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
            // Attach PagerSnapHelper
            if (pagerSnapHelper == null) {
                pagerSnapHelper = CenterPagerSnapHelper()
                pagerSnapHelper!!.attachToRecyclerView(this)
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
                            val distance = abs(center - childCenter)
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
            // Detach PagerSnapHelper
            pagerSnapHelper?.attachToRecyclerView(null)
            pagerSnapHelper = null
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

    // trap past boundaries navigation
    override fun focusSearch(focused: View, direction: Int): View? {
        val lm = layoutManager as? LinearLayoutManager ?: return super.focusSearch(focused, direction)
        val vh = findContainingViewHolder(focused) ?: return super.focusSearch(focused, direction)
        val position = vh.bindingAdapterPosition
        val itemCount = adapter?.itemCount ?: return super.focusSearch(focused, direction)

        return when (direction) {
            View.FOCUS_LEFT -> {
                if (position > 0) {
                    findViewHolderForAdapterPosition(position - 1)?.itemView ?: super.focusSearch(focused, direction)
                } else {
                    focused
                }
            }
            View.FOCUS_RIGHT -> {
                if (position < itemCount - 1) {
                    findViewHolderForAdapterPosition(position + 1)?.itemView ?: super.focusSearch(focused, direction)
                } else {
                    focused
                }
            }
            else -> super.focusSearch(focused, direction)
        }
    }

    // Custom fling multiplier for carousel
    override fun fling(velocityX: Int, velocityY: Int): Boolean {
        val newVelocityX = (velocityX * flingMultiplier).toInt()
        val newVelocityY = (velocityY * flingMultiplier).toInt()
        return super.fling(newVelocityX, newVelocityY)
    }

    // Custom drawing order for carousel (for alpha fade)
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

    // Enable proper center snapping
    inner class CenterPagerSnapHelper : PagerSnapHelper() {

        // allows center snapping
        override fun findSnapView(layoutManager: RecyclerView.LayoutManager): View? {
            if (layoutManager !is LinearLayoutManager) return null
            val center = layoutManager.paddingStart + (layoutManager.width - layoutManager.paddingStart - layoutManager.paddingEnd) / 2
            var minDistance = Int.MAX_VALUE
            var closestChild: View? = null
            for (i in 0 until layoutManager.childCount) {
                val child = layoutManager.getChildAt(i) ?: continue
                val childCenter = (child.left + child.right) / 2
                val distance = kotlin.math.abs(childCenter - center)
                if (distance < minDistance) {
                    minDistance = distance
                    closestChild = child
                }
            }
            return closestChild
        }

        // allows proper centering
        override fun calculateDistanceToFinalSnap(
            layoutManager: RecyclerView.LayoutManager,
            targetView: View
        ): IntArray? {
            if (layoutManager !is LinearLayoutManager) return super.calculateDistanceToFinalSnap(layoutManager, targetView)
            val out = IntArray(2)
            // Horizontal centering
            val center = layoutManager.paddingStart + (layoutManager.width - layoutManager.paddingStart - layoutManager.paddingEnd) / 2
            val childCenter = (targetView.left + targetView.right) / 2
            out[0] = childCenter - center
            // Vertical centering (not used for horizontal carousels)
            out[1] = 0
            return out
        }

        // allows inertial scrolling
        override fun findTargetSnapPosition(
            layoutManager: RecyclerView.LayoutManager,
            velocityX: Int,
            velocityY: Int
        ): Int {
            if (layoutManager !is LinearLayoutManager) return RecyclerView.NO_POSITION
            val forwardDirection = velocityX > 0
            val firstVisible = layoutManager.findFirstVisibleItemPosition()
            val lastVisible = layoutManager.findLastVisibleItemPosition()
            val center = layoutManager.paddingStart + (layoutManager.width - layoutManager.paddingStart - layoutManager.paddingEnd) / 2

            // Find the view closest to center
            var closestChild: View? = null
            var minDistance = Int.MAX_VALUE
            var closestPosition = RecyclerView.NO_POSITION
            for (i in firstVisible..lastVisible) {
                val child = layoutManager.findViewByPosition(i) ?: continue
                val childCenter = (child.left + child.right) / 2
                val distance = kotlin.math.abs(childCenter - center)
                if (distance < minDistance) {
                    minDistance = distance
                    closestChild = child
                    closestPosition = i
                }
            }

            // Estimate how many positions to move based on velocity
            val flingCount = if (velocityX == 0) 0 else velocityX / 2000 // Tune this divisor for your feel
            var targetPos = closestPosition + flingCount
            val itemCount = layoutManager.itemCount
            targetPos = targetPos.coerceIn(0, itemCount - 1)
            return targetPos
        }
    }
}