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

    // Image Carousel functionality
    class ImageCarousel {
        constructor() {
            this.slidesContainer = document.getElementById('carouselSlides');
            this.indicatorsContainer = document.getElementById('carouselIndicators');
            this.slides = document.querySelectorAll('.carousel-slide');
            this.indicators = document.querySelectorAll('.carousel-indicator');
            this.prevBtn = document.querySelector('.carousel-prev');
            this.nextBtn = document.querySelector('.carousel-next');

            this.currentIndex = 0;
            this.totalSlides = this.slides.length;
            this.autoPlayInterval = null;
            this.autoPlayDelay = 4000; // 4 seconds
            this.isTransitioning = false;

            this.init();
        }

        init() {
            if (!this.slidesContainer || !this.indicatorsContainer) return;

            this.setupEventListeners();
            this.updateIndicators();
            this.startAutoPlay();

            // Pause autoplay on hover
            const carouselContainer = document.querySelector('.carousel-container');
            if (carouselContainer) {
                carouselContainer.addEventListener('mouseenter', () => this.stopAutoPlay());
                carouselContainer.addEventListener('mouseleave', () => this.startAutoPlay());
            }
        }

        setupEventListeners() {
            // Previous button
            if (this.prevBtn) {
                this.prevBtn.addEventListener('click', () => this.prevSlide());
            }

            // Next button
            if (this.nextBtn) {
                this.nextBtn.addEventListener('click', () => this.nextSlide());
            }

            // Indicators
            this.indicators.forEach((indicator, index) => {
                indicator.addEventListener('click', () => this.goToSlide(index));
            });

            // Keyboard navigation
            document.addEventListener('keydown', (e) => {
                if (e.key === 'ArrowLeft') {
                    e.preventDefault();
                    this.prevSlide();
                } else if (e.key === 'ArrowRight') {
                    e.preventDefault();
                    this.nextSlide();
                }
            });

            // Touch/swipe support
            let startX = 0;
            let endX = 0;

            this.slidesContainer.addEventListener('touchstart', (e) => {
                startX = e.touches[0].clientX;
                this.stopAutoPlay();
            });

            this.slidesContainer.addEventListener('touchend', (e) => {
                endX = e.changedTouches[0].clientX;
                this.handleSwipe(startX, endX);
                this.startAutoPlay();
            });
        }

        prevSlide() {
            if (this.isTransitioning) return;
            const newIndex = this.currentIndex === 0 ? this.totalSlides - 1 : this.currentIndex - 1;
            this.goToSlide(newIndex);
        }

        nextSlide() {
            if (this.isTransitioning) return;
            const newIndex = (this.currentIndex + 1) % this.totalSlides;
            this.goToSlide(newIndex);
        }

        goToSlide(index) {
            if (this.isTransitioning || index === this.currentIndex) return;

            this.isTransitioning = true;
            this.currentIndex = index;

            // Update slide position
            const translateX = -index * 100;
            this.slidesContainer.style.transform = `translateX(${translateX}%)`;

            // Update indicators
            this.updateIndicators();

            // Reset transition flag after animation completes
            setTimeout(() => {
                this.isTransitioning = false;
            }, 500);
        }

        updateIndicators() {
            this.indicators.forEach((indicator, index) => {
                if (index === this.currentIndex) {
                    indicator.classList.add('active-indicator');
                    indicator.classList.remove('bg-white/30');
                    indicator.classList.add('bg-white/80');
                } else {
                    indicator.classList.remove('active-indicator');
                    indicator.classList.remove('bg-white/80');
                    indicator.classList.add('bg-white/30');
                }
            });
        }

        handleSwipe(startX, endX) {
            const diff = startX - endX;
            const threshold = 50;

            if (Math.abs(diff) > threshold) {
                if (diff > 0) {
                    // Swipe left - next slide
                    this.nextSlide();
                } else {
                    // Swipe right - previous slide
                    this.prevSlide();
                }
            }
        }

        startAutoPlay() {
            this.stopAutoPlay();
            this.autoPlayInterval = setInterval(() => {
                this.nextSlide();
            }, this.autoPlayDelay);
        }

        stopAutoPlay() {
            if (this.autoPlayInterval) {
                clearInterval(this.autoPlayInterval);
                this.autoPlayInterval = null;
            }
        }
    }

    // Initialize carousel
    new ImageCarousel();
});