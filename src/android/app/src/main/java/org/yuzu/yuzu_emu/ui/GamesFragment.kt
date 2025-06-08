// SPDX-FileCopyrightText: Copyright yuzu/Citra Emulator Project / Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.ui

import android.content.Context
import android.content.Intent
import android.content.res.Configuration
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.inputmethod.InputMethodManager
import android.widget.ImageButton
import android.widget.PopupMenu
import android.widget.TextView
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.constraintlayout.widget.ConstraintLayout
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.core.widget.doOnTextChanged
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.navigation.fragment.findNavController
import androidx.preference.PreferenceManager
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.recyclerview.widget.LinearSnapHelper
import androidx.recyclerview.widget.LinearSmoothScroller
import com.google.android.material.color.MaterialColors
import info.debatty.java.stringsimilarity.Jaccard
import info.debatty.java.stringsimilarity.JaroWinkler
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.adapters.GameAdapter
import org.yuzu.yuzu_emu.databinding.FragmentGamesBinding
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.model.GamesViewModel
import org.yuzu.yuzu_emu.model.HomeViewModel
import org.yuzu.yuzu_emu.ui.main.MainActivity
import org.yuzu.yuzu_emu.utils.ViewUtils.setVisible
import org.yuzu.yuzu_emu.utils.collect
import java.util.Locale
import androidx.core.content.edit
import androidx.core.view.updateLayoutParams
import org.yuzu.yuzu_emu.features.settings.model.Settings

class GamesFragment : Fragment() {
    private var _binding: FragmentGamesBinding? = null
    private val binding get() = _binding!!

    private var originalHeaderTopMargin: Int? = null
    private var originalHeaderBottomMargin: Int? = null
    private var originalHeaderRightMargin: Int? = null
    private var originalHeaderLeftMargin: Int? = null

    companion object {
        private const val SEARCH_TEXT = "SearchText"
        private const val PREF_VIEW_TYPE = "GamesViewType"
        private const val PREF_SORT_TYPE = "GamesSortType"
    }

    private val gamesViewModel: GamesViewModel by activityViewModels()
    private val homeViewModel: HomeViewModel by activityViewModels()
    private lateinit var gameAdapter: GameAdapter

