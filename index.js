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
    let isPlaying = false;
    let autoScrollInterval;

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
        if (slides[slideIndex]) {
            currentSlide = slideIndex;
            slides[slideIndex].scrollIntoView({
                behavior: 'smooth',
                block: 'nearest',
                inline: 'center'
            });
            updateActiveDot();
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
            currentSlide = (currentSlide + 1) % 5; // 5 slides total
            scrollToSlide(currentSlide);
        }, 3000); // Change slide every 3 seconds
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

    // Listen for scroll events to update active dot
    scrollContainer.addEventListener('scroll', () => {
        // Use requestAnimationFrame for better performance
        requestAnimationFrame(updateCurrentSlideFromScroll);
    });

    // Initialize active dot
    updateActiveDot();

    // Ensure initial icon state shows Play only
    playIcon.classList.remove('hidden');
    pauseIcon.classList.add('hidden');

    // Optional: Start with first slide centered on load
    setTimeout(() => {
        scrollToSlide(0);
    }, 100);
});