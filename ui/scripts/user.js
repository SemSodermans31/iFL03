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
	// Config management actions
	const dupBtn = document.getElementById('btn-duplicate-config');
	if (dupBtn) dupBtn.addEventListener('click', showSaveAsModal);
	const delBtn = document.getElementById('deleteConfig');
	if (delBtn) delBtn.addEventListener('click', deleteSelectedConfig);
	const setActiveBtn = document.getElementById('btn-set-active');
	if (setActiveBtn) setActiveBtn.addEventListener('click', setSelectedActive);

	// Modal actions
	document.getElementById('confirmSaveAs').addEventListener('click', confirmSaveAs);
	document.getElementById('cancelSaveAs').addEventListener('click', hideSaveAsModal);

	// Close modals on background click
	document.getElementById('saveAsModal').addEventListener('click', function(e) {
		if (e.target === this) hideSaveAsModal();
	});

	// Buddies/Flagged add buttons
	const buddiesAdd = document.getElementById('buddies-add');
	if (buddiesAdd) buddiesAdd.addEventListener('click', () => addListEntry('General', 'buddies', 'buddies-input'));
	const flaggedAdd = document.getElementById('flagged-add');
	if (flaggedAdd) flaggedAdd.addEventListener('click', () => addListEntry('General', 'flagged', 'flagged-input'));

	// Overlay font settings
	const fontSel = document.getElementById('overlay-font');
	if (fontSel) fontSel.addEventListener('change', () => setConfigString('Overlay', 'font', fontSel.value));
	const fontSize = document.getElementById('overlay-font-size');
	if (fontSize) fontSize.addEventListener('blur', () => setConfigInt('Overlay', 'font_size', parseInt(fontSize.value || '16', 10)));
	const fontSpacing = document.getElementById('overlay-font-spacing');
	if (fontSpacing) fontSpacing.addEventListener('blur', () => setConfigFloat('Overlay', 'font_spacing', parseFloat(fontSpacing.value || '0.30')));
	const fontStyle = document.getElementById('overlay-font-style');
	if (fontStyle) fontStyle.addEventListener('change', () => setConfigString('Overlay', 'font_style', fontStyle.value));
	const fontWeight = document.getElementById('overlay-font-weight');
	if (fontWeight) fontWeight.addEventListener('input', () => updateFontWeightPreview());
	if (fontWeight) fontWeight.addEventListener('change', () => setConfigInt('Overlay', 'font_weight', parseInt(fontWeight.value || '500', 10)));
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
	
	// Update current car information
	updateCurrentCarInfo();
	
	// Update available configs
	updateAvailableConfigs();

	// Update general lists and overlay font settings
	updateGeneralLists();
	updateOverlayFontSettings();
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

function updateOverlayFontSettings() {
    const overlay = currentState.config && currentState.config.Overlay;
    if (!overlay) return;
    const fontSel = document.getElementById('overlay-font');
    if (fontSel && overlay.font) fontSel.value = overlay.font;
    const fontSize = document.getElementById('overlay-font-size');
    if (fontSize && overlay.font_size !== undefined) fontSize.value = overlay.font_size;
    const fontSpacing = document.getElementById('overlay-font-spacing');
    if (fontSpacing && overlay.font_spacing !== undefined) fontSpacing.value = overlay.font_spacing;
    const fontStyle = document.getElementById('overlay-font-style');
    if (fontStyle && overlay.font_style) fontStyle.value = overlay.font_style;
    const fontWeight = document.getElementById('overlay-font-weight');
    if (fontWeight && overlay.font_weight !== undefined) fontWeight.value = overlay.font_weight;
    updateFontWeightPreview();
}

function updateFontWeightPreview() {
    const fontWeight = document.getElementById('overlay-font-weight');
    const label = document.getElementById('overlay-font-weight-value');
    if (fontWeight && label) {
        label.textContent = ` (${fontWeight.value})`;
    }
}

function updateCurrentCarInfo() {
	const currentCarName = document.getElementById('currentCarName');
	const currentCarConfig = document.getElementById('currentCarConfig');
	
	if (currentState.previewMode) {
		if (currentCarName) currentCarName.textContent = 'preview car';
		if (currentCarConfig) currentCarConfig.textContent = 'preview config';
		return;
	}

	// When not in preview, avoid leaking stub car names if not driving/connected
	const carName = currentState.connectionStatus === 'DRIVING' ? (currentState.currentCar || '') : '';
	if (currentCarName) {
		currentCarName.textContent = carName || 'No car detected';
	}

	if (currentCarConfig) {
		currentCarConfig.textContent = currentState.currentCarConfig || 'Default';
	}
}

