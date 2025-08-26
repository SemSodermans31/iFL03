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
			if (confirm('Revert all settings to defaults?')) {
				sendCommand('resetConfig', {});
			}
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

// Listen for state updates from the main application
window.onIronState = function(state) {
	currentState = state;
	updateUI();
};