    private val preferences =
        PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)

    private lateinit var mainActivity: MainActivity
    private val getGamesDirectory =
        registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { result ->
            if (result != null) {
                mainActivity.processGamesDir(result, true)
            }
        }


    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentGamesBinding.inflate(inflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        homeViewModel.setStatusBarShadeVisibility(true)
        mainActivity = requireActivity() as MainActivity

        if (savedInstanceState != null) {
            binding.searchText.setText(savedInstanceState.getString(SEARCH_TEXT))
        }

        gameAdapter = GameAdapter(
            requireActivity() as AppCompatActivity,
        )

        applyGridGamesBinding()

        binding.gridGames.viewTreeObserver.addOnGlobalLayoutListener {
            val isPortrait = resources.configuration.orientation == Configuration.ORIENTATION_PORTRAIT
            val layoutParams = binding.gridGames.layoutParams as ViewGroup.MarginLayoutParams
            val topMargin = layoutParams.topMargin
            val bottomMargin = layoutParams.bottomMargin
            val availableHeight = binding.gridGames.height - topMargin - bottomMargin

            val extraPadding = resources.getDimensionPixelSize(R.dimen.spacing_large) // or a fixed value like 16/24dp
            val maxScale = 1f // or 1.1f if you ever increase the center card scale
            val size = if (isPortrait) {
                (binding.gridGames.width * 0.6f).toInt()
            } else {
                ((availableHeight - extraPadding) / maxScale).toInt()
            }
            gameAdapter.setCardSize(size)
        }

        binding.swipeRefresh.apply {
            // Add swipe down to refresh gesture
            setOnRefreshListener {
                gamesViewModel.reloadGames(false)
            }

            // Set theme color to the refresh animation's background
            setProgressBackgroundColorSchemeColor(
                MaterialColors.getColor(
                    binding.swipeRefresh,
                    com.google.android.material.R.attr.colorPrimary
                )
            )
            setColorSchemeColors(
                MaterialColors.getColor(
                    binding.swipeRefresh,
                    com.google.android.material.R.attr.colorOnPrimary
                )
            )

            // Make sure the loading indicator appears even if the layout is told to refresh before being fully drawn
            post {
                if (_binding == null) {
                    return@post
                }
                binding.swipeRefresh.isRefreshing = gamesViewModel.isReloading.value
            }
        }

        gamesViewModel.isReloading.collect(viewLifecycleOwner) {
            binding.swipeRefresh.isRefreshing = it
            binding.noticeText.setVisible(
                visible = gamesViewModel.games.value.isEmpty() && !it,
                gone = false
            )
        }
        gamesViewModel.games.collect(viewLifecycleOwner) {
            setAdapter(it)
        }
        gamesViewModel.shouldSwapData.collect(
            viewLifecycleOwner,
            resetState = { gamesViewModel.setShouldSwapData(false) }
        ) {
            if (it) {
                setAdapter(gamesViewModel.games.value)
            }
        }
        gamesViewModel.shouldScrollToTop.collect(
            viewLifecycleOwner,
            resetState = { gamesViewModel.setShouldScrollToTop(false) }
        ) { if (it) scrollToTop() }

        setupTopView()

        binding.addDirectory.setOnClickListener {
            getGamesDirectory.launch(Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).data)
        }

        setInsets()
        val shouldDisplayPreAlphaBanner =
            PreferenceManager.getDefaultSharedPreferences(requireContext())
                .getBoolean(Settings.PREF_SHOULD_SHOW_PRE_ALPHA_BANNER, true)
        if (shouldDisplayPreAlphaBanner) {
            addPreAlphaBanner()
        }
    }

    val applyGridGamesBinding = {
        binding.gridGames.apply {
            val savedViewType = preferences.getInt(PREF_VIEW_TYPE, GameAdapter.VIEW_TYPE_GRID)
            gameAdapter.setViewType(savedViewType)
            currentFilter = preferences.getInt(PREF_SORT_TYPE, View.NO_ID)
            adapter = gameAdapter

            // Reset scaling and alpha for all children when leaving carousel mode
            if (savedViewType != GameAdapter.VIEW_TYPE_CAROUSEL) {
                for (i in 0 until binding.gridGames.childCount) {
                    val child = binding.gridGames.getChildAt(i)
                    child.scaleX = 1f
                    child.scaleY = 1f
                    child.alpha = 1f
                }
                carouselScrollListener?.let { binding.gridGames.removeOnScrollListener(it) }
                carouselSnapHelper?.attachToRecyclerView(null)
                gameAdapter.setCardSize(0)
            }

            if (resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE &&
                savedViewType == GameAdapter.VIEW_TYPE_CAROUSEL
            ) {
                (binding.gridGames as? FastFlingRecyclerView)?.flingMultiplier = 2.5f // or higher
            } else {
                (binding.gridGames as? FastFlingRecyclerView)?.flingMultiplier = 1.0f
            }

            layoutManager = when (savedViewType) {
                GameAdapter.VIEW_TYPE_LIST -> {
                    val columns = resources.getInteger(R.integer.game_columns_list)
                    GridLayoutManager(requireContext(), columns)
                }
                GameAdapter.VIEW_TYPE_GRID -> {
                    val columns = resources.getInteger(R.integer.game_columns_grid)
                    GridLayoutManager(requireContext(), columns)
                }
                GameAdapter.VIEW_TYPE_CAROUSEL -> {
                    class FastLinearLayoutManager(context: Context) : LinearLayoutManager(context, HORIZONTAL, false) {

                        override fun smoothScrollToPosition(
                            recyclerView: RecyclerView,
                            state: RecyclerView.State,
                            position: Int
                        ) {
                            val smoothScroller = object : LinearSmoothScroller(recyclerView.context) {
                                override fun calculateSpeedPerPixel(displayMetrics: android.util.DisplayMetrics): Float {
                                    // Lower value = faster scroll. Default is about 25f.
                                    return 10f / displayMetrics.densityDpi
                                }
                            }
                            smoothScroller.targetPosition = position
                            startSmoothScroll(smoothScroller)
                        }
                    }

                    // Remove previous snap helper if any
                    carouselSnapHelper?.attachToRecyclerView(null)

                    // Create and attach a new one
                    carouselSnapHelper = LinearSnapHelper().also { it.attachToRecyclerView(binding.gridGames) }

                    // Remove previous scroll listener if any
                    carouselScrollListener?.let { binding.gridGames.removeOnScrollListener(it) }

                    // Create and add a new scroll listener
                    carouselScrollListener = object : RecyclerView.OnScrollListener() {
                        override fun onScrolled(recyclerView: RecyclerView, dx: Int, dy: Int) {
                            val center = recyclerView.width / 2f
                            for (i in 0 until recyclerView.childCount) {
                                val child = recyclerView.getChildAt(i)
                                val childCenter = (child.left + child.right) / 2f
                                val distanceFromCenter = Math.abs(center - childCenter)
                                val maxDistance = center * 1.2f // adjust for how quickly scaling falls off

                                // Increase the ratio here: scale drops off more for peripheric cards
                                val scale = 1f - 0.70f * (distanceFromCenter / maxDistance).coerceAtMost(1f)
                                child.scaleX = scale
                                child.scaleY = scale
                                child.alpha = 0.5f + 0.5f * scale // optional: fade out peripheric cards
                            }
                        }
                    }
                    binding.gridGames.addOnScrollListener(carouselScrollListener!!)

                    // Carousel: horizontal scrolling, 1 row
                    val layoutManager = if (resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE) {
                        FastLinearLayoutManager(requireContext())
                    } else {
                        LinearLayoutManager(requireContext(), LinearLayoutManager.HORIZONTAL, false)
                    }

                    /* tentativa de ajustar aceleração do fling
                    // Set the custom fling listener for carousel in landscape
                    if (resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE) {
                        binding.gridGames.setOnFlingListener(object : RecyclerView.OnFlingListener() {
                            override fun onFling(velocityX: Int, velocityY: Int): Boolean {
                                binding.gridGames.fling(velocityX * 2, velocityY)
                                return true
                            }
                        })
                    } else {
                        binding.gridGames.setOnFlingListener(null)
                    }
                    */

                    layoutManager
                }
                else -> {
                    val columns = resources.getInteger(R.integer.game_columns_grid)
                    GridLayoutManager(requireContext(), columns)
                }
            }
        }
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        if (_binding != null) {
            outState.putString(SEARCH_TEXT, binding.searchText.text.toString())
        }
    }

    private fun setAdapter(games: List<Game>) {
        val currentSearchText = binding.searchText.text.toString()
        val currentFilter = binding.filterButton.id


        if (currentSearchText.isNotEmpty() || currentFilter != View.NO_ID) {
            filterAndSearch(games)
        } else {
            (binding.gridGames.adapter as GameAdapter).submitList(games)
            gamesViewModel.setFilteredGames(games)
        }
    }

    private fun setupTopView() {
        binding.searchText.doOnTextChanged() { text: CharSequence?, _: Int, _: Int, _: Int ->
            if (text.toString().isNotEmpty()) {
                binding.clearButton.visibility = View.VISIBLE
            } else {
                binding.clearButton.visibility = View.INVISIBLE
            }
            filterAndSearch()
        }

        binding.clearButton.setOnClickListener { binding.searchText.setText("") }
        binding.searchBackground.setOnClickListener { focusSearch() }

        // Setup view button
        binding.viewButton.setOnClickListener { showViewMenu(it) }

        // Setup filter button
        binding.filterButton.setOnClickListener { view ->
            showFilterMenu(view)
        }

        // Setup settings button
        binding.settingsButton.setOnClickListener { navigateToSettings() }
    }

    private fun navigateToSettings() {
        val navController = findNavController()
        navController.navigate(R.id.action_gamesFragment_to_homeSettingsFragment)
    }

    private fun addPreAlphaBanner() {
        val preAlphaBanner = TextView(requireContext()).apply {
            id = "pre_alpha_banner".hashCode()
            layoutParams = ConstraintLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            ).apply {
                marginStart = resources.getDimensionPixelSize(R.dimen.spacing_med)
                marginEnd = resources.getDimensionPixelSize(R.dimen.spacing_med)
                topMargin = resources.getDimensionPixelSize(R.dimen.spacing_large)
                topToBottom = R.id.frame_search
            }
            setPadding(
                resources.getDimensionPixelSize(R.dimen.spacing_med),
                resources.getDimensionPixelSize(R.dimen.spacing_large),
                resources.getDimensionPixelSize(R.dimen.spacing_med),
                resources.getDimensionPixelSize(R.dimen.spacing_med)
            )

            setBackgroundColor(
                MaterialColors.getColor(
                    this,
                    com.google.android.material.R.attr.colorPrimary
                )
            )
            text = getString(R.string.pre_alpha_warning)
            setTextAppearance(
                com.google.android.material.R.style.TextAppearance_Material3_HeadlineSmall
            )
            setTextColor(
                MaterialColors.getColor(
                    this,
                    com.google.android.material.R.attr.colorOnError
                )
            )
            gravity = Gravity.CENTER
        }

        val closeButton = ImageButton(requireContext()).apply {
            id = "pre_alpha_close_button".hashCode()
            layoutParams = ConstraintLayout.LayoutParams(
                resources.getDimensionPixelSize(R.dimen.spacing_large),
                resources.getDimensionPixelSize(R.dimen.spacing_large)
            ).apply {
                startToStart = "pre_alpha_banner".hashCode()
                topToTop = "pre_alpha_banner".hashCode()
                bottomToBottom = "pre_alpha_banner".hashCode()
                marginStart = resources.getDimensionPixelSize(R.dimen.spacing_large) * 2
                topMargin = resources.getDimensionPixelSize(R.dimen.spacing_small)
            }
            setImageResource(android.R.drawable.ic_menu_close_clear_cancel)
            setColorFilter(
                MaterialColors.getColor(
                    this,
                    com.google.android.material.R.attr.colorOnError
                )
            )
            setBackgroundColor(Color.Transparent.toArgb())
            setOnClickListener {
                PreferenceManager.getDefaultSharedPreferences(requireContext())
                    .edit() {
                        putBoolean(Settings.PREF_SHOULD_SHOW_PRE_ALPHA_BANNER, false)
                    }
                binding.root.removeView(preAlphaBanner)
                binding.root.removeView(this)

                binding.swipeRefresh.updateLayoutParams<ConstraintLayout.LayoutParams> {
                    topToBottom = R.id.frame_search
                }
            }
        }

        binding.root.addView(preAlphaBanner)
        binding.root.addView(closeButton)

        binding.swipeRefresh.updateLayoutParams<ConstraintLayout.LayoutParams> {
            topToBottom = preAlphaBanner.id
        }
    }

    private fun showViewMenu(anchor: View) {
        val popup = PopupMenu(requireContext(), anchor)
        popup.menuInflater.inflate(R.menu.menu_game_views, popup.menu)

        val currentViewType = (preferences.getInt(PREF_VIEW_TYPE, GameAdapter.VIEW_TYPE_GRID))
        when (currentViewType) {
            GameAdapter.VIEW_TYPE_LIST -> popup.menu.findItem(R.id.view_list).isChecked = true
            GameAdapter.VIEW_TYPE_GRID -> popup.menu.findItem(R.id.view_grid).isChecked = true
            GameAdapter.VIEW_TYPE_CAROUSEL -> popup.menu.findItem(R.id.view_carousel).isChecked = true
        }

        popup.setOnMenuItemClickListener { item ->
            when (item.itemId) {
                R.id.view_grid -> {
                    preferences.edit() { putInt(PREF_VIEW_TYPE, GameAdapter.VIEW_TYPE_GRID) }
                    applyGridGamesBinding()
                    item.isChecked = true
                    true
                }

                R.id.view_list -> {
                    preferences.edit() { putInt(PREF_VIEW_TYPE, GameAdapter.VIEW_TYPE_LIST) }
                    applyGridGamesBinding()
                    item.isChecked = true
                    true
                }

                R.id.view_carousel -> {
                    preferences.edit() { putInt(PREF_VIEW_TYPE, GameAdapter.VIEW_TYPE_CAROUSEL) }
                    applyGridGamesBinding()
                    item.isChecked = true
                    true
                }

                else -> false
            }
        }

        popup.show()
    }

    private fun showFilterMenu(anchor: View) {
        val popup = PopupMenu(requireContext(), anchor)
        popup.menuInflater.inflate(R.menu.menu_game_filters, popup.menu)

        // Set checked state based on current filter
        when (currentFilter) {
            R.id.alphabetical -> popup.menu.findItem(R.id.alphabetical).isChecked = true
            R.id.filter_recently_played -> popup.menu.findItem(R.id.filter_recently_played).isChecked =
                true

            R.id.filter_recently_added -> popup.menu.findItem(R.id.filter_recently_added).isChecked =
                true
        }

        popup.setOnMenuItemClickListener { item ->
            currentFilter = item.itemId
            preferences.edit().putInt(PREF_SORT_TYPE, currentFilter).apply()
            filterAndSearch()
            true
        }

        popup.show()
    }

    // Track current filter
    private var currentFilter = View.NO_ID

    private fun filterAndSearch(baseList: List<Game> = gamesViewModel.games.value) {
        val filteredList: List<Game> = when (currentFilter) {
            R.id.alphabetical -> baseList.sortedBy { it.title }
            R.id.filter_recently_played -> {
                baseList.filter {
                    val lastPlayedTime = preferences.getLong(it.keyLastPlayedTime, 0L)
                    lastPlayedTime > (System.currentTimeMillis() - 24 * 60 * 60 * 1000)
                }.sortedByDescending { preferences.getLong(it.keyLastPlayedTime, 0L) }
            }

            R.id.filter_recently_added -> {
                baseList.filter {
                    val addedTime = preferences.getLong(it.keyAddedToLibraryTime, 0L)
                    addedTime > (System.currentTimeMillis() - 24 * 60 * 60 * 1000)
                }.sortedByDescending { preferences.getLong(it.keyAddedToLibraryTime, 0L) }
            }

            else -> baseList
        }

        val searchTerm = binding.searchText.text.toString().lowercase(Locale.getDefault())
        if (searchTerm.isEmpty()) {
            (binding.gridGames.adapter as GameAdapter).submitList(filteredList)
            gamesViewModel.setFilteredGames(filteredList)
            return
        }

        val searchAlgorithm = if (searchTerm.length > 1) Jaccard(2) else JaroWinkler()
        val sortedList = filteredList.mapNotNull { game ->
            val title = game.title.lowercase(Locale.getDefault())
            val score = searchAlgorithm.similarity(searchTerm, title)
            if (score > 0.03) {
                ScoredGame(score, game)
            } else {
                null
            }
        }.sortedByDescending { it.score }.map { it.item }

        (binding.gridGames.adapter as GameAdapter).submitList(sortedList)
        gamesViewModel.setFilteredGames(sortedList)
    }

    private inner class ScoredGame(val score: Double, val item: Game)

    private fun focusSearch() {
        binding.searchText.requestFocus()
        val imm = requireActivity()
            .getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager?
        imm?.showSoftInput(binding.searchText, InputMethodManager.SHOW_IMPLICIT)
    }


    override fun onDestroyView() {
        super.onDestroyView()
        carouselSnapHelper?.attachToRecyclerView(null)
        carouselScrollListener?.let { binding.gridGames.removeOnScrollListener(it) }
        _binding = null
    }

    private fun scrollToTop() {
        if (_binding != null) {
            binding.gridGames.smoothScrollToPosition(0)
        }
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())
            val spacingNavigation = resources.getDimensionPixelSize(R.dimen.spacing_navigation)
            resources.getDimensionPixelSize(R.dimen.spacing_navigation_rail)

            binding.swipeRefresh.setProgressViewEndTarget(
                false,
                barInsets.top + resources.getDimensionPixelSize(R.dimen.spacing_refresh_end)
            )

            val leftInset = barInsets.left + cutoutInsets.left
            val rightInset = barInsets.right + cutoutInsets.right
            val topInset = maxOf(barInsets.top, cutoutInsets.top)

            val mlpSwipe = binding.swipeRefresh.layoutParams as ViewGroup.MarginLayoutParams
            mlpSwipe.leftMargin = leftInset
            mlpSwipe.rightMargin = rightInset
            binding.swipeRefresh.layoutParams = mlpSwipe

            val mlpHeader = binding.header.layoutParams as ViewGroup.MarginLayoutParams

            // Store original margins only once
            if (originalHeaderTopMargin == null) {
                originalHeaderTopMargin = mlpHeader.topMargin
                originalHeaderRightMargin = mlpHeader.rightMargin
                originalHeaderLeftMargin = mlpHeader.leftMargin
            }

            // Always set margin as original + insets
            mlpHeader.leftMargin = (originalHeaderLeftMargin ?: 0) + leftInset
            mlpHeader.rightMargin = (originalHeaderRightMargin ?: 0) + rightInset
            mlpHeader.topMargin = (originalHeaderTopMargin ?: 0) + topInset + resources.getDimensionPixelSize(R.dimen.spacing_med)
            binding.header.layoutParams = mlpHeader

            binding.noticeText.updatePadding(bottom = spacingNavigation)

            binding.gridGames.updatePadding(
                top = resources.getDimensionPixelSize(R.dimen.spacing_med)
            )

            val mlpFab = binding.addDirectory.layoutParams as ViewGroup.MarginLayoutParams
            val fabPadding = resources.getDimensionPixelSize(R.dimen.spacing_large)
            mlpFab.leftMargin = leftInset + fabPadding
            mlpFab.bottomMargin = barInsets.bottom + fabPadding
            mlpFab.rightMargin = rightInset + fabPadding
            binding.addDirectory.layoutParams = mlpFab

            windowInsets
        }
}
