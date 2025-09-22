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

	// Standings: Font Settings
	const standingsFontSel = document.getElementById('overlay-standings-font');
	if (standingsFontSel) {
		standingsFontSel.addEventListener('change', function() {
			if (selectedOverlay === 'standings') {
				sendCommand('setConfigString', {
					component: 'OverlayStandings',
					key: 'font',
					value: this.value
				});
			}
		});
	}

	const standingsFontSizeInput = document.getElementById('overlay-standings-font-size');
	if (standingsFontSizeInput) {
		standingsFontSizeInput.addEventListener('input', function() {
			if (selectedOverlay === 'standings') {
				sendCommand('setConfigFloat', {
					component: 'OverlayStandings',
					key: 'font_size',
					value: parseFloat(this.value)
				});
			}
		});
	}

	const standingsFontSpacingInput = document.getElementById('overlay-standings-font-spacing');
	if (standingsFontSpacingInput) {
		standingsFontSpacingInput.addEventListener('input', function() {
			if (selectedOverlay === 'standings') {
				sendCommand('setConfigFloat', {
					component: 'OverlayStandings',
					key: 'font_spacing',
					value: parseFloat(this.value)
				});
			}
		});
	}

	const standingsFontStyleSel = document.getElementById('overlay-standings-font-style');
	if (standingsFontStyleSel) {
		standingsFontStyleSel.addEventListener('change', function() {
			if (selectedOverlay === 'standings') {
				sendCommand('setConfigString', {
					component: 'OverlayStandings',
					key: 'font_style',
					value: this.value
				});
			}
		});
	}

	const standingsFontWeightSlider = document.getElementById('overlay-standings-font-weight');
	if (standingsFontWeightSlider) {
		standingsFontWeightSlider.addEventListener('input', function() {
			const value = parseInt(this.value);
			document.getElementById('overlay-standings-font-weight-value').textContent = value;
			sendCommand('setConfigInt', {
				component: 'OverlayStandings',
				key: 'font_weight',
				value
			});
		});
	}

	// Relative: Font Settings
	const relativeFontSel = document.getElementById('overlay-relative-font');
	if (relativeFontSel) {
		relativeFontSel.addEventListener('change', function() {
			if (selectedOverlay === 'relative') {
				sendCommand('setConfigString', {
					component: 'OverlayRelative',
					key: 'font',
					value: this.value
				});
			}
		});
	}

	const relativeFontSizeInput = document.getElementById('overlay-relative-font-size');
	if (relativeFontSizeInput) {
		relativeFontSizeInput.addEventListener('input', function() {
			if (selectedOverlay === 'relative') {
				sendCommand('setConfigFloat', {
					component: 'OverlayRelative',
					key: 'font_size',
					value: parseFloat(this.value)
				});
			}
		});
	}

	const relativeFontSpacingInput = document.getElementById('overlay-relative-font-spacing');
	if (relativeFontSpacingInput) {
		relativeFontSpacingInput.addEventListener('input', function() {
			if (selectedOverlay === 'relative') {
				sendCommand('setConfigFloat', {
					component: 'OverlayRelative',
					key: 'font_spacing',
					value: parseFloat(this.value)
				});
			}
		});
	}

	const relativeFontStyleSel = document.getElementById('overlay-relative-font-style');
	if (relativeFontStyleSel) {
		relativeFontStyleSel.addEventListener('change', function() {
			if (selectedOverlay === 'relative') {
				sendCommand('setConfigString', {
					component: 'OverlayRelative',
					key: 'font_style',
					value: this.value
				});
			}
		});
	}

	const relativeFontWeightSlider = document.getElementById('overlay-relative-font-weight');
	if (relativeFontWeightSlider) {
		relativeFontWeightSlider.addEventListener('input', function() {
			const value = parseInt(this.value);
			document.getElementById('overlay-relative-font-weight-value').textContent = value;
			sendCommand('setConfigInt', {
				component: 'OverlayRelative',
				key: 'font_weight',
				value
			});
		});
	}

	// Track: Track Width slider
	const trackWidthSlider = document.getElementById('track-width-slider');
	if (trackWidthSlider) {
		trackWidthSlider.addEventListener('input', function() {
			if (selectedOverlay === 'track') {
				const value = parseFloat(this.value);
				document.getElementById('track-width-value').textContent = value.toFixed(1);
				sendCommand('setConfigFloat', {
					component: 'OverlayTrack',
					key: 'track_width',
					value: value
				});
			}
		});
	}

	// Fuel: Font Settings
	const fuelFontSel = document.getElementById('overlay-fuel-font');
	if (fuelFontSel) {
		fuelFontSel.addEventListener('change', function() {
			if (selectedOverlay === 'fuel') {
				sendCommand('setConfigString', {
					component: 'OverlayFuel',
					key: 'font',
					value: this.value
				});
			}
		});
	}

	const fuelFontSizeInput = document.getElementById('overlay-fuel-font-size');
	if (fuelFontSizeInput) {
		fuelFontSizeInput.addEventListener('input', function() {
			if (selectedOverlay === 'fuel') {
				sendCommand('setConfigFloat', {
					component: 'OverlayFuel',
					key: 'font_size',
					value: parseFloat(this.value)
				});
			}
		});
	}

	const fuelFontSpacingInput = document.getElementById('overlay-fuel-font-spacing');
	if (fuelFontSpacingInput) {
		fuelFontSpacingInput.addEventListener('input', function() {
			if (selectedOverlay === 'fuel') {
				sendCommand('setConfigFloat', {
					component: 'OverlayFuel',
					key: 'font_spacing',
					value: parseFloat(this.value)
				});
			}
		});
	}

	const fuelFontStyleSel = document.getElementById('overlay-fuel-font-style');
	if (fuelFontStyleSel) {
		fuelFontStyleSel.addEventListener('change', function() {
			if (selectedOverlay === 'fuel') {
				sendCommand('setConfigString', {
					component: 'OverlayFuel',
					key: 'font_style',
					value: this.value
				});
			}
		});
	}

	const fuelFontWeightSlider = document.getElementById('overlay-fuel-font-weight');
	if (fuelFontWeightSlider) {
		fuelFontWeightSlider.addEventListener('input', function() {
			const value = parseInt(this.value);
			document.getElementById('overlay-fuel-font-weight-value').textContent = value;
			sendCommand('setConfigInt', {
				component: 'OverlayFuel',
				key: 'font_weight',
				value
			});
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
	const priority = { enabled: 0, show_in_menu: 1, show_in_race: 2 };
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

	// Update target FPS
	const targetFpsSlider = document.getElementById('overlay-target-fps');
	if (targetFpsSlider && currentState.config && currentState.config[config.configKey]) {
		// Per-overlay default fallbacks (UI only). If not present, use 10.
		const defaults = {
			OverlayInputs: 30,
			OverlayPit: 30,
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
		const cfg = currentState.config[config.configKey];
		const def = defaults[config.configKey] !== undefined ? defaults[config.configKey] : 10;
		const fps = (cfg.target_fps !== undefined ? cfg.target_fps : def);
		targetFpsSlider.value = fps;
		const vEl = document.getElementById('target-fps-value');
		if (vEl) vEl.textContent = fps;
	}

	// Flags-specific: preview flag selector removed; no flags-specific UI

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

	// Standings-specific: show font settings when Standings overlay selected
	const standingsFontRow = document.getElementById('overlay-standings-font-row');
	if (standingsFontRow) standingsFontRow.classList.add('hidden');
	if (overlayKey === 'standings') {
		if (standingsFontRow) standingsFontRow.classList.remove('hidden');
		// Update font settings from config
		const cfg = currentState.config && currentState.config['OverlayStandings'] || {};
		const fontSel = document.getElementById('overlay-standings-font');
		const fontSizeInput = document.getElementById('overlay-standings-font-size');
		const fontSpacingInput = document.getElementById('overlay-standings-font-spacing');
		const fontStyleSel = document.getElementById('overlay-standings-font-style');
		const fontWeightSlider = document.getElementById('overlay-standings-font-weight');
		const fontWeightValue = document.getElementById('overlay-standings-font-weight-value');

		if (fontSel) fontSel.value = cfg.font || (currentState.config && currentState.config.Overlay && currentState.config.Overlay.font) || 'Poppins';
		if (fontSizeInput) fontSizeInput.value = (cfg.font_size !== undefined ? cfg.font_size : (currentState.config && currentState.config.Overlay && currentState.config.Overlay.font_size) || 16);
		if (fontSpacingInput) fontSpacingInput.value = (cfg.font_spacing !== undefined ? cfg.font_spacing : (currentState.config && currentState.config.Overlay && currentState.config.Overlay.font_spacing) || 0.30);
		if (fontStyleSel) fontStyleSel.value = cfg.font_style || (currentState.config && currentState.config.Overlay && currentState.config.Overlay.font_style) || 'normal';
		if (fontWeightSlider) fontWeightSlider.value = (cfg.font_weight !== undefined ? cfg.font_weight : (currentState.config && currentState.config.Overlay && currentState.config.Overlay.font_weight) || 500);
		if (fontWeightValue) fontWeightValue.textContent = fontWeightSlider ? fontWeightSlider.value : '';
	}

	// Fuel-specific: show font settings when Fuel overlay selected
	const fuelFontRow = document.getElementById('overlay-fuel-font-row');
	if (fuelFontRow) fuelFontRow.classList.add('hidden');
	if (overlayKey === 'fuel') {
		if (fuelFontRow) fuelFontRow.classList.remove('hidden');
		// Update font settings from config
		const cfg = currentState.config && currentState.config['OverlayFuel'] || {};
		const fontSel = document.getElementById('overlay-fuel-font');
		const fontSizeInput = document.getElementById('overlay-fuel-font-size');
		const fontSpacingInput = document.getElementById('overlay-fuel-font-spacing');
		const fontStyleSel = document.getElementById('overlay-fuel-font-style');
		const fontWeightSlider = document.getElementById('overlay-fuel-font-weight');
		const fontWeightValue = document.getElementById('overlay-fuel-font-weight-value');

		if (fontSel) fontSel.value = cfg.font || (currentState.config && currentState.config.Overlay && currentState.config.Overlay.font) || 'Poppins';
		if (fontSizeInput) fontSizeInput.value = (cfg.font_size !== undefined ? cfg.font_size : (currentState.config && currentState.config.Overlay && currentState.config.Overlay.font_size) || 16);
		if (fontSpacingInput) fontSpacingInput.value = (cfg.font_spacing !== undefined ? cfg.font_spacing : (currentState.config && currentState.config.Overlay && currentState.config.Overlay.font_spacing) || 0.30);
		if (fontStyleSel) fontStyleSel.value = cfg.font_style || (currentState.config && currentState.config.Overlay && currentState.config.Overlay.font_style) || 'normal';
		if (fontWeightSlider) fontWeightSlider.value = (cfg.font_weight !== undefined ? cfg.font_weight : (currentState.config && currentState.config.Overlay && currentState.config.Overlay.font_weight) || 500);
		if (fontWeightValue) fontWeightValue.textContent = fontWeightSlider ? fontWeightSlider.value : '';
	}

	// Relative-specific: show font settings when Relative overlay selected
	const relativeFontRow = document.getElementById('overlay-relative-font-row');
	if (relativeFontRow) relativeFontRow.classList.add('hidden');
	if (overlayKey === 'relative') {
		if (relativeFontRow) relativeFontRow.classList.remove('hidden');
		// Update font settings from config
		const cfg = currentState.config && currentState.config['OverlayRelative'] || {};
		const fontSel = document.getElementById('overlay-relative-font');
		const fontSizeInput = document.getElementById('overlay-relative-font-size');
		const fontSpacingInput = document.getElementById('overlay-relative-font-spacing');
		const fontStyleSel = document.getElementById('overlay-relative-font-style');
		const fontWeightSlider = document.getElementById('overlay-relative-font-weight');
		const fontWeightValue = document.getElementById('overlay-relative-font-weight-value');

		if (fontSel) fontSel.value = cfg.font || (currentState.config && currentState.config.Overlay && currentState.config.Overlay.font) || 'Poppins';
		if (fontSizeInput) fontSizeInput.value = (cfg.font_size !== undefined ? cfg.font_size : (currentState.config && currentState.config.Overlay && currentState.config.Overlay.font_size) || 16);
		if (fontSpacingInput) fontSpacingInput.value = (cfg.font_spacing !== undefined ? cfg.font_spacing : (currentState.config && currentState.config.Overlay && currentState.config.Overlay.font_spacing) || 0.30);
		if (fontStyleSel) fontStyleSel.value = cfg.font_style || (currentState.config && currentState.config.Overlay && currentState.config.Overlay.font_style) || 'normal';
		if (fontWeightSlider) fontWeightSlider.value = (cfg.font_weight !== undefined ? cfg.font_weight : (currentState.config && currentState.config.Overlay && currentState.config.Overlay.font_weight) || 500);
		if (fontWeightValue) fontWeightValue.textContent = fontWeightSlider ? fontWeightSlider.value : '';
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


	// Tire-specific: show tire settings when Tire overlay selected
	const tireSettingsRow = document.getElementById('overlay-tire-settings-row');
	if (tireSettingsRow) tireSettingsRow.classList.add('hidden');
	if (overlayKey === 'tire') {
		if (tireSettingsRow) tireSettingsRow.classList.remove('hidden');

		// Update tire settings from config
		const cfg = currentState.config && currentState.config['OverlayTire'] || {};
		const tempCoolInput = document.getElementById('overlay-tire-temp-cool');
		const tempOptInput = document.getElementById('overlay-tire-temp-opt');
		const tempHotInput = document.getElementById('overlay-tire-temp-hot');

		if (tempCoolInput) tempCoolInput.value = (cfg.temp_cool_c !== undefined ? cfg.temp_cool_c : 60);
		if (tempOptInput) tempOptInput.value = (cfg.temp_opt_c !== undefined ? cfg.temp_opt_c : 85);
		if (tempHotInput) tempHotInput.value = (cfg.temp_hot_c !== undefined ? cfg.temp_hot_c : 105);
	}

	// Track-specific: show track width slider when Track overlay selected
	const trackWidthRow = document.getElementById('overlay-track-width-row');
	if (trackWidthRow) trackWidthRow.classList.add('hidden');
	if (overlayKey === 'track') {
		if (trackWidthRow) trackWidthRow.classList.remove('hidden');

		const cfg = currentState.config && currentState.config['OverlayTrack'];
		if (cfg) {
			// Update track width slider from config
			const trackWidthSlider = document.getElementById('track-width-slider');
			const trackWidthValue = document.getElementById('track-width-value');
			if (trackWidthSlider && trackWidthValue) {
				const trackWidth = cfg.track_width !== undefined ? cfg.track_width : 6.0;
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