let selectedConfigName = '';

function updateAvailableConfigs() {
	const container = document.getElementById('availableConfigs');
	if (!container) return;
	
	container.innerHTML = '';
	
	if (!currentState.availableCarConfigs || currentState.availableCarConfigs.length === 0) {
		container.innerHTML = '<div class="col-span-full text-center text-[#a8a8a8] py-8">No car configurations found</div>';
		return;
	}
	
	const active = currentState.currentCarConfig || '';
	currentState.availableCarConfigs.forEach(carName => {
		const card = document.createElement('div');
		const isActive = carName === active;
		const isSelected = carName === selectedConfigName;

		// Base classes
		let className = 'bg-[#1f1f1f] rounded-lg p-4 hover:bg-[#3c3c3c] transition-colors relative';

		// Add green border for selected config
		if (isSelected) {
			className += ' ring-2 ring-green-500/50';
		}

		// Add check icon for active config
		const checkIcon = isActive ? '<i class="fa-solid fa-circle-check absolute top-2 right-2 text-green-500"></i>' : '';

		card.className = className;
		card.innerHTML = `
			${checkIcon}
			<div class="font-medium text-slate-100 mb-1">${escapeHtml(carName)}</div>
			<div class="text-sm text-[#a8a8a8]">Car Configuration</div>
		`;
		card.addEventListener('click', () => selectCarConfig(carName));
		container.appendChild(card);
	});

	const dupBtn = document.getElementById('btn-duplicate-config');
	const delBtn = document.getElementById('deleteConfig');
	const setActiveBtn = document.getElementById('btn-set-active');
	const hasSelection = !!selectedConfigName;
	if (dupBtn) dupBtn.disabled = !hasSelection;
	if (delBtn) delBtn.disabled = !hasSelection;
	if (setActiveBtn) setActiveBtn.disabled = !hasSelection;
}

function selectCarConfig(carName) {
	selectedConfigName = carName;
	updateAvailableConfigs();
}

// API Functions
function deleteSelectedConfig() {
	const name = selectedConfigName;
	if (!name) {
		showStatus('Please select a car configuration', 'error');
		return;
	}

	showConfirmModal({
		title: 'Delete Configuration',
		message: `Are you sure you want to delete "${escapeHtml(name)}"?`,
		confirmText: 'Delete',
		confirmStyle: 'danger'
	}).then(confirmed => {
		if (!confirmed) return;
		sendCommand({ cmd: 'deleteCarConfig', carName: name }, 
			'Configuration deleted successfully', 'Configuration delete failed');
		selectedConfigName = '';
	});
}

// Modal Functions
function showSaveAsModal() {
	const modal = document.getElementById('saveAsModal');
	const input = document.getElementById('saveAsCarName');
	
	if (modal && input) {
		input.value = selectedConfigName ? (selectedConfigName + ' copy') : '';
		modal.classList.remove('hidden');
		input.focus();
	}
}

function hideSaveAsModal() {
	const modal = document.getElementById('saveAsModal');
	if (modal) {
		modal.classList.add('hidden');
		document.getElementById('saveAsCarName').value = '';
	}
}

function confirmSaveAs() {
	const input = document.getElementById('saveAsCarName');
	if (!input || !input.value.trim()) {
		showStatus('Please enter a car name', 'error');
		return;
	}
	const newName = input.value.trim();
	const fromCar = selectedConfigName || '';

	sendCommand({ cmd: 'copyCarConfig', fromCar: fromCar, toCar: newName }, 
		'Configuration saved successfully', 'Configuration save failed');

	hideSaveAsModal();
}

function setSelectedActive() {
	const name = selectedConfigName;
	if (!name) {
		showStatus('Please select a car configuration', 'error');
		return;
	}
	// Load the selected config; state update will reflect the active ring
	sendCommand({ cmd: 'loadCarConfig', carName: name }, 'Configuration loaded', 'Configuration load failed');
}

// Removed copy modal logic; duplicate flow handled via Save As modal

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
