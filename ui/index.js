/**
 * iRon CEF GUI Controller
 * Handles communication between the HTML frontend and C++ backend
 */

class IronGuiController {
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
        window.onIronState = (state) => this.render(state);
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
                content.textContent = 'Failed to load release notes.';
            }
        }
    }

    closeReleaseNotes() {
        const modal = document.getElementById('release-notes-modal');
        if (modal) modal.classList.add('hidden');
    }
}

const ironGui = new IronGuiController();
if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', () => ironGui.init());
else ironGui.init();
window.ironGui = ironGui; 