// User page JavaScript
let currentState = {};

// Initialize the page
document.addEventListener('DOMContentLoaded', function() {
	console.log('User page loaded');
	
	// Request initial state
	requestState();
});

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

// Listen for state updates from the main application
window.onIronState = function(state) {
	currentState = state;
	updateUI();
};
