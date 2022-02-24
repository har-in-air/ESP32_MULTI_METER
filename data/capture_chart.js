// Chart Initialization

let c = document.getElementById("myChart");
let Ctxt = document.getElementById("myChart").getContext("2d");

let timeMs = 0.0;
let periodMs = 0.5;
let iScale = 0.05;
let vScale = 0.00125;
let Time = [];
let Data_mA = [];
let Data_V = [];

for(let inx = 0; inx < 1000; inx++){
	Time.push(periodMs*inx);
	Data_mA.push(0);
	Data_V.push(0);
	}  

var ChartInst;

function new_chart() {
	ChartInst = new Chart(Ctxt, {
	type: "line",
	data: {
		labels: Time,
		datasets: [{
			label: 'mA',
			yAxisID: 'mA',
			backgroundColor: "rgb(209, 20, 61)",
			borderColor: "rgb(209, 20, 61)",
			data: Data_mA,
			cubicInterpolationMode: 'monotone', // maxima/minima stay at sample points
			},
			{
			label: 'V',
			yAxisID: 'V',
			backgroundColor: "rgb(34, 73, 228)",
			borderColor: "rgb(34, 73, 228)",
			data: Data_V,        
			cubicInterpolationMode: 'monotone',
			}],
		},
	options: {
		animation: {
			duration: 0
			},		
		responsive : true,
		maintainAspectRatio: false,		
		borderWidth : 1,
		pointRadius : 0,
		scales: {
			mA : {
				type: 'linear',
				position: 'left',
				ticks : {
					color:	"rgb(209, 20, 61)"
					}
				},
			V : {
				type: 'linear',
				position: 'right',
				ticks : {
					color: "rgb(34, 73, 228)"
					}
				}
			}    
		},  
	});
}

// Chart Handling

function init_sliders() {
	let sliderSections = document.getElementsByClassName("range-slider");
	for (let i = 0; i < sliderSections.length; i++) {
		let sliders = sliderSections[i].getElementsByTagName("input");
		for (let j = 0; j < sliders.length; j++) {
			if (sliders[j].type === "range") {
				sliders[j].oninput = update_chart;
				sliders[j].value = (j == 0 ? 0.0 : Time.length*periodMs);
				sliders[j].min = 0.0;
				sliders[j].max = parseFloat(Time.length)*periodMs;
				// Manually trigger event first time to display values
				sliders[j].oninput();
				}
			}
		}
	}

function update_chart() {
	// Get slider values
	let slides = document.getElementsByTagName("input");
	let min = parseFloat(slides[0].value);
	let max = parseFloat(slides[1].value);
	// Neither slider will clip the other, so make sure we determine which is larger
	if (min > max) {
		let tmp = max;
		max = min;
		min = tmp;
		}

	let time_slice = [];
	let data_mA_slice = [];
	let data_V_slice = [];

	let min_index = min/periodMs;
	let max_index = max/periodMs;

	time_slice = JSON.parse(JSON.stringify(Time)).slice(min_index, max_index);
	data_mA_slice = JSON.parse(JSON.stringify(Data_mA)).slice(min_index, max_index);
	data_V_slice = JSON.parse(JSON.stringify(Data_V)).slice(min_index, max_index);
	
	ChartInst.data.labels = time_slice;
	ChartInst.data.datasets[0].data = data_mA_slice;
	ChartInst.data.datasets[1].data = data_V_slice;
	ChartInst.update(0); // no animation

	let iAvg = 0.0;
	let vAvg = 0.0;
	let iMax = -9999999.0;
	let iMin = 9999999.0;
	let vMax = -9999999.0;
	let vMin = 9999999.0;
	for(let t = 0; t < time_slice.length ; t++){
		let i = parseFloat(data_mA_slice[t]);
		let v = parseFloat(data_V_slice[t]);
		iAvg = iAvg + i;
		vAvg = vAvg + v;
		if (i > iMax) iMax = i;
		if (v > vMax) vMax = v;
		if (i < iMin) iMin = i;
		if (v < vMin) vMin = v;
		}  
	iAvg = iAvg/time_slice.length;
	vAvg = vAvg/time_slice.length;
	
	let displayElement = document.getElementsByClassName("rangeValues")[0];
	displayElement.innerHTML = "[" + min + "," + max + "]mS";
	document.getElementById("istats").innerHTML = 
		"avg : " + iAvg.toFixed(3) + "mA<br>" + 
		"min : " + iMin.toFixed(3) + "mA<br>" + 
		"max : " + iMax.toFixed(3) + "mA";
	document.getElementById("vstats").innerHTML = 
		"avg : " + vAvg.toFixed(3) + "V<br>" +
		"min : " + vMin.toFixed(3) + "V<br>" +
		"max : " + vMax.toFixed(3) + "V";
	}	


