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
	// Current car actions
	document.getElementById('saveCurrentCarConfig').addEventListener('click', saveCurrentCarConfig);
	document.getElementById('loadCurrentCarConfig').addEventListener('click', loadCurrentCarConfig);
	
	// Config management actions
	document.getElementById('carSelect').addEventListener('change', onCarSelectChange);
	document.getElementById('loadSelectedConfig').addEventListener('click', loadSelectedConfig);
	document.getElementById('saveAsNewConfig').addEventListener('click', showSaveAsModal);
	document.getElementById('copyConfig').addEventListener('click', showCopyModal);
	document.getElementById('deleteConfig').addEventListener('click', deleteSelectedConfig);
	
	// Modal actions
	document.getElementById('confirmSaveAs').addEventListener('click', confirmSaveAs);
	document.getElementById('cancelSaveAs').addEventListener('click', hideSaveAsModal);
	document.getElementById('confirmCopy').addEventListener('click', confirmCopy);
	document.getElementById('cancelCopy').addEventListener('click', hideCopyModal);
	
	// Close modals on background click
	document.getElementById('saveAsModal').addEventListener('click', function(e) {
		if (e.target === this) hideSaveAsModal();
	});
	document.getElementById('copyModal').addEventListener('click', function(e) {
		if (e.target === this) hideCopyModal();
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
	
	// Update car selection dropdown
	updateCarSelect();
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
	const saveBtn = document.getElementById('saveCurrentCarConfig');
	const loadBtn = document.getElementById('loadCurrentCarConfig');
	
	if (currentCarName) {
		currentCarName.textContent = currentState.currentCar || 'No car detected';
	}
	
	if (currentCarConfig) {
		currentCarConfig.textContent = currentState.currentCarConfig || 'Default';
	}
	
	// Enable/disable buttons based on car availability
	const hasCurrentCar = currentState.currentCar && currentState.currentCar.trim() !== '';
	if (saveBtn) saveBtn.disabled = !hasCurrentCar;
	if (loadBtn) loadBtn.disabled = !hasCurrentCar;
}

function updateAvailableConfigs() {
	const container = document.getElementById('availableConfigs');
	if (!container) return;
	
	container.innerHTML = '';
	
	if (!currentState.availableCarConfigs || currentState.availableCarConfigs.length === 0) {
		container.innerHTML = '<div class="col-span-full text-center text-slate-400 py-8">No car configurations found</div>';
		return;
	}
	
	currentState.availableCarConfigs.forEach(carName => {
		const card = document.createElement('div');
		card.className = 'bg-slate-800 rounded-lg p-4 border border-slate-700 hover:border-slate-600 transition-colors';
		card.innerHTML = `
			<div class="font-medium text-slate-100 mb-1">${escapeHtml(carName)}</div>
			<div class="text-sm text-slate-400">Car Configuration</div>
		`;
		card.addEventListener('click', () => selectCarConfig(carName));
		container.appendChild(card);
	});
}

function updateCarSelect() {
	const select = document.getElementById('carSelect');
	const copyFromSelect = document.getElementById('copyFromCar');
	
	if (!select) return;
	
	// Clear existing options except the first one
	select.innerHTML = '<option value="">Select a car...</option>';
	
	if (copyFromSelect) {
		copyFromSelect.innerHTML = '<option value="">Default Config</option>';
	}
	
	if (currentState.availableCarConfigs) {
		currentState.availableCarConfigs.forEach(carName => {
			const option = document.createElement('option');
			option.value = carName;
			option.textContent = carName;
			select.appendChild(option);
			
			if (copyFromSelect) {
				const copyOption = document.createElement('option');
				copyOption.value = carName;
				copyOption.textContent = carName;
				copyFromSelect.appendChild(copyOption);
			}
		});
	}
}

function selectCarConfig(carName) {
	const select = document.getElementById('carSelect');
	if (select) {
		select.value = carName;
		onCarSelectChange();
	}
}

function onCarSelectChange() {
	const select = document.getElementById('carSelect');
	const loadBtn = document.getElementById('loadSelectedConfig');
	const copyBtn = document.getElementById('copyConfig');
	const deleteBtn = document.getElementById('deleteConfig');
	
	const hasSelection = select && select.value.trim() !== '';
	
	if (loadBtn) loadBtn.disabled = !hasSelection;
	if (copyBtn) copyBtn.disabled = !hasSelection;
	if (deleteBtn) deleteBtn.disabled = !hasSelection;
}

// API Functions
function saveCurrentCarConfig() {
	if (!currentState.currentCar) {
		showStatus('No current car detected', 'error');
		return;
	}
	
	sendCommand({ cmd: 'saveCarConfig', carName: currentState.currentCar }, 
		'Configuration saved successfully', 'Configuration save failed');
}

function loadCurrentCarConfig() {
	if (!currentState.currentCar) {
		showStatus('No current car detected', 'error');
		return;
	}
	
	sendCommand({ cmd: 'loadCarConfig', carName: currentState.currentCar }, 
		'Configuration loaded successfully', 'Configuration load failed');
}

function loadSelectedConfig() {
	const select = document.getElementById('carSelect');
	if (!select || !select.value) {
		showStatus('Please select a car configuration', 'error');
		return;
	}
	
	sendCommand({ cmd: 'loadCarConfig', carName: select.value }, 
		'Configuration loaded successfully', 'Configuration load failed');
}

function deleteSelectedConfig() {
	const select = document.getElementById('carSelect');
	if (!select || !select.value) {
		showStatus('Please select a car configuration', 'error');
		return;
	}
	
	if (!confirm(`Are you sure you want to delete the configuration for "${select.value}"?`)) {
		return;
	}
	
	sendCommand({ cmd: 'deleteCarConfig', carName: select.value }, 
		'Configuration deleted successfully', 'Configuration delete failed');
}

// Modal Functions
function showSaveAsModal() {
	const modal = document.getElementById('saveAsModal');
	const input = document.getElementById('saveAsCarName');
	
	if (modal && input) {
		// Pre-fill with current car name if available
		if (currentState.currentCar) {
			input.value = currentState.currentCar;
		}
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
	
	sendCommand({ cmd: 'saveCarConfig', carName: input.value.trim() }, 
		'Configuration saved successfully', 'Configuration save failed');
	
	hideSaveAsModal();
}

function showCopyModal() {
	const modal = document.getElementById('copyModal');
	if (modal) {
		modal.classList.remove('hidden');
		document.getElementById('copyToCar').focus();
	}
}

function hideCopyModal() {
	const modal = document.getElementById('copyModal');
	if (modal) {
		modal.classList.add('hidden');
		document.getElementById('copyFromCar').value = '';
		document.getElementById('copyToCar').value = '';
	}
}

function confirmCopy() {
	const fromSelect = document.getElementById('copyFromCar');
	const toInput = document.getElementById('copyToCar');
	
	if (!toInput || !toInput.value.trim()) {
		showStatus('Please enter a target car name', 'error');
		return;
	}
	
	const fromCar = fromSelect ? fromSelect.value : '';
	const toCar = toInput.value.trim();
	
	sendCommand({ cmd: 'copyCarConfig', fromCar: fromCar, toCar: toCar }, 
		'Configuration copied successfully', 'Configuration copy failed');
	
	hideCopyModal();
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
					showStatus(successMessage, 'success');
				} catch (e) {
					console.error('Failed to parse response:', e);
					showStatus(errorMessage, 'error');
				}
			},
			onFailure: function(error_code, error_message) {
				console.error('Command failed:', error_code, error_message);
				showStatus(errorMessage, 'error');
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

// Listen for state updates from the main application
window.onIronState = function(state) {
	currentState = state;
	updateUI();
};
