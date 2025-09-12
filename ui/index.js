/*
MIT License

Copyright (c) 2021-2025 L. E. Spalt & Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/**
 * iFL03 CEF GUI Controller
 * Handles communication between the HTML frontend and C++ backend
 */

class IFL03GuiController {
    constructor() {
        this.isInitialized = false;
        this.setupEventListeners();
    }

    async send(cmd, payload = {}) {
        return new Promise((resolve, reject) => {
            if (!window.cefQuery) {
                reject(new Error('CEF not available'));
                return;
            }
            const request = JSON.stringify({ cmd, ...payload });
            window.cefQuery({
                request,
                onSuccess: (response) => {
                    try {
                        const data = response ? JSON.parse(response) : null;
                        resolve(data);
                    } catch (e) {
                        reject(new Error('Invalid JSON response: ' + e.message));
                    }
                },
                onFailure: (code, msg) => reject(new Error(`CEF Error ${code}: ${msg}`))
            });
        });
    }

    async init() {
        try {
            const state = await this.send('getState');
            this.render(state);
            this.isInitialized = true;
        } catch (e) { console.error('Init failed', e); }
    }

    async setUiEdit(enabled) {
        try { this.render(await this.send('setUiEdit', { on: enabled })); } catch (e) { console.error(e); }
    }

    async setOverlay(key, enabled) {
        try { this.render(await this.send('setOverlay', { key, on: enabled })); } catch (e) { console.error(e); }
    }

    render(state) {
        if (!state) return;
        this.updateConnectionStatus(state.connectionStatus);
        this.updateUiEditToggle(state.uiEdit);
        this.updateOverlayToggles(state.overlays);
        this.updateVersionInfo(state);
    }

    updateConnectionStatus(status) {
        const t = document.getElementById('connText');
        const d = document.getElementById('connDot');
        if (t) t.textContent = status || 'UNKNOWN';
        if (d) {
            d.classList.remove('bg-emerald-500','bg-amber-500','bg-red-500','bg-slate-500');
            d.classList.add(status==='DRIVING'?'bg-emerald-500':status==='CONNECTED'?'bg-amber-500':status==='DISCONNECTED'?'bg-red-500':'bg-slate-500');
        }
    }

    updateUiEditToggle(enabled) {
        const el = document.getElementById('chk_uiEdit');
        if (el) el.checked = !!enabled;
    }

    updateOverlayToggles(overlays) {
        if (!overlays) return;
        ['OverlayStandings','OverlayDDU','OverlayInputs','OverlayRelative','OverlayCover'].forEach(k => {
            const el = document.getElementById('chk_'+k);
            if (el) el.checked = !!overlays[k];
        });
    }

    setupEventListeners() {
        if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', () => this.bindControls());
        else this.bindControls();
        window.onIFL03State = (state) => this.render(state);
    }

    bindControls() {
        const ui = document.getElementById('chk_uiEdit');
        if (ui) ui.addEventListener('change', e => this.setUiEdit(e.target.checked));
        ['OverlayStandings','OverlayDDU','OverlayInputs','OverlayRelative','OverlayCover'].forEach(k => {
            const el = document.getElementById('chk_'+k);
            if (el) el.addEventListener('change', e => this.setOverlay(k, e.target.checked));
        });

        const updateBtn = document.getElementById('btn-update');
        if (updateBtn) {
            updateBtn.addEventListener('click', async () => {
                try { await this.send('updateApp'); } catch (e) { console.error(e); }
            });
        }

        const rnBtn = document.getElementById('btn-release-notes');
        if (rnBtn) rnBtn.addEventListener('click', () => this.openReleaseNotes());
        // Intercept external link clicks and open in default browser via backend
        document.querySelectorAll('a[href^="http://"], a[href^="https://"], a[href^="mailto:"]').forEach(a => {
            a.addEventListener('click', (e) => {
                try {
                    const href = a.getAttribute('href');
                    if (href) {
                        e.preventDefault();
                        this.send('openExternal', { url: href });
                    }
                } catch (err) { console.error(err); }
            });
        });
        const rnClose = document.getElementById('btn-close-release-notes');
        if (rnClose) rnClose.addEventListener('click', () => this.closeReleaseNotes());
        const rnModal = document.getElementById('release-notes-modal');
        const rnBackdrop = document.getElementById('release-notes-backdrop');
        if (rnBackdrop) rnBackdrop.addEventListener('click', () => this.closeReleaseNotes());
    }

