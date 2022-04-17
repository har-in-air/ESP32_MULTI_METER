
let iScale = 0.05;
let vScale = 0.00125;
let offScale = 0;
let i, v, bgColor;

function update_meter() {
	document.getElementById("ma").innerHTML = i.toFixed(3) + " mA"; 
	if (offScale == 1) {
		document.getElementById("ma").style.backgroundColor = 'orange';
		}
	else {
		document.getElementById("ma").style.backgroundColor = bgColor;
		}
	document.getElementById("volts").innerHTML = v.toFixed(3) + " V";
	}	

// WebSocket Initialization

let gateway = `ws://${window.location.hostname}/ws`;
var websocket;

window.addEventListener('load', on_window_load);

function on_window_load(event) {
	bgColor = document.getElementById("ma").style.backgroundColor;
    init_web_socket();
	}

function trigger_capture() {
	let scale = document.getElementById("scale").value;
	websocket.send("m"+scale);
	}

var periodicTrigger = setInterval(trigger_capture, 1000);
	
window.onbeforeunload = function() {
	clearInterval(periodicTrigger);
	websocket.onclose = function () {}; // disable onclose handler first
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
	let view = new Int16Array(event.data);
	if ((view.length == 5) && (view[0] == 4444)){
		iScale = (view[1] == 0 ? 0.05 : 0.002381);
		i = view[2] * iScale;
		v = view[3] * vScale;
		offScale = view[4];
		update_meter();
		} 
	// acknowledge packet 
	websocket.send("x");
	}


