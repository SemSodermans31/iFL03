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
