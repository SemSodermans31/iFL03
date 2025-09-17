document.addEventListener('DOMContentLoaded', function() {
    // Preview buttons functionality
    var buttons = document.querySelectorAll('.preview-btn');
    buttons.forEach(function(btn) {
        btn.addEventListener('click', function() {
            // Check if this button is already open
            var isCurrentlyOpen = btn.classList.contains('show-explanation');

            // Close all buttons first
            buttons.forEach(function(otherBtn) {
                otherBtn.classList.remove('show-explanation');
            });

            // If this button wasn't open, open it now
            if (!isCurrentlyOpen) {
                btn.classList.add('show-explanation');
            }
        });
    });

    // Horizontal scroll gallery functionality
    const scrollContainer = document.querySelector('.scrollbar-hide');
    const scrollDots = document.querySelectorAll('#scroll-dots .w-2');
    const playPauseBtn = document.getElementById('play-pause-btn');
    const playIcon = document.getElementById('play-icon');
    const pauseIcon = document.getElementById('pause-icon');

    let currentSlide = 0;
    let isPlaying = true; // Start with auto-play enabled
    let autoScrollInterval;
    let isProgrammaticScroll = false;

    // Function to update active dot
    function updateActiveDot() {
        scrollDots.forEach((dot, index) => {
            if (index === currentSlide) {
                dot.classList.remove('bg-slate-400');
                dot.classList.add('bg-white');
            } else {
                dot.classList.remove('bg-white');
                dot.classList.add('bg-slate-400');
            }
        });
    }

    // Function to scroll to specific slide
    function scrollToSlide(slideIndex) {
        const slides = document.querySelectorAll('.scroll-snap-align-center');
        const container = document.querySelector('.scrollbar-hide');
        if (slides[slideIndex] && container) {
            currentSlide = slideIndex;
            const slide = slides[slideIndex];
            const containerRect = container.getBoundingClientRect();
            const slideRect = slide.getBoundingClientRect();
            const scrollLeft = container.scrollLeft + (slideRect.left - containerRect.left) - (containerRect.width / 2) + (slideRect.width / 2);

            isProgrammaticScroll = true;
            container.scrollTo({
                left: scrollLeft,
                behavior: 'smooth'
            });
            updateActiveDot();
            setTimeout(() => { isProgrammaticScroll = false; }, 300);
        }
    }

    // Function to determine which slide is currently in view
    function updateCurrentSlideFromScroll() {
        const slides = document.querySelectorAll('.scroll-snap-align-center');
        const container = document.querySelector('.scrollbar-hide');
        const containerRect = container.getBoundingClientRect();
        const containerCenter = containerRect.left + containerRect.width / 2;

        let closestSlide = 0;
        let minDistance = Infinity;

        slides.forEach((slide, index) => {
            const slideRect = slide.getBoundingClientRect();
            const slideCenter = slideRect.left + slideRect.width / 2;
            const distance = Math.abs(containerCenter - slideCenter);

            if (distance < minDistance) {
                minDistance = distance;
                closestSlide = index;
            }
        });

        if (closestSlide !== currentSlide) {
            currentSlide = closestSlide;
            updateActiveDot();
        }
    }

    // Function to start auto-scrolling
    function startAutoScroll() {
        if (autoScrollInterval) clearInterval(autoScrollInterval);
        autoScrollInterval = setInterval(() => {
            currentSlide = (currentSlide + 1) % 5;
            scrollToSlide(currentSlide);
        }, 6000);
    }

    // Function to stop auto-scrolling
    function stopAutoScroll() {
        if (autoScrollInterval) {
            clearInterval(autoScrollInterval);
            autoScrollInterval = null;
        }
    }

    // Function to toggle play/pause
    function togglePlayPause() {
        isPlaying = !isPlaying;

        if (isPlaying) {
            // Switch to pause state
            playIcon.classList.add('hidden');
            pauseIcon.classList.remove('hidden');
            startAutoScroll();
        } else {
            // Switch to play state
            pauseIcon.classList.add('hidden');
            playIcon.classList.remove('hidden');
            stopAutoScroll();
        }
    }

    // Event listeners for dots
    scrollDots.forEach((dot, index) => {
        dot.addEventListener('click', () => {
            scrollToSlide(index);
            // Stop auto-scroll if it's playing
            if (isPlaying) {
                togglePlayPause();
            }
        });
    });

    // Event listeners for image clicks (to center the clicked image)
    const slideContainers = document.querySelectorAll('.scroll-snap-align-center');
    slideContainers.forEach((container, index) => {
        container.addEventListener('click', () => {
            scrollToSlide(index);
            // Stop auto-scroll if it's playing
            if (isPlaying) {
                togglePlayPause();
            }
        });
    });

    // Event listener for play/pause button
    playPauseBtn.addEventListener('click', togglePlayPause);

    // Listen for scroll events to update active dot and limit scroll bounds
    scrollContainer.addEventListener('scroll', () => {
        // Skip if this is a programmatic scroll
        if (isProgrammaticScroll) return;

        // Use requestAnimationFrame for better performance
        requestAnimationFrame(() => {
            updateCurrentSlideFromScroll();

            // Prevent scrolling beyond the 5th slide (index 4)
            const slides = document.querySelectorAll('.scroll-snap-align-center');
            if (slides.length >= 5 && currentSlide > 4) {
                // User scrolled to empty slides, snap back to 5th slide
                isProgrammaticScroll = true;
                scrollToSlide(4);
                setTimeout(() => { isProgrammaticScroll = false; }, 300);
            }
        });
    });

    // Initialize active dot
    updateActiveDot();

    // Start auto-play by default
    startAutoScroll();

    // Ensure initial icon state shows Pause (since auto-play is active)
    playIcon.classList.add('hidden');
    pauseIcon.classList.remove('hidden');

    // Overlay Gallery Navigation
    const overlayGallery = document.getElementById('overlay-gallery');
    const overlayPrevBtn = document.getElementById('overlay-nav-prev');
    const overlayNextBtn = document.getElementById('overlay-nav-next');
    let currentOverlayIndex = 0;
    const overlayItems = document.querySelectorAll('.overlay-gallery-item');

    function updateOverlayNavigation() {
        overlayPrevBtn.disabled = currentOverlayIndex === 0;
        overlayNextBtn.disabled = currentOverlayIndex === overlayItems.length - 1;
    }

    function scrollToOverlay(index) {
        if (index >= 0 && index < overlayItems.length) {
            currentOverlayIndex = index;
            const targetItem = overlayItems[index];
            targetItem.scrollIntoView({
                behavior: 'smooth',
                block: 'nearest',
                inline: 'start'
            });
            updateOverlayNavigation();
        }
    }

    overlayPrevBtn.addEventListener('click', () => {
        scrollToOverlay(currentOverlayIndex - 1);
    });

    overlayNextBtn.addEventListener('click', () => {
        scrollToOverlay(currentOverlayIndex + 1);
    });

    // Initialize overlay navigation
    updateOverlayNavigation();
});