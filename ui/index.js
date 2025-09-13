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
        this.installedVersion = null;
        this.latestVersion = null;
        this.latestDownloadUrl = null;
        this.repoOwner = 'SemSodermans31';
        this.repoName = 'iFL03';
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
            // Load local version first to avoid flicker and '-' placeholder
            await this.loadInstalledVersion();
            const state = await this.send('getState');
            this.render(state);
            this.isInitialized = true;
            await this.checkLatestRelease();
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
                try {
                    if (this.latestDownloadUrl) {
                        await this.send('openExternal', { url: this.latestDownloadUrl });
                    }
                } catch (e) { console.error(e); }
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
            const version = (this.installedVersion) || (state && (state.app?.version || state.version || state.appVersion)) || '-';
            if (versionEl) versionEl.textContent = String(version);
            if (updateBtn) updateBtn.disabled = !(this.latestVersion && this.compareSemver(this.latestVersion, version) > 0);
        } catch (e) { console.error('updateVersionInfo failed', e); }
    }

    async loadInstalledVersion() {
        try {
            const res = await fetch('version.json', { cache: 'no-store' });
            if (!res.ok) throw new Error('version.json not found');
            const data = await res.json();
            if (data && data.version) {
                this.installedVersion = String(data.version).replace(/^v/i, '');
                const el = document.getElementById('app-version');
                if (el) el.textContent = this.installedVersion;
            }
        } catch (e) {
            // Silent; fallback to whatever the backend provided or the hardcoded value
        }
    }

    async checkLatestRelease() {
        try {
            const res = await fetch('https://api.github.com/repos/SemSodermans31/iFL03/releases/latest', { headers: { 'Accept': 'application/vnd.github+json' } });
            if (!res.ok) throw new Error('GitHub latest failed');
            const json = await res.json();
            const tag = (json && json.tag_name) ? String(json.tag_name) : '';
            this.latestVersion = tag ? tag.replace(/^v/i, '') : null;
            // Prefer the first asset that looks like our installer
            let dl = null;
            if (Array.isArray(json.assets)) {
                const exe = json.assets.find(a => /setup/i.test(a.name) && /\.exe$/i.test(a.name)) || json.assets[0];
                if (exe && exe.browser_download_url) dl = exe.browser_download_url;
            }
            if (!dl && json.html_url) dl = json.html_url;
            this.latestDownloadUrl = dl;

            const updateBtn = document.getElementById('btn-update');
            if (this.installedVersion && this.latestVersion && this.compareSemver(this.latestVersion, this.installedVersion) > 0) {
                if (updateBtn) updateBtn.disabled = false;
            } else {
                if (updateBtn) updateBtn.disabled = true;
            }
        } catch (e) {
            // Network failure or rate limit; keep button disabled
        }
    }

    compareSemver(a, b) {
        // returns 1 if a>b, 0 if equal, -1 if a<b
        const pa = String(a).split('.').map(n => parseInt(n, 10) || 0);
        const pb = String(b).split('.').map(n => parseInt(n, 10) || 0);
        const len = Math.max(pa.length, pb.length);
        for (let i = 0; i < len; i++) {
            const da = pa[i] || 0, db = pb[i] || 0;
            if (da > db) return 1;
            if (da < db) return -1;
        }
        return 0;
    }

    async openReleaseNotes() {
        const modal = document.getElementById('release-notes-modal');
        const content = document.getElementById('release-notes-content');
        if (modal) modal.classList.remove('hidden');
        if (content) {
            content.textContent = 'Loading...';
            try {
                const tag = this.installedVersion ? `v${this.installedVersion}` : null;
                let body = '';
                if (tag) {
                    // Fetch release by tag
                    const url = `https://api.github.com/repos/${this.repoOwner}/${this.repoName}/releases/tags/${encodeURIComponent(tag)}`;
                    const res = await fetch(url, { headers: { 'Accept': 'application/vnd.github+json' } });
                    if (res.ok) {
                        const json = await res.json();
                        body = (json && typeof json.body === 'string') ? json.body : '';
                    }
                }
                if (!body) {
                    // Fallback to latest
                    const res2 = await fetch(`https://api.github.com/repos/${this.repoOwner}/${this.repoName}/releases/latest`, { headers: { 'Accept': 'application/vnd.github+json' } });
                    if (res2.ok) {
                        const json2 = await res2.json();
                        body = (json2 && typeof json2.body === 'string') ? json2.body : '';
                    }
                }
                const html = this.formatReleaseNotes(body || 'No release notes available.');
                content.innerHTML = html;
            } catch (e) {
                console.error('Failed to load release notes', e);
                content.innerHTML = this.formatReleaseNotes('No release notes available.');
            }
        }
    }

    formatReleaseNotes(text) {
        // Escape HTML
        const escape = (s) => s
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;');
        let t = escape(String(text));
        // Insert double <br> after sentences ending with a period
        t = t.replace(/\.\r?\n/g, '.<br><br>');
        t = t.replace(/\.\s+(?=[A-Z])/g, '.<br><br>');
        if (/\.$/.test(t)) t += '<br><br>';
        // Normalize remaining newlines to single <br>
        t = t.replace(/\r?\n/g, '<br>');
        return t;
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