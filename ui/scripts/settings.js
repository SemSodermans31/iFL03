// Settings page JavaScript
let currentState = {};

// Initialize the page
document.addEventListener('DOMContentLoaded', function() {
	console.log('Settings page loaded');
	
	// Set up event listeners
	setupEventListeners();
	
	// Request initial state
	requestState();
});

function setupEventListeners() {
	// Performance mode toggle
	const performanceModeCheckbox = document.getElementById('chk_performance_mode');
	if (performanceModeCheckbox) {
		performanceModeCheckbox.addEventListener('change', function() {
			sendCommand('setPerformanceMode', { on: this.checked });
		});
	}

	// Save settings button
	const saveButton = document.getElementById('save_settings');
	if (saveButton) {
		saveButton.addEventListener('click', function() {
			saveAllSettings();
		});
	}

	// Units selector
	const unitsSel = document.getElementById('sel_units');
	if (unitsSel) {
		unitsSel.addEventListener('change', function() {
			sendCommand('setConfigString', { component: 'General', key: 'units', value: this.value });
		});
	}


	// Reset to defaults
	const resetBtn = document.getElementById('btn-reset-defaults');
	if (resetBtn) {
		resetBtn.addEventListener('click', function() {
			showConfirmModal({
				title: 'Reset to Defaults',
				message: 'Are you sure you want to revert all settings to defaults?',
				confirmText: 'Reset',
				confirmStyle: 'danger'
			}).then(confirmed => {
				if (confirmed) {
					sendCommand('resetConfig', {});
				}
			});
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
	
	// Update performance mode
	const performanceModeCheckbox = document.getElementById('chk_performance_mode');
	if (performanceModeCheckbox && currentState.config) {
		performanceModeCheckbox.checked = currentState.config.General?.performance_mode_30hz || false;
	}

	// Update units
	const unitsSel = document.getElementById('sel_units');
	if (unitsSel && currentState.config) {
		unitsSel.value = currentState.config.General?.units || 'metric';
	}

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

function sendCommand(command, data) {
	if (window.cefQuery) {
		const request = {
			cmd: command,
			...data
		};
		
		window.cefQuery({
			request: JSON.stringify(request),
			onSuccess: function(response) {
				try {
					const newState = JSON.parse(response);
					currentState = newState;
					updateUI();
				} catch (e) {
					console.error('Failed to parse response:', e);
				}
			},
			onFailure: function(error_code, error_message) {
				console.error('Command failed:', error_code, error_message);
			}
		});
	}
}

function saveAllSettings() {
	// Collect all current values and save them
	const settings = {};
	
	// Performance mode
	const performanceModeCheckbox = document.getElementById('chk_performance_mode');
	if (performanceModeCheckbox) {
		settings.performance_mode_30hz = performanceModeCheckbox.checked;
	}

	// Units
	const unitsSel = document.getElementById('sel_units');
	if (unitsSel) {
		sendCommand('setConfigString', { component: 'General', key: 'units', value: unitsSel.value });
	}

	
	// Send save command
	sendCommand('saveSettings', settings);
	
	// Show feedback
	const saveButton = document.getElementById('save_settings');
	if (saveButton) {
		const originalText = saveButton.textContent;
		saveButton.textContent = 'Saved!';
		saveButton.disabled = true;
		
		setTimeout(() => {
			saveButton.textContent = originalText;
			saveButton.disabled = false;
		}, 2000);
	}
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
