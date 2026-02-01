// User page JavaScript
let currentState = {};

// Initialize the page
document.addEventListener('DOMContentLoaded', function() {
	console.log('User page loaded');
	
	// Setup event listeners
	setupEventListeners();
	
	// Request initial state
	requestState();
});

function setupEventListeners() {
	// Buddies/Flagged add buttons
	const buddiesAdd = document.getElementById('buddies-add');
	if (buddiesAdd) buddiesAdd.addEventListener('click', () => addListEntry('General', 'buddies', 'buddies-input'));
	const flaggedAdd = document.getElementById('flagged-add');
	if (flaggedAdd) flaggedAdd.addEventListener('click', () => addListEntry('General', 'flagged', 'flagged-input'));

	// UI preferences
	const overlaysHelpToggle = document.getElementById('toggle-overlays-help');
	if (overlaysHelpToggle) {
		overlaysHelpToggle.addEventListener('change', function() {
			sendCommand(
				{ cmd: 'setConfigBool', component: 'General', key: 'show_overlays_help', value: this.checked },
				'Setting saved',
				'Failed to save setting'
			);
		});
	}
}

function requestState() {
	if (window.cefQuery) {
		window.cefQuery({
			request: JSON.stringify({ cmd: 'getState' }),
			onSuccess: function(response) {
				try {
					currentState = JSON.parse(response);
					updateUI();
				} catch (e) {
					console.error('Failed to parse state:', e);
				}
			},
			onFailure: function(error_code, error_message) {
				console.error('Failed to get state:', error_code, error_message);
			}
		});
	}
}

function updateUI() {
	// Update connection status
	updateConnectionStatus(currentState.connectionStatus);

	// Update general lists
	updateGeneralLists();

	// Update Ghost Telemetry UI
	updateGhostTelemetryUI();

	// Update UI preferences
	updateUiPreferences();
}

function updateUiPreferences() {
	const overlaysHelpToggle = document.getElementById('toggle-overlays-help');
	if (!overlaysHelpToggle) return;
	// Default to true if key is missing
	const show = currentState?.config?.General?.show_overlays_help !== false;
	overlaysHelpToggle.checked = !!show;
}

function updateConnectionStatus(status) {
	const connDot = document.getElementById('connDot');
	const connText = document.getElementById('connText');
	
	if (connDot && connText) {
		switch (status) {
			case 'DRIVING':
				connDot.className = 'size-2 rounded-full bg-green-500';
				connText.textContent = 'DRIVING';
				break;
			case 'CONNECTED':
				connDot.className = 'size-2 rounded-full bg-yellow-500';
				connText.textContent = 'CONNECTED';
				break;
			case 'DISCONNECTED':
				connDot.className = 'size-2 rounded-full bg-red-500';
				connText.textContent = 'DISCONNECTED';
				break;
			default:
				connDot.className = 'size-2 rounded-full bg-slate-500';
				connText.textContent = 'UNKNOWN';
		}
	}
}

function updateGeneralLists() {
    const rawBuddies = (currentState.config && currentState.config.General && Array.isArray(currentState.config.General.buddies)) ? currentState.config.General.buddies : [];
    const rawFlagged = (currentState.config && currentState.config.General && Array.isArray(currentState.config.General.flagged)) ? currentState.config.General.flagged : [];

    const sanitize = (arr) => arr.filter(s => typeof s === 'string' && s.trim().length > 0).map(s => s.trim());
    const buddies = sanitize(rawBuddies);
    const flagged = sanitize(rawFlagged);

    // Auto-clean persisted empties from older versions
    if (buddies.length !== rawBuddies.length) setConfigStringVec('General', 'buddies', buddies);
    if (flagged.length !== rawFlagged.length) setConfigStringVec('General', 'flagged', flagged);

    renderStringList('buddies-list', buddies, (idx) => removeListEntry('General', 'buddies', idx));
    renderStringList('flagged-list', flagged, (idx) => removeListEntry('General', 'flagged', idx));
}

