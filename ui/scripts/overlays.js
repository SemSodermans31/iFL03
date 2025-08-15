// Overlays page JavaScript
let currentState = {};
let selectedOverlay = null;

// Overlay configuration mapping
const overlayConfig = {
	'standings': {
		name: 'Standings',
		configKey: 'OverlayStandings',
		description: 'Race position and timing'
	},
	'ddu': {
		name: 'DDU',
		configKey: 'OverlayDDU',
		description: 'Driver display unit'
	},
	'inputs': {
		name: 'Inputs',
		configKey: 'OverlayInputs',
		description: 'Controller inputs'
	},
	'relative': {
		name: 'Relative',
		configKey: 'OverlayRelative',
		description: 'Relative timing'
	},
	'cover': {
		name: 'Cover',
		configKey: 'OverlayCover',
		description: 'Cover overlay'
	},
	'weather': {
		name: 'Weather',
		configKey: 'OverlayWeather',
		description: 'Weather conditions and track information'
	}
};

// Initialize the page
document.addEventListener('DOMContentLoaded', function() {
	console.log('Overlays page loaded');
	
	// Set up event listeners
	setupEventListeners();
	
	// Request initial state
	requestState();
});

function setupEventListeners() {
	// Overlay card clicks
	document.querySelectorAll('.overlay-card').forEach(card => {
		card.addEventListener('click', function() {
			const overlayKey = this.dataset.overlay;
			selectOverlay(overlayKey);
		});
	});

	// Overlay toggle
	const overlayToggle = document.getElementById('overlay-toggle');
	if (overlayToggle) {
		overlayToggle.addEventListener('change', function() {
			if (selectedOverlay) {
				const configKey = overlayConfig[selectedOverlay].configKey;
				sendCommand('setOverlay', { 
					key: configKey, 
					on: this.checked 
				});
			}
		});
	}

	// Hotkey input
	const overlayHotkey = document.getElementById('overlay-hotkey');
	if (overlayHotkey) {
		overlayHotkey.addEventListener('blur', function() {
			if (selectedOverlay) {
				const configKey = overlayConfig[selectedOverlay].configKey;
				sendCommand('setHotkey', { 
					component: configKey,
					key: 'toggle_hotkey',
					value: this.value 
				});
			}
		});
	}

	// Position select
	const overlayPosition = document.getElementById('overlay-position');
	if (overlayPosition) {
		overlayPosition.addEventListener('change', function() {
			if (selectedOverlay) {
				const configKey = overlayConfig[selectedOverlay].configKey;
				sendCommand('setOverlayPosition', { 
					component: configKey,
					position: this.value 
				});
			}
		});
	}

	// Opacity slider
	const overlayOpacity = document.getElementById('overlay-opacity');
	if (overlayOpacity) {
		overlayOpacity.addEventListener('input', function() {
			const value = this.value;
			document.getElementById('opacity-value').textContent = value + '%';
		});

		overlayOpacity.addEventListener('change', function() {
			if (selectedOverlay) {
				const configKey = overlayConfig[selectedOverlay].configKey;
				sendCommand('setOverlayOpacity', { 
					component: configKey,
					opacity: parseInt(this.value) 
				});
			}
		});
	}

	// Save settings button
	const saveButton = document.getElementById('save-overlay-settings');
	if (saveButton) {
		saveButton.addEventListener('click', function() {
			saveOverlaySettings();
		});
	}
}

function selectOverlay(overlayKey) {
	selectedOverlay = overlayKey;
	
	// Update card selection
	document.querySelectorAll('.overlay-card').forEach(card => {
		card.classList.remove('border-indigo-500', 'bg-slate-900/80');
		card.classList.add('border-slate-800', 'bg-slate-900/60');
	});
	
	const selectedCard = document.querySelector(`[data-overlay="${overlayKey}"]`);
	if (selectedCard) {
		selectedCard.classList.remove('border-slate-800', 'bg-slate-900/60');
		selectedCard.classList.add('border-indigo-500', 'bg-slate-900/80');
	}
	
	// Show settings sidebar
	const settingsSidebar = document.getElementById('overlay-settings');
	settingsSidebar.classList.remove('hidden');
	
	// Update sidebar content
	updateOverlaySettings(overlayKey);
}

function updateOverlaySettings(overlayKey) {
	const config = overlayConfig[overlayKey];
	if (!config) return;
	
	// Update title
	document.getElementById('overlay-title').textContent = config.name + ' Settings';
	
	// Update toggle state
	const overlayToggle = document.getElementById('overlay-toggle');
	if (overlayToggle && currentState.config && currentState.config[config.configKey]) {
		overlayToggle.checked = currentState.config[config.configKey].enabled || false;
	}
	
	// Update hotkey
	const overlayHotkey = document.getElementById('overlay-hotkey');
	if (overlayHotkey && currentState.config && currentState.config[config.configKey]) {
		overlayHotkey.value = currentState.config[config.configKey].toggle_hotkey || '';
	}
	
	// Update position
	const overlayPosition = document.getElementById('overlay-position');
	if (overlayPosition && currentState.config && currentState.config[config.configKey]) {
		overlayPosition.value = currentState.config[config.configKey].position || 'top-left';
	}
	
	// Update opacity
	const overlayOpacity = document.getElementById('overlay-opacity');
	if (overlayOpacity && currentState.config && currentState.config[config.configKey]) {
		const opacity = currentState.config[config.configKey].opacity || 100;
		overlayOpacity.value = opacity;
		document.getElementById('opacity-value').textContent = opacity + '%';
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
	
	// Update overlay states in grid
	Object.keys(overlayConfig).forEach(overlayKey => {
		const config = overlayConfig[overlayKey];
		const card = document.querySelector(`[data-overlay="${overlayKey}"]`);
		
		if (card && currentState.config && currentState.config[config.configKey]) {
			const isEnabled = currentState.config[config.configKey].enabled || false;
			
			// Add visual indicator for enabled overlays
			if (isEnabled) {
				card.classList.add('ring-2', 'ring-green-500/50');
			} else {
				card.classList.remove('ring-2', 'ring-green-500/50');
			}
		}
	});
	
	// Update selected overlay settings if one is selected
	if (selectedOverlay) {
		updateOverlaySettings(selectedOverlay);
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

function saveOverlaySettings() {
	if (!selectedOverlay) return;
	
	// Show feedback
	const saveButton = document.getElementById('save-overlay-settings');
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
