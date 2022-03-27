
let freqHz;

function update_frequency() {
	document.getElementById("hz").innerHTML = freqHz + " Hz"; 
	}	

// WebSocket Initialization

let gateway = `ws://${window.location.hostname}/ws`;
var websocket;

window.addEventListener('load', on_window_load);

function on_window_load(event) {
    init_web_socket();
	}

function trigger_capture() {
	websocket.send("f");
	}

var periodicTrigger = setInterval(trigger_capture, 1000);
	
window.onbeforeunload = function() {
	clearInterval(periodicTrigger);
	//websocket.onclose = function () {}; // disable onclose handler first
	websocket.close();
	}

// WebSocket handling

function init_web_socket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
	websocket.binaryType = "arraybuffer";
    websocket.onopen    = on_ws_open;
    websocket.onclose   = on_ws_close;
    websocket.onmessage = on_ws_message;
	}

function on_ws_open(event) {
    console.log('Connection opened');
	}

function on_ws_close(event) {
    console.log('Connection closed');
    setTimeout(init_web_socket, 2000);
	}


function on_ws_message(event) {
	let view = new Int32Array(event.data);
	if ((view.length == 2) && (view[0] == 5555)){
		freqHz = view[1];
		update_frequency();
		} 
	// acknowledge packet 
	websocket.send("x");
	}


function on_osc_freq_change(selectObject) {
	let value = selectObject.value;  
	let jsonObj = {};
	jsonObj["action"] = "oscfreq";
	jsonObj["freqhz"] = value;
	websocket.send(JSON.stringify(jsonObj));
	}	
	