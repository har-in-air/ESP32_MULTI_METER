// Chart Initialization

let c = document.getElementById("myChart");

let Ctxt = document.getElementById("myChart").getContext("2d");

let periodMs = 1;
let Time = [];
let Data_mA = [];
let Data_V = [];
for(let inx = 0; inx < 1000; inx++){
	Time.push(inx);
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
			},
			{
			label: 'V',
			yAxisID: 'V',
			backgroundColor: "rgb(34, 73, 228)",
			borderColor: "rgb(34, 73, 228)",
			data: Data_V,        
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
				sliders[j].value = (j == 0 ? 0 : Time.length);
				sliders[j].min = 0;
				sliders[j].max = Time.length;
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

	time_slice = JSON.parse(JSON.stringify(Time)).slice(min, max);
	data_mA_slice = JSON.parse(JSON.stringify(Data_mA)).slice(min, max);
	data_V_slice = JSON.parse(JSON.stringify(Data_V)).slice(min, max);
	
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
	displayElement.innerHTML = "[" + min*periodMs + "," + max*periodMs + "]mS";
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
	init_capture_button();
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
	let len = (view.length - 2) / 2;
	ChartInst.destroy();
	Time = [];
	Data_mA = [];
	Data_V = [];
	periodMs = view[0];
	let iScale = view[1] == 0 ? 0.05 : 0.002381;
    let vScale = 0.00125;

	for(var t = 0; t < len; t++){
		Time.push(t*periodMs);
		let ima = view[2*t+2] * iScale;
		let v = view[2*t+3] * vScale;
		Data_mA.push(ima);
		Data_V.push(v);
		}  
	new_chart();
	init_sliders();
	update_chart();
	document.getElementById("led").innerHTML = "<div class=\"led-green\"></div>";
	}

// Button handling

function init_capture_button() {
    document.getElementById('capture').addEventListener('click', on_capture_click);
	}

function on_capture_click(event) {
	let cfgIndex = document.getElementById("cfgInx").value;
	let sampleSeconds = document.getElementById("sampleSecs").value;
	let scale = document.getElementById("scale").value;
	//var nsamples = (1+ Math.random()*499).toFixed(0);
	let jsonObj = {};
	jsonObj["action"] = "capture";
	jsonObj["cfgIndex"] = cfgIndex;
	jsonObj["sampleSecs"] = sampleSeconds.toString();
	jsonObj["scale"] = scale;
    websocket.send(JSON.stringify(jsonObj));
	document.getElementById("led").innerHTML = "<div class=\"led-red\"></div>";
	}
