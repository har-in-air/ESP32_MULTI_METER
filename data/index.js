// Chart Initialization

var c = document.getElementById("myChart");
//c.width = 800;
//c.height = 400;

var Ctxt = document.getElementById("myChart").getContext("2d");
Ctxt.canvas.width = 800;
Ctxt.canvas.height = 400;

var Time = [];
var Data_mA = [];
var Data_V = [];
for(var inx = 0; inx < 10; inx++){
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
	var sliderSections = document.getElementsByClassName("range-slider");
	for (var i = 0; i < sliderSections.length; i++) {
		var sliders = sliderSections[i].getElementsByTagName("input");
		for (var j = 0; j < sliders.length; j++) {
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
/*
function init_sliders() {
	var slider0 = document.getElementByID("slider0");
	var slider1 = document.getElementByID("slider1");
	slider0.min = 0;
	slider1.min = 0;
	slider0.max = Time.length;
	slider1.max = Time.length;
	slider0.value = 0;
	slider1.value = Time.length;
	slider0.oninput = update_chart;
	slider1.oninput = update_chart;
	slider0.oninput();
	slider1.oninput();
	}
*/


function update_chart() {
	// Get slider values
	var slides = document.getElementsByTagName("input");
	var min = parseFloat(slides[0].value);
	var max = parseFloat(slides[1].value);
	// Neither slider will clip the other, so make sure we determine which is larger
	if (min > max) {
		var tmp = max;
		max = min;
		min = tmp;
		}

	var time_slice = [];
	var data_mA_slice = [];
	var data_V_slice = [];

	time_slice = JSON.parse(JSON.stringify(Time)).slice(min, max);
	data_mA_slice = JSON.parse(JSON.stringify(Data_mA)).slice(min, max);
	data_V_slice = JSON.parse(JSON.stringify(Data_V)).slice(min, max);
	
	ChartInst.data.labels = time_slice;
	ChartInst.data.datasets[0].data = data_mA_slice;
	ChartInst.data.datasets[1].data = data_V_slice;
	ChartInst.update(0); // no animation

	var iAvg = 0.0;
	var vAvg = 0.0;
	var iMax = -9999999.0;
	var iMin = 9999999.0;
	var vMax = -9999999.0;
	var vMin = 9999999.0;
	for(var t = 0; t < time_slice.length ; t++){
		var i = parseFloat(data_mA_slice[t]);
		var v = parseFloat(data_V_slice[t]);
		iAvg = iAvg + i;
		vAvg = vAvg + v;
		if (i > iMax) iMax = i;
		if (v > vMax) vMax = v;
		if (i < iMin) iMin = i;
		if (v < vMin) vMin = v;
		}  
	iAvg = iAvg/time_slice.length;
	vAvg = vAvg/time_slice.length;
	
	var displayElement = document.getElementsByClassName("rangeValues")[0];
	displayElement.innerHTML = "[" + min + "," + max + "]";
	document.getElementById("istats").innerHTML = 
		"iAvg : " + iAvg.toFixed(3) + "mA<p>" + 
		"iMin : " + iMin.toFixed(3) + "mA<p>" + 
		"iMax : " + iMax.toFixed(3) + "mA";
	document.getElementById("vstats").innerHTML = 
		"vAvg : " + vAvg.toFixed(2) + "V<p>" +
		"vMin : " + vMin.toFixed(2) + "V<p>" +
		"vMax : " + vMax.toFixed(2) + "V";
	}	


// WebSocket Initialization

var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

var Scale = 0;
window.addEventListener('load', on_load);

function on_load(event) {
	new_chart();
	init_sliders();
    init_web_socket();
	init_button();
	}

// WebSocket handling

function init_web_socket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
	websocket.binaryType = "arraybuffer";
    websocket.onopen    = on_open;
    websocket.onclose   = on_close;
    websocket.onmessage = on_message;
	}

function on_open(event) {
    console.log('Connection opened');
	}

function on_close(event) {
    console.log('Connection closed');
    setTimeout(init_web_socket, 2000);
	}

function on_message(event) {
	let view = new Int16Array(event.data);
	let len = (view.length - 1) / 2;
	ChartInst.destroy();
	Time = [];
	Data_mA = [];
	Data_V = [];
	//var scaleOpt = document.getElementById("scale").value;
	//var iScale = scaleOpt == "0" ? 0.05 : 0.002381;
	var iScale = view[0] == 0 ? 0.05 : 0.002381;
    var vScale = 0.00125;

	for(var t = 0; t < len; t++){
		Time.push(t);
		var ima = view[2*t+1] * iScale;
		var v = view[2*t+2] * vScale;
		Data_mA.push(ima);
		Data_V.push(v);
		}  
	new_chart();
	init_sliders();
	update_chart();
	}

// Button handling

function init_button() {
    document.getElementById('capture').addEventListener('click', on_capture);
	}

function on_capture(event) {
	var cfgIndex = document.getElementById("cfgInx").value;
	var sampleSeconds = document.getElementById("sampleSecs").value;
	var scale = document.getElementById("scale").value;
	//var nsamples = (1+ Math.random()*499).toFixed(0);
	var jsonObj = {};
	jsonObj["action"] = "capture";
	jsonObj["cfgIndex"] = cfgIndex;
	//jsonObj["sampleSecs"] = nsamples.toString();
	jsonObj["sampleSecs"] = sampleSeconds.toString();
	jsonObj["scale"] = scale;
    websocket.send(JSON.stringify(jsonObj));
	}