// Preview Mode page JavaScript
let currentState = {};

// Initialize the page
document.addEventListener('DOMContentLoaded', function() {
	console.log('Preview Mode page loaded');
	
	// Set up event listeners
	setupEventListeners();
	
	// Request initial state
	requestState();
});

function setupEventListeners() {
	// UI Edit toggle
	const uiEditCheckbox = document.getElementById('chk_uiEdit');
	if (uiEditCheckbox) {
		uiEditCheckbox.addEventListener('change', function() {
			sendCommand('setUiEdit', { on: this.checked });
		});
	}

	// Preview Mode toggle
	const previewModeCheckbox = document.getElementById('chk_previewMode');
	if (previewModeCheckbox) {
		previewModeCheckbox.addEventListener('change', function() {
			sendCommand('setPreviewMode', { on: this.checked });
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
	
	// Update UI Edit toggle state
	const uiEditCheckbox = document.getElementById('chk_uiEdit');
	if (uiEditCheckbox && currentState.uiEdit !== undefined) {
		uiEditCheckbox.checked = currentState.uiEdit;
	}

	// Update Preview Mode toggle state
	const previewModeCheckbox = document.getElementById('chk_previewMode');
	if (previewModeCheckbox && currentState.previewMode !== undefined) {
		previewModeCheckbox.checked = currentState.previewMode;
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

// Listen for state updates from the main application
window.onIFL03State = function(state) {
	currentState = state;
	updateUI();
};