    updateVersionInfo(state) {
        try {
            const versionEl = document.getElementById('app-version');
            const updateBtn = document.getElementById('btn-update');
            const version = (state && (state.app?.version || state.version || state.appVersion)) || '-';
            const updateAvailableRaw = state && (state.app?.updateAvailable ?? state.updateAvailable ?? false);
            const updateAvailable = !!updateAvailableRaw;
            if (versionEl) versionEl.textContent = String(version);
            if (updateBtn) updateBtn.disabled = !updateAvailable;
        } catch (e) { console.error('updateVersionInfo failed', e); }
    }

    async openReleaseNotes() {
        const modal = document.getElementById('release-notes-modal');
        const content = document.getElementById('release-notes-content');
        if (modal) modal.classList.remove('hidden');
        if (content) {
            content.textContent = 'Loading...';
            try {
                const notes = await this.send('getReleaseNotes');
                if (!notes) { content.textContent = 'No release notes available.'; return; }
                if (typeof notes === 'string') { content.textContent = notes; return; }
                if (notes.html) { content.innerHTML = notes.html; return; }
                if (notes.text) { content.textContent = notes.text; return; }
                content.textContent = 'No release notes available.';
            } catch (e) {
                console.error('Failed to load release notes', e);
                content.textContent = `Alongside the original overlays, iFL03 adds several new overlays and many quality‑of‑life upgrades
                New overlays
                Delta: Circular delta with trend‑aware coloring and predicted lap time.
                Flags: Clean, high‑contrast flag callouts with two‑band design.
                Weather: Track temp, wetness bar, precipitation/air temp, and a wind compass relative to car.
                Track Map: Scaled track rendering with start/finish markers and cars for most tracks.
                Radar: Proximity radar with 8 m/2 m guides and sticky alerts.

                Enhanced original overlays
                Relative: Optional minimap, license or SR, iRating, pit age, last lap, average of last 5 laps, positions gained, class colors, buddy highlighting, scrollable list, optional iRating prediction in races.
                Standings: Class‑aware grid with fastest‑lap highlighting, car brand icons, deltas/gaps, average of last 5 laps, configurable top/ahead/behind visibility, scroll bar.
                DDU: Fuel calculator refinements, P1 last, delta vs session best, shift light behavior, temperatures, brake bias, incident count, RPM lights, etc.
                Inputs: Dual graph plus vertical bars (clutch/brake/throttle), steering ring or image wheel (Moza KS / RS v2), on‑wheel speed/gear.
                Preview mode: Populate overlays with stub data even when disconnected to place layouts.
                Global opacity: All overlays respect a global opacity for easy blending with broadcasts/streams.`;
            }
        }
    }

    closeReleaseNotes() {
        const modal = document.getElementById('release-notes-modal');
        if (modal) modal.classList.add('hidden');
    }
}

/**
 * Image Carousel for Hero Background
 * Cycles through cover images every 30 seconds
 * All Images are copyright free from Pexels.com
 */
class ImageCarousel {
    constructor() {
        this.coverImages = [
            '../assets/covers/max.jpg',
            '../assets/covers/amg.jpg',
            '../assets/covers/closeup.jpg',
            '../assets/covers/imola.jpg',
            '../assets/covers/rally.jpg',
            '../assets/covers/spray.jpg',
            '../assets/covers/vice.jpg'
        ];
        this.currentImageIndex = 0;
        this.heroBg = null;
        this.intervalId = null;
        this.init();
    }

    init() {
        // Wait for DOM to be ready
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', () => this.setupCarousel());
        } else {
            this.setupCarousel();
        }
    }

    setupCarousel() {
        this.heroBg = document.getElementById('hero-bg');
        if (!this.heroBg) {
            console.warn('Hero background element not found');
            return;
        }

        // Preload all images
        this.preloadImages();

        // Start the carousel
        this.startCarousel();
    }

    preloadImages() {
        this.coverImages.forEach(imageSrc => {
            const img = new Image();
            img.src = imageSrc;
        });
    }

    changeBackgroundImage() {
        // Select a random image different from the current one
        let newIndex;
        do {
            newIndex = Math.floor(Math.random() * this.coverImages.length);
        } while (newIndex === this.currentImageIndex && this.coverImages.length > 1);

        this.currentImageIndex = newIndex;
        this.heroBg.style.backgroundImage = `url('${this.coverImages[this.currentImageIndex]}')`;
    }

    startCarousel() {
        // Start the carousel - change image every 30 seconds (30000 milliseconds)
        this.intervalId = setInterval(() => {
            this.changeBackgroundImage();
        }, 30000);
    }

    stopCarousel() {
        if (this.intervalId) {
            clearInterval(this.intervalId);
            this.intervalId = null;
        }
    }
}

// Initialize the image carousel
const imageCarousel = new ImageCarousel();

const ifl03Gui = new IFL03GuiController();
if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', () => ifl03Gui.init());
else ifl03Gui.init();
window.ifl03Gui = ifl03Gui; 