// WebSocket Initialization

let gateway = `ws://${window.location.hostname}/ws`;
var websocket;


window.addEventListener('load', on_window_load);

function on_window_load(event) {
	new_chart();
	init_sliders();
    init_web_socket();
	init_capture_buttons();
	}

window.onbeforeunload = function() {
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
	if ((view.length == 1) && (view[0] == 1234)){
		document.getElementById("led").innerHTML = "<div class=\"led-red\"></div>";
		}
	else 
	if ((view.length > 3) && (view[0] == 1111)){
		// new capture tx start
		periodMs = parseFloat(view[1])/1000.0;
		iScale = view[2] == 0 ? 0.05 : 0.002381;
		ChartInst.destroy();
		timeMs = 0.0;
		Time = [];
		Data_mA = [];
		Data_V = [];
		let len = (view.length - 3) / 2;
		for(let t = 0; t < len; t++){
			Time.push(timeMs);
			let ima = view[2*t+3] * iScale;
			let v = view[2*t+4] * vScale;
			Data_mA.push(ima);
			Data_V.push(v);
			timeMs += periodMs;
			} 
		// ready to receive next data packet 
	    websocket.send("x");
		new_chart();
		init_sliders();
		update_chart();
		}
	else 
	if ((view.length > 1) && (view[0] == 2222)){
		let len = (view.length - 1) / 2;
		for(let t = 0; t < len; t++){
			Time.push(timeMs);
			let ima = view[2*t+1] * iScale;
			let v = view[2*t+2] * vScale;
			Data_mA.push(ima);
			Data_V.push(v);
			timeMs += periodMs;
			}  
		// ready to receive next data packet 
		websocket.send("x");
		init_sliders();
		update_chart();
		}
	else		
	if ((view.length == 1) && (view[0] == 3333)){
		// tx complete
		init_sliders();
		//update_chart();
		document.getElementById("led").innerHTML = "<div class=\"led-green\"></div>";
		}
	}

// Button handling

function init_capture_buttons() {
    document.getElementById("capture").addEventListener("click", on_capture_click);
    document.getElementById("captureGated").addEventListener("click", on_capture_gated_click);
	}

function on_capture_click(event) {
	let cfgIndex = document.getElementById("cfgInx").value;
	let captureSeconds = document.getElementById("captureSecs").value;
	let scale = document.getElementById("scale").value;
	let jsonObj = {};
	jsonObj["action"] = "capture";
	jsonObj["cfgIndex"] = cfgIndex;
	jsonObj["captureSecs"] = captureSeconds.toString();
	jsonObj["scale"] = scale;
    websocket.send(JSON.stringify(jsonObj));
	// set capture led to red, indicate capturing
	document.getElementById("led").innerHTML = "<div class=\"led-red\"></div>";
	}


function on_capture_gated_click(event) {
	let cfgIndex = document.getElementById("cfgInx").value;
	let scale = document.getElementById("scale").value;
	let jsonObj = {};
	jsonObj["action"] = "capture";
	jsonObj["cfgIndex"] = cfgIndex;
	// set capture seconds to 0 for gated capture
	jsonObj["captureSecs"] = "0"; 
	jsonObj["scale"] = scale;
	websocket.send(JSON.stringify(jsonObj));
	// set capture led to yellow, indicate waiting for gate
	document.getElementById("led").innerHTML = "<div class=\"led-yellow\"></div>";
	}
	

function on_sample_rate_change(selectObject) {
	let value = selectObject.value;  
	let docobj = document.getElementById("captureSecs");
	if (value == "0") {
		docobj.max = "8";
		if (docobj.value > 8) docobj.value = 8; 
		}
	else
	if (value == "1") {
		docobj.max = "16";
		if (docobj.value > 16) docobj.value = 16; 
		}
	else 
	if (value == "2") {
		docobj.max = "40";
		if (docobj.value > 40) docobj.value = 40; 
		}
  	}	
