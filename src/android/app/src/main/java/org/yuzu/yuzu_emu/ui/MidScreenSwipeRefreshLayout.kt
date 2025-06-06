package org.yuzu.yuzu_emu.ui

import android.content.Context
import android.util.AttributeSet
import android.view.MotionEvent
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout

class MidScreenSwipeRefreshLayout @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null
) : SwipeRefreshLayout(context, attrs) {

    private var startX = 0f

    override fun onInterceptTouchEvent(ev: MotionEvent): Boolean {
        when (ev.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                startX = ev.x
            }
            MotionEvent.ACTION_MOVE -> {
                val width = width
                val leftBound = width / 3
                val rightBound = width * 2 / 3
                // Only intercept if the gesture started in the middle third
                if (startX < leftBound || startX > rightBound) {
                    return false
                }
            }
        }
        return super.onInterceptTouchEvent(ev)
    }
}