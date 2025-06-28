// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.ui

import android.util.Log
import android.content.Context
import android.graphics.Rect
import android.util.AttributeSet
import android.view.View
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.PagerSnapHelper
import androidx.recyclerview.widget.RecyclerView
import kotlin.math.abs
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.adapters.GameAdapter

/**
 * JukeboxRecyclerView encapsulates all carousel/grid/list logic for the games UI.
 * It manages overlapping cards, center snapping, custom drawing order, and mid-screen swipe-to-refresh.
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

    var flingMultiplier: Float = resources.getFraction(R.fraction.carousel_fling_multiplier, 1, 1)

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
     * Returns the horizontal center given width and paddings.
     */
    private fun calculateCenter(width: Int, paddingStart: Int, paddingEnd: Int): Int {
        return paddingStart + (width - paddingStart - paddingEnd) / 2
    }

    /**
     * Returns the horizontal center of this RecyclerView, accounting for padding.
     */
    private fun getRecyclerViewCenter(): Float {
        return calculateCenter(width, paddingLeft, paddingRight).toFloat()
    }

    /**
     * Returns the horizontal center of a LayoutManager, accounting for padding.
     */
    private fun getLayoutManagerCenter(layoutManager: RecyclerView.LayoutManager): Int {
        return if (layoutManager is LinearLayoutManager) {
            calculateCenter(layoutManager.width, layoutManager.paddingStart, layoutManager.paddingEnd)
        } else {
            width / 2
        }
    }

    //na jukebox
    fun getCenteredAdapterPosition(): Int {
        val lm = layoutManager as? LinearLayoutManager ?: return RecyclerView.NO_POSITION
        val center = getLayoutManagerCenter(lm)
        var minDistance = Int.MAX_VALUE
        var closestPosition = RecyclerView.NO_POSITION
        for (i in lm.findFirstVisibleItemPosition()..lm.findLastVisibleItemPosition()) {
            val child = lm.findViewByPosition(i) ?: continue
            val childCenter = (child.left + child.right) / 2
            val distance = kotlin.math.abs(childCenter - center)
            if (distance < minDistance) {
                minDistance = distance
                closestPosition = i
            }
        }
        Log.d("JukeboxRecyclerView", "Centered position: $closestPosition, distance: $minDistance")
        return closestPosition
    }

    fun updateChildScalesAndAlpha() {
        Log.d("JukeboxRecyclerView", "Updating child scales and alpha childCount ${childCount}")
        for (i in 0 until childCount) {
            val child = getChildAt(i) ?: continue
            updateChildScaleAndAlphaForPosition(child)
        }
    }

    fun updateChildScaleAndAlphaForPosition(child: View) {
        val cardSize = (adapter as? GameAdapter ?: return).cardSize
        val position = getChildViewHolder(child).bindingAdapterPosition
        if (position == RecyclerView.NO_POSITION || cardSize <= 0) {
            return // No valid position or card size
        }
        child.layoutParams.width = cardSize
        child.layoutParams.height = cardSize

        val center = getRecyclerViewCenter()
        val childCenter = (child.left + child.right) / 2f
        val distance = abs(center - childCenter)
        val minScale = resources.getFraction(R.fraction.carousel_min_scale, 1, 1)
        val scale = minScale + (1f - minScale) * (1f - distance / center).coerceAtMost(1f)

        val maxDistance = width / 2f
        val norm = (distance / maxDistance).coerceIn(0f, 1f)
        val minAlpha = resources.getFraction(R.fraction.carousel_min_alpha, 1, 1)
        val alpha = minAlpha + (1f - minAlpha) * kotlin.math.cos(norm * Math.PI).toFloat()

        child.animate().cancel()
        child.alpha = alpha
        child.scaleX = scale
        child.scaleY = scale

        //Log.d("JukeboxRecyclerView", "Child:$child c/cc:$center/$childCenter scale:$scale alpha:$alpha")
    }

    fun focusCenteredCard() {
        val centeredPos = getCenteredAdapterPosition()
        if (centeredPos != RecyclerView.NO_POSITION) {
            val vh = findViewHolderForAdapterPosition(centeredPos)
            vh?.itemView?.let { child ->
                child.isFocusable = true
                child.isFocusableInTouchMode = true
                child.requestFocus()
                Log.d("JukeboxRecyclerView", "Requested focus on centered card: $centeredPos")
            }
        }
    }

    /**
     * Enable or disable carousel mode.
     * When enabled, applies overlap, snap, and custom drawing order.
     */
    fun setCarouselMode(enabled: Boolean, cardSize: Int = 0) {
        if (enabled) {
            useCustomDrawingOrder = true
            overlapPx = (cardSize * resources.getFraction(R.fraction.carousel_overlap_factor, 1, 1)).toInt()
            flingMultiplier = resources.getFraction(R.fraction.carousel_fling_multiplier, 1, 1)

            // Detach SnapHelper during setup
            pagerSnapHelper?.attachToRecyclerView(null)

            // Add overlap decoration if not present
            if (overlapDecoration == null) {
                overlapDecoration = OverlappingDecoration(overlapPx)
                addItemDecoration(overlapDecoration!!)
            }

            // Gradual scalingAdd commentMore actions
            if (scalingScrollListener == null) {
                scalingScrollListener = object : OnScrollListener() {
                    override fun onScrolled(recyclerView: RecyclerView, dx: Int, dy: Int) {
                        super.onScrolled(recyclerView, dx, dy)
                        Log.d("JukeboxRecyclerView", "onScrolled dx=$dx, dy=$dy")
                        updateChildScalesAndAlpha()
                    }
                }
                addOnScrollListener(scalingScrollListener!!)
            }

            // Handle bottom insets for keyboard/navigation bar only
            setOnApplyWindowInsetsListener { view, insets ->
                val imeInset = insets.getInsets(android.view.WindowInsets.Type.ime()).bottom
                val navInset = insets.getInsets(android.view.WindowInsets.Type.navigationBars()).bottom
                view.setPadding(view.paddingLeft, 0, view.paddingRight, maxOf(imeInset, navInset))
                insets
            }

            // Center first/last card IMPORTANT!!
            post {
                if (cardSize > 0) {
                    val sidePadding = (width - cardSize) / 2
                    setPadding(sidePadding, 0, sidePadding, 0)
                    clipToPadding = false
                }
                // Attach PagerSnapHelper
                if (pagerSnapHelper == null) {
                    pagerSnapHelper = CenterPagerSnapHelper()
                    pagerSnapHelper!!.attachToRecyclerView(this)
                }
                post { //IMPORTANT: post² fixes the center carousel smol cards issue
                    Log.d("JukeboxRecyclerView", "Post² updateChildScalesAndAlpha")
                    updateChildScalesAndAlpha()
                    focusCenteredCard()

                    // Request focus on the centered card for joypad navigation
                    // val centeredPos = getCenteredAdapterPosition()
                    // if (centeredPos != RecyclerView.NO_POSITION) {
                    //     val vh = findViewHolderForAdapterPosition(centeredPos)
                    //     vh?.itemView?.let { child ->
                    //         child.isFocusable = true
                    //         child.isFocusableInTouchMode = true
                    //         child.requestFocus()
                    //         Log.d("JukeboxRecyclerView", "Requested focus on centered card: $centeredPos")
                    //     }
                    // }
                }
                Log.d("JukeboxRecyclerView", "Carousel mode enabled with overlapPx=$overlapPx, cardSize=$cardSize")
            }
        } else {
            // Remove overlap decoration
            overlapDecoration?.let { removeItemDecoration(it) }
            overlapDecoration = null
            // Remove scaling scroll listener
            scalingScrollListener?.let { removeOnScrollListener(it) }
            scalingScrollListener = null
            // Detach PagerSnapHelper
            pagerSnapHelper?.attachToRecyclerView(null)
            pagerSnapHelper = null
            useCustomDrawingOrder = false
            // Reset padding and fling
            setPadding(0, 0, 0, 0)
            clipToPadding = true
            flingMultiplier = 1.0f
            // Reset scaling
            for (i in 0 until childCount) {
                val child = getChildAt(i)
                child?.scaleX = 1f
                child?.scaleY = 1f
                child?.alpha = 1f
            }
        }
    }

    override fun scrollToPosition(position: Int) {
        super.scrollToPosition(position)

        if (position == 1) {//important to compensate for the overlap
            (layoutManager as? LinearLayoutManager)?.scrollToPositionWithOffset(position, overlapPx)
            Log.d("JukeboxRecyclerView", "Extra offset applied for position 1: $overlapPx")
        }

        post { //important to post to ensure layout is done
            Log.d("JukeboxRecyclerView", "Post scrollToPosition: $position")
            updateChildScalesAndAlpha()
            //Log.d("GamesFragment", "Scrolled to position: $position got ${getCenteredAdapterPosition()}")
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
                    val targetView = findViewHolderForAdapterPosition(position - 1)?.itemView
                        ?: super.focusSearch(focused, direction)
                    val offset = (targetView.scaleX * targetView.width / 2f).toInt()
                    smoothScrollBy(-offset, 0)
                    Log.d("JukeboxRecyclerView", "Focus left offset $offset, position $position")
                    targetView
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
        val center = getRecyclerViewCenter()
        val children = (0 until childCount).map { idx ->
            val child = getChildAt(idx)
            val childCenter = (child.left + child.right) / 2f
            val distance = abs(childCenter - center)
            Pair(idx, distance)
        }
        val sorted = children.sortedWith(
            compareByDescending<Pair<Int, Float>> { it.second }
                .thenBy { it.first }
        )
        //Log.d("JukeboxRecyclerView", "Child $i got order ${sorted[i].first} at distance ${sorted[i].second} from center $center")
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

        // NEEDED: fixes center snapping, but introduces ghost movement
        override fun findSnapView(layoutManager: RecyclerView.LayoutManager): View? {
            if (layoutManager !is LinearLayoutManager) return null
            val center = (this@JukeboxRecyclerView).getLayoutManagerCenter(layoutManager)
            var minDistance = Int.MAX_VALUE
            var closestChild: View? = null
            var  pos: Int = 0
            for (i in 0 until layoutManager.childCount) {
                val child = layoutManager.getChildAt(i) ?: continue
                val childCenter = (child.left + child.right) / 2
                val distance = kotlin.math.abs(childCenter - center)
                if (distance < minDistance) {
                    minDistance = distance
                    closestChild = child
                    pos = i
                }
            }
            Log.d("SnapHelper", "findSnapView Chosen child: $pos, minDistance=$minDistance")
            return closestChild
        }

        //NEEDED: fixes ghost movement when snapping, but breaks inertial scrolling
        override fun calculateDistanceToFinalSnap(
            layoutManager: RecyclerView.LayoutManager,
            targetView: View
        ): IntArray? {
            if (layoutManager !is LinearLayoutManager) return super.calculateDistanceToFinalSnap(layoutManager, targetView)
            val out = IntArray(2)
            val center = (this@JukeboxRecyclerView).getLayoutManagerCenter(layoutManager)
            val childCenter = (targetView.left + targetView.right) / 2
            out[0] = childCenter - center
            out[1] = 0
            return out
        }

        // NEEDED: fixes inertial scrolling (broken by calculateDistanceToFinalSnap)
        override fun findTargetSnapPosition(
            layoutManager: RecyclerView.LayoutManager,
            velocityX: Int,
            velocityY: Int
        ): Int {
            if (layoutManager !is LinearLayoutManager) return RecyclerView.NO_POSITION
            val firstVisible = layoutManager.findFirstVisibleItemPosition()
            val lastVisible = layoutManager.findLastVisibleItemPosition()
            val center = (this@JukeboxRecyclerView).getLayoutManagerCenter(layoutManager)

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

            val flingCount = if (velocityX == 0) 0 else velocityX / 2000
            var targetPos = closestPosition + flingCount
            val itemCount = layoutManager.itemCount
            targetPos = targetPos.coerceIn(0, itemCount - 1)
            Log.d("SnapHelper", "findTargetSnapPosition position: $targetPos")
            return targetPos
        }
    }
}