function renderStringList(containerId, items, onRemove) {
    const container = document.getElementById(containerId);
    if (!container) return;
    container.innerHTML = '';
    if (!items || items.length === 0) {
        const empty = document.createElement('div');
        empty.className = 'text-xs text-[#a8a8a8]';
        empty.textContent = 'No entries';
        container.appendChild(empty);
        return;
    }
    items.forEach((name, idx) => {
        const row = document.createElement('div');
        row.className = 'flex items-center justify-between rounded-lg bg-[#1f1f1f] px-3 py-2';
        const span = document.createElement('span');
        span.className = 'text-slate-200';
        span.textContent = name;
        const btn = document.createElement('button');
        btn.className = 'px-2 py-1 text-xs bg-red-600 hover:bg-red-700 text-white rounded-md';
        btn.textContent = 'Remove';
        btn.addEventListener('click', () => onRemove(idx));
        row.appendChild(span);
        row.appendChild(btn);
        container.appendChild(row);
    });
}

function updateGhostTelemetryUI() {
    const sel = document.getElementById('ghostTelemetrySelect');
    const toggle = document.getElementById('ghostShowToggle');
    if (!sel || !toggle) return;

    // Populate files list from state
    const files = (currentState.ghostTelemetry && Array.isArray(currentState.ghostTelemetry.files)) ? currentState.ghostTelemetry.files : [];
    const selected = (currentState.ghostTelemetry && typeof currentState.ghostTelemetry.selected === 'string') ? currentState.ghostTelemetry.selected : '';

    sel.innerHTML = '';
    const optNone = document.createElement('option');
    optNone.value = '';
    optNone.textContent = files.length ? 'None' : 'No CSV files found';
    sel.appendChild(optNone);
    files.forEach(f => {
        const opt = document.createElement('option');
        opt.value = f;
        opt.textContent = f;
        sel.appendChild(opt);
    });
    sel.value = selected || '';

    // Reflect show toggle from Inputs config
    const showGhost = currentState.config && currentState.config.OverlayInputs && !!currentState.config.OverlayInputs.show_ghost_data;
    toggle.checked = showGhost;

    // Wire listeners once
    if (!sel._ifl03Bound) {
        sel.addEventListener('change', function() {
            // Persist selection in General.ghost_telemetry_file
            setConfigString('General', 'ghost_telemetry_file', this.value || '');
        });
        sel._ifl03Bound = true;
    }

    if (!toggle._ifl03Bound) {
        toggle.addEventListener('change', function() {
            // Toggle OverlayInputs.show_ghost_data
            sendCommand({ cmd: 'setConfigBool', component: 'OverlayInputs', key: 'show_ghost_data', value: this.checked }, 'Setting saved', 'Failed to save setting');
        });
        toggle._ifl03Bound = true;
    }
}

function addListEntry(component, key, inputId) {
    const input = document.getElementById(inputId);
    const value = (input && input.value) ? input.value.trim() : '';
    if (!value) return;
    const list = (currentState.config && currentState.config[component] && Array.isArray(currentState.config[component][key])) ? currentState.config[component][key] : [];
    const newList = list.filter(s => typeof s === 'string' && s.trim().length > 0).map(s => s.trim());
    newList.push(value);
    setConfigStringVec(component, key, newList);
    input.value = '';
}

function removeListEntry(component, key, index) {
    const list = (currentState.config && currentState.config[component] && Array.isArray(currentState.config[component][key])) ? currentState.config[component][key] : [];
    if (index < 0 || index >= list.length) return;
    const newList = list.slice();
    newList.splice(index, 1);
    setConfigStringVec(component, key, newList);
}

function setConfigString(component, key, value) {
    sendCommand({ cmd: 'setConfigString', component, key, value }, 'Setting saved', 'Failed to save setting');
}

function setConfigInt(component, key, value) {
    if (!Number.isFinite(value)) return;
    sendCommand({ cmd: 'setConfigInt', component, key, value }, 'Setting saved', 'Failed to save setting');
}

