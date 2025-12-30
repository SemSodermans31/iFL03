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
	'fuel': {
		name: 'Fuel',
		configKey: 'OverlayFuel',
		description: 'Fuel calculator'
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
	},
	'tire': {
		name: 'Tire',
		configKey: 'OverlayTire',
		description: 'Tire health, temperature, pressure and laps'
	},
	'radar': {
		name: 'Radar',
		configKey: 'OverlayRadar',
		description: 'Proximity radar for nearby cars'
	},
	'track': {
		name: 'Track Map',
		configKey: 'OverlayTrack',
		description: 'Track map with live car position'
	},
	'pit': {
		name: 'Pit Entry',
		configKey: 'OverlayPit',
		description: 'Distance to pitlane entry'
	},
	'traffic': {
		name: 'Traffic',
		configKey: 'OverlayTraffic',
		description: 'Faster-class warning popup (multiclass)'
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

	// Enabled toggle
	const overlayToggle = document.getElementById('overlay-toggle');
	if (overlayToggle) {
		overlayToggle.addEventListener('change', function() {
			if (selectedOverlay) {
				const configKey = overlayConfig[selectedOverlay].configKey;
				sendCommand('setOverlay', {
					key: configKey,
					on: this.checked
				});

				// Update border immediately for visual feedback
				const card = document.querySelector(`[data-overlay="${selectedOverlay}"]`);
				const iconContainer = card ? card.querySelector('.flex.items-center.justify-center') : null;
				if (iconContainer) {
					if (this.checked) {
						iconContainer.classList.add('border-2', 'border-green-500');
					} else {
						iconContainer.classList.remove('border-2', 'border-green-500');
					}
				}
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

	// Target FPS slider
	const targetFpsSlider = document.getElementById('overlay-target-fps');
	if (targetFpsSlider) {
		targetFpsSlider.addEventListener('input', function() {
			const value = parseInt(this.value);
			document.getElementById('target-fps-value').textContent = value;
		});

		targetFpsSlider.addEventListener('change', function() {
			if (selectedOverlay) {
				const configKey = overlayConfig[selectedOverlay].configKey;
				sendCommand('setConfigInt', {
					component: configKey,
					key: 'target_fps',
					value: parseInt(this.value)
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

	// D-pad movement controls with tap/hold functionality

	function setupDpadButton(buttonId, direction) {
		const button = document.getElementById(buttonId);
		if (!button) return;

		let isHolding = false;
		let holdInterval = null;
		let tapTimeout = null;

		function moveOverlay(deltaX, deltaY) {
			if (selectedOverlay) {
				const configKey = overlayConfig[selectedOverlay].configKey;
				sendCommand('moveOverlayDelta', {
					component: configKey,
					deltaX: deltaX,
					deltaY: deltaY
				});
			}
		}

		function startHold() {
			isHolding = true;
			// Move 100px every 100ms while holding
			holdInterval = setInterval(() => {
				const deltaX = direction === 'left' ? -100 : direction === 'right' ? 100 : 0;
				const deltaY = direction === 'up' ? -100 : direction === 'down' ? 100 : 0;
				moveOverlay(deltaX, deltaY);
			}, 100);
		}

		function stopHold() {
			if (holdInterval) {
				clearInterval(holdInterval);
				holdInterval = null;
			}
			isHolding = false;
		}

		button.addEventListener('mousedown', function(e) {
			e.preventDefault();
			if (!selectedOverlay) return;

			// Clear any existing timers
			if (tapTimeout) clearTimeout(tapTimeout);
			if (holdInterval) clearInterval(holdInterval);

			// Start tap detection timeout
			tapTimeout = setTimeout(() => {
				// If still pressed after 300ms, start hold mode
				startHold();
			}, 300);
		});

		button.addEventListener('mouseup', function(e) {
			e.preventDefault();
			if (!selectedOverlay) return;

			if (tapTimeout) {
				clearTimeout(tapTimeout);
				tapTimeout = null;

				// If not holding, it was a tap - move 10px
				if (!isHolding) {
					const deltaX = direction === 'left' ? -10 : direction === 'right' ? 10 : 0;
					const deltaY = direction === 'up' ? -10 : direction === 'down' ? 10 : 0;
					moveOverlay(deltaX, deltaY);
				}
			}

			stopHold();
		});

		button.addEventListener('mouseleave', function() {
			// Stop movement if mouse leaves button while pressed
			if (tapTimeout) {
				clearTimeout(tapTimeout);
				tapTimeout = null;
			}
			stopHold();
		});
	}

	setupDpadButton('dpad-up', 'up');
	setupDpadButton('dpad-down', 'down');
	setupDpadButton('dpad-left', 'left');
	setupDpadButton('dpad-right', 'right');

	const dpadCenter = document.getElementById('dpad-center');
	if (dpadCenter) {
		dpadCenter.addEventListener('click', function() {
			if (selectedOverlay) {
				const configKey = overlayConfig[selectedOverlay].configKey;
				sendCommand('centerOverlay', {
					component: configKey
				});
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


	// Tire: Temperature thresholds
	const tireTempCoolInput = document.getElementById('overlay-tire-temp-cool');
	if (tireTempCoolInput) {
		tireTempCoolInput.addEventListener('input', function() {
			if (selectedOverlay === 'tire') {
				sendCommand('setConfigFloat', {
					component: 'OverlayTire',
					key: 'temp_cool_c',
					value: parseFloat(this.value)
				});
			}
		});
	}

	const tireTempOptInput = document.getElementById('overlay-tire-temp-opt');
	if (tireTempOptInput) {
		tireTempOptInput.addEventListener('input', function() {
			if (selectedOverlay === 'tire') {
				sendCommand('setConfigFloat', {
					component: 'OverlayTire',
					key: 'temp_opt_c',
					value: parseFloat(this.value)
				});
			}
		});
	}

	const tireTempHotInput = document.getElementById('overlay-tire-temp-hot');
	if (tireTempHotInput) {
		tireTempHotInput.addEventListener('input', function() {
			if (selectedOverlay === 'tire') {
				sendCommand('setConfigFloat', {
					component: 'OverlayTire',
					key: 'temp_hot_c',
					value: parseFloat(this.value)
				});
			}
		});
	}

	// Track: Track Width slider
	const trackWidthSlider = document.getElementById('track-width-slider');
	if (trackWidthSlider) {
		trackWidthSlider.addEventListener('input', function() {
			const value = parseFloat(this.value);
			document.getElementById('track-width-value').textContent = value.toFixed(1);
		});

		trackWidthSlider.addEventListener('change', function() {
			if (selectedOverlay === 'track') {
				sendCommand('setConfigFloat', {
					component: 'OverlayTrack',
					key: 'track_width',
					value: parseFloat(this.value)
				});
			}
		});
	}

	// Generic Typography listeners
	const fontSel = document.getElementById('overlay-font');
	if (fontSel) fontSel.addEventListener('change', function() {
		if (!selectedOverlay) return;
		const component = overlayConfig[selectedOverlay].configKey;
		sendCommand('setConfigString', { component, key: 'font', value: this.value });
	});
	const fontSize = document.getElementById('overlay-font-size');
	if (fontSize) fontSize.addEventListener('input', function() {
		if (!selectedOverlay) return;
		const component = overlayConfig[selectedOverlay].configKey;
		sendCommand('setConfigFloat', { component, key: 'font_size', value: parseFloat(this.value) });
	});
	const fontSpacing = document.getElementById('overlay-font-spacing');
	if (fontSpacing) fontSpacing.addEventListener('input', function() {
		if (!selectedOverlay) return;
		const component = overlayConfig[selectedOverlay].configKey;
		sendCommand('setConfigFloat', { component, key: 'font_spacing', value: parseFloat(this.value) });
	});
	const fontStyle = document.getElementById('overlay-font-style');
	if (fontStyle) fontStyle.addEventListener('change', function() {
		if (!selectedOverlay) return;
		const component = overlayConfig[selectedOverlay].configKey;
		sendCommand('setConfigString', { component, key: 'font_style', value: this.value });
	});
	const fontWeight = document.getElementById('overlay-font-weight');
	if (fontWeight) {
		const label = document.getElementById('overlay-font-weight-value');
		fontWeight.addEventListener('input', function() {
			if (label) label.textContent = ` (${this.value})`;
			if (!selectedOverlay) return;
			const component = overlayConfig[selectedOverlay].configKey;
			sendCommand('setConfigInt', { component, key: 'font_weight', value: parseInt(this.value) });
		});
	}
}

function selectOverlay(overlayKey) {
	selectedOverlay = overlayKey;
	
	// Show settings sidebar
	const settingsSidebar = document.getElementById('overlay-settings');
	settingsSidebar.classList.remove('hidden');
	
	// Update sidebar content
	updateOverlaySettings(overlayKey);
}

function toTitleCase(str) {
	return str.replace(/_/g, ' ').replace(/\b\w/g, c => c.toUpperCase());
}

function renderBooleanToggles(configKey) {
	const container = document.getElementById('overlay-bool-list');
	if (!container) return;
	container.innerHTML = '';

	const cfg = currentState.config && currentState.config[configKey];
	if (!cfg) return;

	// Collect boolean keys
	const exclude = new Set(['enabled', 'show_in_menu', 'show_in_race']);
	const keys = Object.keys(cfg).filter(k => typeof cfg[k] === 'boolean' && !exclude.has(k));
	if (keys.length === 0) return;

	// Order: enabled, show_in_menu, show_in_race, then others alpha
	const priority = { enabled: 0, show_in_menu: 1, show_in_race: 2, show_tire_compound: 3, show_steering_wheel: 4 };
	keys.sort((a, b) => (priority[a] ?? 99) - (priority[b] ?? 99) || a.localeCompare(b));

	keys.forEach(key => {
		const id = `bool-${key}`;
		const checked = !!cfg[key];
		const labelText = toTitleCase(key);

		const wrapper = document.createElement('label');
		wrapper.className = 'flex items-center justify-between gap-4 rounded-lg bg-[#1f1f1f] px-3 py-2';

		const spanText = document.createElement('span');
		spanText.className = 'text-slate-200';
		spanText.textContent = labelText;

		const switchWrap = document.createElement('span');
		switchWrap.className = 'relative inline-flex h-6 w-11 items-center';

		const input = document.createElement('input');
		input.type = 'checkbox';
		input.id = id;
		input.className = 'peer sr-only';
		input.checked = checked;
		input.dataset.key = key;

		const bg = document.createElement('span');
		bg.className = 'block h-6 w-11 rounded-full bg-slate-600 transition-colors duration-200 ease-in-out peer-checked:bg-green-600';

		const knob = document.createElement('span');
		knob.className = 'pointer-events-none absolute left-0 top-0.5 ml-0.5 h-5 w-5 rounded-full bg-white shadow translate-x-0 transition-transform duration-200 ease-in-out peer-checked:translate-x-[1.25rem]';

		switchWrap.appendChild(input);
		switchWrap.appendChild(bg);
		switchWrap.appendChild(knob);

		wrapper.appendChild(spanText);
		wrapper.appendChild(switchWrap);

		container.appendChild(wrapper);

		input.addEventListener('change', function() {
			const k = this.dataset.key;
			if (!selectedOverlay) return;
			const component = overlayConfig[selectedOverlay].configKey;
			if (k === 'enabled') {
				sendCommand('setOverlay', { key: component, on: this.checked });
				// Visual feedback on card
				const card = document.querySelector(`[data-overlay="${selectedOverlay}"]`);
				const iconContainer = card ? card.querySelector('.flex.items-center.justify-center') : null;
				if (iconContainer) {
					if (this.checked) iconContainer.classList.add('border-2', 'border-green-500');
					else iconContainer.classList.remove('border-2', 'border-green-500');
				}
			} else {
				sendCommand('setConfigBool', { component, key: k, value: this.checked });
			}
		});
	});
}

function updateOverlaySettings(overlayKey) {
	const config = overlayConfig[overlayKey];
	if (!config) return;
	
	// Update title
	document.getElementById('overlay-title').textContent = config.name + ' Settings';

	// Update general boolean toggles description
	const overlayGeneralBoolSectionTitle = document.getElementById('overlay-general-bool-section-title');
	if (overlayGeneralBoolSectionTitle) {
		overlayGeneralBoolSectionTitle.textContent = config.name + ' Options';
	}

	const overlayGeneralBoolListDescription = document.getElementById('overlay-bool-list-description');
	if (overlayGeneralBoolListDescription) {
		overlayGeneralBoolListDescription.textContent = 'All options for the ' + config.name + ' configuration.';
	}

	// Render general boolean toggles
	renderBooleanToggles(config.configKey);

	// Sync display toggles states
	const cfg = currentState.config && currentState.config[config.configKey];
	if (cfg) {
		const enabled = document.getElementById('overlay-toggle');
		if (enabled) enabled.checked = !!cfg.enabled;
		const showMenu = document.getElementById('overlay-show-menu');
		if (showMenu) showMenu.checked = cfg.show_in_menu !== false; // default true
		const showRace = document.getElementById('overlay-show-race');
		if (showRace) showRace.checked = cfg.show_in_race !== false; // default true
	}
	
	// Update hotkey
	const overlayHotkey = document.getElementById('overlay-hotkey');
	if (overlayHotkey && currentState.config && currentState.config[config.configKey]) {
		overlayHotkey.value = currentState.config[config.configKey].toggle_hotkey || '';
	}
	
	
	// Update opacity
	const overlayOpacity = document.getElementById('overlay-opacity');
	if (overlayOpacity && currentState.config && currentState.config[config.configKey]) {
		const opacity = currentState.config[config.configKey].opacity || 100;
		overlayOpacity.value = opacity;
		document.getElementById('opacity-value').textContent = opacity + '%';
	}

	// Update target FPS
	const targetFpsSlider = document.getElementById('overlay-target-fps');
	if (targetFpsSlider && currentState.config && currentState.config[config.configKey]) {
		// Per-overlay default fallbacks (UI only). If not present, use 10.
		const defaults = {
			OverlayInputs: 30,
			OverlayPit: 30,
			OverlayTraffic: 15,
			OverlayDelta: 15,
			OverlayTrack: 15,
			OverlayDDU: 10,
			OverlayFuel: 10,
			OverlayWeather: 10,
			OverlayFlags: 10,
			OverlayRelative: 10,
			OverlayCover: 10,
			OverlayTire: 10,
			OverlayRadar: 10,
			OverlayStandings: 10
		};
		const cfgIn = currentState.config[config.configKey];
		const def = defaults[config.configKey] !== undefined ? defaults[config.configKey] : 10;
		const fps = (cfgIn.target_fps !== undefined ? cfgIn.target_fps : def);
		targetFpsSlider.value = fps;
		const vEl = document.getElementById('target-fps-value');
		if (vEl) vEl.textContent = fps;
	}

	// Weather-specific: preview
	const previewWeatherRow = document.getElementById('overlay-preview-weather-row');
	if (previewWeatherRow) previewWeatherRow.classList.add('hidden');
	if (overlayKey === 'weather') {
		if (previewWeatherRow) previewWeatherRow.classList.remove('hidden');
		const sel = document.getElementById('overlay-preview-weather-type');
		if (sel) {
			const cfgW = currentState.config && currentState.config['OverlayWeather'];
			if (cfgW && cfgW.preview_weather_type !== undefined) sel.value = cfgW.preview_weather_type;
		}
	}

	// Inputs-specific: steering wheel
	const inputsSteeringRow = document.getElementById('overlay-inputs-steering-row');
	if (inputsSteeringRow) inputsSteeringRow.classList.add('hidden');
	if (overlayKey === 'inputs') {
		if (inputsSteeringRow) inputsSteeringRow.classList.remove('hidden');
		const sel = document.getElementById('overlay-inputs-steering');
		if (sel) {
			const cfgI = currentState.config && currentState.config['OverlayInputs'];
			sel.value = (cfgI && cfgI.steering_wheel) ? cfgI.steering_wheel : 'builtin';
		}
	}

	// Populate generic typography for current overlay
	const cfgT = currentState.config && currentState.config[config.configKey] || {};
	const fontSel = document.getElementById('overlay-font');
	const fontSize = document.getElementById('overlay-font-size');
	const fontSpacing = document.getElementById('overlay-font-spacing');
	const fontStyle = document.getElementById('overlay-font-style');
	const fontWeight = document.getElementById('overlay-font-weight');
	const fontWeightValue = document.getElementById('overlay-font-weight-value');
	if (fontSel) fontSel.value = cfgT.font || 'Poppins';
	if (fontSize) fontSize.value = (cfgT.font_size !== undefined ? cfgT.font_size : 16);
	if (fontSpacing) fontSpacing.value = (cfgT.font_spacing !== undefined ? cfgT.font_spacing : 0.30);
	if (fontStyle) fontStyle.value = cfgT.font_style || 'normal';
	if (fontWeight) fontWeight.value = (cfgT.font_weight !== undefined ? cfgT.font_weight : 500);
	if (fontWeightValue && fontWeight) fontWeightValue.textContent = ` (${fontWeight.value})`;

	// Track-specific
	const trackWidthRow = document.getElementById('overlay-track-width-row');
	if (trackWidthRow) trackWidthRow.classList.add('hidden');
	if (overlayKey === 'track') {
		if (trackWidthRow) trackWidthRow.classList.remove('hidden');
		const cfgTrack = currentState.config && currentState.config['OverlayTrack'];
		if (cfgTrack) {
			const trackWidthSlider = document.getElementById('track-width-slider');
			const trackWidthValue = document.getElementById('track-width-value');
			if (trackWidthSlider && trackWidthValue) {
				const trackWidth = cfgTrack.track_width !== undefined ? cfgTrack.track_width : 6.0;
				trackWidthSlider.value = trackWidth;
				trackWidthValue.textContent = trackWidth.toFixed(1);
			}
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
		const iconContainer = card ? card.querySelector('.flex.items-center.justify-center') : null;

		if (card && iconContainer) {
			// Check if overlay is enabled
			const isEnabled = currentState.config && currentState.config[config.configKey] && currentState.config[config.configKey].enabled;

			if (isEnabled) {
				iconContainer.classList.add('border-2', 'border-green-500');
			} else {
				iconContainer.classList.remove('border-2', 'border-green-500');
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
window.onIFL03State = function(state) {
	currentState = state;
	updateUI();
};
