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
		description: 'Driving inputs'
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
	},
	'flags': {
		name: 'Flags',
		configKey: 'OverlayFlags',
		description: 'Displays active iRacing session flags'
	},
	'delta': {
		name: 'Delta',
		configKey: 'OverlayDelta',
		description: 'Lap time delta comparison to best lap'
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

	// Show in Menu toggle
	const showMenuToggle = document.getElementById('overlay-show-menu');
	if (showMenuToggle) {
		showMenuToggle.addEventListener('change', function() {
			if (selectedOverlay) {
				const configKey = overlayConfig[selectedOverlay].configKey;
				sendCommand('setConfigBool', {
					component: configKey,
					key: 'show_in_menu',
					value: this.checked
				});
			}
		});
	}

	// Show in Race toggle
	const showRaceToggle = document.getElementById('overlay-show-race');
	if (showRaceToggle) {
		showRaceToggle.addEventListener('change', function() {
			if (selectedOverlay) {
				const configKey = overlayConfig[selectedOverlay].configKey;
				sendCommand('setConfigBool', {
					component: configKey,
					key: 'show_in_race',
					value: this.checked
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

	// Flags preview flag change
	const previewFlagSel = document.getElementById('overlay-preview-flag');
	if (previewFlagSel) {
		previewFlagSel.addEventListener('change', function() {
			// Send to backend only for OverlayFlags
			if (selectedOverlay === 'flags') {
				if (window.cefQuery) {
					window.cefQuery({
						request: JSON.stringify({ cmd: 'setPreviewFlag', value: this.value }),
						onSuccess: function(response) {
							try { currentState = JSON.parse(response); updateUI(); } catch(e) { console.error(e); }
						},
						onFailure: function(code, msg) { console.error('setPreviewFlag failed', code, msg); }
					});
				}
			}
		});
	}

	// Weather type change
	const previewWeatherTypeSel = document.getElementById('overlay-preview-weather-type');
	if (previewWeatherTypeSel) {
		previewWeatherTypeSel.addEventListener('change', function() {
			// Send to backend only for OverlayWeather
			if (selectedOverlay === 'weather') {
				if (window.cefQuery) {
					window.cefQuery({
						request: JSON.stringify({ cmd: 'setPreviewWeatherType', value: parseInt(this.value) }),
						onSuccess: function(response) {
							try { currentState = JSON.parse(response); updateUI(); } catch(e) { console.error(e); }
						},
						onFailure: function(code, msg) { console.error('setPreviewWeatherType failed', code, msg); }
					});
				}
			}
		});
	}

	// Delta reference mode change
	const deltaReferenceModeSel = document.getElementById('overlay-delta-reference-mode');
	if (deltaReferenceModeSel) {
		deltaReferenceModeSel.addEventListener('change', function() {
			// Send to backend only for OverlayDelta
			if (selectedOverlay === 'delta') {
				sendCommand('setConfigInt', {
					component: 'OverlayDelta',
					key: 'reference_mode',
					value: parseInt(this.value)
				});
			}
		});
	}

	// Inputs steering wheel change
	const inputsSteeringSel = document.getElementById('overlay-inputs-steering');
	if (inputsSteeringSel) {
		inputsSteeringSel.addEventListener('change', function() {
			if (selectedOverlay === 'inputs') {
				sendCommand('setConfigString', {
					component: 'OverlayInputs',
					key: 'steering_wheel',
					value: this.value
				});
			}
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

	// Update show in menu/race toggles
	const showMenuToggle = document.getElementById('overlay-show-menu');
	const showRaceToggle = document.getElementById('overlay-show-race');
	if (currentState.config && currentState.config[config.configKey]) {
		const cfg = currentState.config[config.configKey];
		if (showMenuToggle) showMenuToggle.checked = cfg.show_in_menu !== false; // Default to true
		if (showRaceToggle) showRaceToggle.checked = cfg.show_in_race !== false; // Default to true
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

	// Flags-specific: show preview flag selector when Flags overlay selected
	const previewFlagRow = document.getElementById('overlay-preview-flag-row');
	if (previewFlagRow) previewFlagRow.classList.add('hidden');
	if (overlayKey === 'flags') {
		if (previewFlagRow) previewFlagRow.classList.remove('hidden');
		const sel = document.getElementById('overlay-preview-flag');
		if (sel) {
			const cfg = currentState.config && currentState.config['OverlayFlags'];
			if (cfg && cfg.preview_flag) sel.value = cfg.preview_flag;
		}
	}

	// Inputs-specific: show steering wheel selector when Inputs overlay selected
	const inputsSteeringRow = document.getElementById('overlay-inputs-steering-row');
	if (inputsSteeringRow) inputsSteeringRow.classList.add('hidden');
	if (overlayKey === 'inputs') {
		if (inputsSteeringRow) inputsSteeringRow.classList.remove('hidden');
		const sel = document.getElementById('overlay-inputs-steering');
		if (sel) {
			const cfg = currentState.config && currentState.config['OverlayInputs'];
			sel.value = (cfg && cfg.steering_wheel) ? cfg.steering_wheel : 'builtin';
		}
	}

	// Weather-specific: show weather type selector when Weather overlay selected
	const previewWeatherRow = document.getElementById('overlay-preview-weather-row');
	if (previewWeatherRow) previewWeatherRow.classList.add('hidden');
	if (overlayKey === 'weather') {
		if (previewWeatherRow) previewWeatherRow.classList.remove('hidden');
		const sel = document.getElementById('overlay-preview-weather-type');
		if (sel) {
			const cfg = currentState.config && currentState.config['OverlayWeather'];
			if (cfg && cfg.preview_weather_type !== undefined) sel.value = cfg.preview_weather_type;
		}
	}

	// Delta-specific: show reference mode selector when Delta overlay selected
	const deltaReferenceRow = document.getElementById('overlay-delta-reference-row');
	if (deltaReferenceRow) deltaReferenceRow.classList.add('hidden');
	if (overlayKey === 'delta') {
		if (deltaReferenceRow) deltaReferenceRow.classList.remove('hidden');
		const sel = document.getElementById('overlay-delta-reference-mode');
		if (sel) {
			const cfg = currentState.config && currentState.config['OverlayDelta'];
			if (cfg && cfg.reference_mode !== undefined) sel.value = cfg.reference_mode;
			else sel.value = '1'; // Default to SESSION_BEST
		}
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