function setConfigFloat(component, key, value) {
    if (!Number.isFinite(value)) return;
    sendCommand({ cmd: 'setConfigFloat', component, key, value }, 'Setting saved', 'Failed to save setting');
}

function setConfigStringVec(component, key, values) {
    sendCommand({ cmd: 'setConfigStringVec', component, key, values }, 'List updated', 'Failed to update list');
}

// Utility Functions
function sendCommand(command, successMessage, errorMessage) {
	if (window.cefQuery) {
		window.cefQuery({
			request: JSON.stringify(command),
			onSuccess: function(response) {
				try {
					currentState = JSON.parse(response);
					updateUI();
					if (successMessage) showStatus(successMessage, 'success');
				} catch (e) {
					console.error('Failed to parse response:', e);
					if (errorMessage) showStatus(errorMessage, 'error');
				}
			},
			onFailure: function(error_code, error_message) {
				console.error('Command failed:', error_code, error_message);
				if (errorMessage) showStatus(errorMessage, 'error');
			}
		});
	}
}

function showStatus(message, type = 'info') {
	const statusDiv = document.getElementById('statusMessage');
	const statusText = document.getElementById('statusText');
	
	if (!statusDiv || !statusText) return;
	
	statusText.textContent = message;
	
	// Remove existing type classes
	statusDiv.classList.remove('bg-green-100', 'bg-red-100', 'bg-blue-100', 
						   'border-green-500', 'border-red-500', 'border-blue-500',
						   'text-green-800', 'text-red-800', 'text-blue-800');
	
	// Add type-specific classes
	switch (type) {
		case 'success':
			statusDiv.classList.add('bg-green-100', 'border-green-500', 'text-green-800', 'border');
			break;
		case 'error':
			statusDiv.classList.add('bg-red-100', 'border-red-500', 'text-red-800', 'border');
			break;
		default:
			statusDiv.classList.add('bg-blue-100', 'border-blue-500', 'text-blue-800', 'border');
			break;
	}
	
	statusDiv.classList.remove('hidden');
	
	// Auto-hide after 3 seconds
	setTimeout(() => {
		statusDiv.classList.add('hidden');
	}, 3000);
}

function escapeHtml(text) {
	const div = document.createElement('div');
	div.textContent = text;
	return div.innerHTML;
}

// Reusable styled confirm modal
function showConfirmModal(opts) {
	return new Promise(resolve => {
		const modal = document.getElementById('confirmModal');
		const title = document.getElementById('confirmTitle');
		const message = document.getElementById('confirmMessage');
		const yes = document.getElementById('confirmYes');
		const no = document.getElementById('confirmNo');

		if (!modal || !title || !message || !yes || !no) {
			const fallback = confirm(opts?.message || 'Are you sure?');
			resolve(fallback);
			return;
		}

		title.textContent = opts?.title || 'Confirm Action';
		message.innerHTML = opts?.message || 'Are you sure?';
		yes.textContent = opts?.confirmText || 'Confirm';
		// Style confirm button
		yes.className = 'flex-1 px-4 py-2 text-white text-sm font-medium rounded-lg transition-colors ' +
			(opts?.confirmStyle === 'danger' ? 'bg-red-600 hover:bg-red-700' : 'bg-blue-600 hover:bg-blue-700');

		const cleanup = (result) => {
			modal.classList.add('hidden');
			yes.removeEventListener('click', onYes);
			no.removeEventListener('click', onNo);
			modal.removeEventListener('click', onBackdrop);
			resolve(result);
		};

		const onYes = () => cleanup(true);
		const onNo = () => cleanup(false);
		const onBackdrop = (e) => { if (e.target === modal) cleanup(false); };

		yes.addEventListener('click', onYes);
		no.addEventListener('click', onNo);
		modal.addEventListener('click', onBackdrop);
		modal.classList.remove('hidden');
	});
}

// Listen for state updates from the main application
window.onIFL03State = function(state) {
	currentState = state;
	updateUI();
};
