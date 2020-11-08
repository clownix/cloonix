/*
 * flowatcher.js
 */

document.getElementById("serial").textContent = getWebSocketURL();
document.getElementById("parse_res").textContent = "Initializing";


/* flow export protocol */

var socket;
openSocket();

function openSocket() {
	if (typeof MozWebSocket != "undefined") {
		socket = new MozWebSocket(getWebSocketURL(), "flow-export-protocol");
	} else {
		socket = new WebSocket(getWebSocketURL(), "flow-export-protocol");
	}

	try {
		socket.onopen = function() {
			document.getElementById("ws_statustd").style.backgroundColor = "#40ff40";
			document.getElementById("ws_status").textContent = "websocket connection opened";
			document.getElementById("ws_control").value = "Close websocket connection";
			document.getElementById("ws_control").onclick = closeSocket;
			document.getElementById("ws_control").disabled = false;
			initRenderDatasets();
		}

		socket.binaryType = 'arraybuffer';
		socket.onmessage = function (msg) {
			processData(msg.data);
		}

		socket.onclose = function() {
			document.getElementById("ws_statustd").style.backgroundColor = "#ff4040";
			document.getElementById("ws_status").textContent = "websocket connection CLOSED";
			document.getElementById("ws_control").value = "Open websocket connection";
			document.getElementById("ws_control").onclick = openSocket;
			document.getElementById("ws_control").disabled = false;
		}
	} catch(exception) {
		alert('<p>Error' + exception);
	}
}
function closeSocket() { socket.send("closeme\n"); }
function resetSerial() { socket.send("reset\n"); }

function getWebSocketURL() {
	var p;
	var u = document.URL;

	if (u.substring(0, 5) == "https") {
		p = "wss://";
		u = u.substr(8);
	} else {
		p = "ws://";
		if (u.substring(0, 4) == "http")
			u = u.substr(7);
	}

	u = u.split('/');

	/* + "/xxx" bit is for IE10 workaround */
	return p + u[0] + "/xxx";
}


/* main process */

var prevSerial;
function processData(data) {
	var v = new DataView(data);
	var serial = v.getUint32(0, true);
	var flowCount = v.getUint32(4, true);
	var ipType = v.getUint32(8, true);
	var kpps = v.getFloat32(12, true);
	var mbps = v.getFloat32(16, true);

	document.getElementById("serial").textContent = "Serial: " + serial;
	document.getElementById("flow_cnt").textContent = "Flow Count: " + flowCount;
	document.getElementById("RX_status").textContent = "RX: " +
		kpps.toFixed(2) + " kpps, " + mbps.toFixed(2) + " Mbps";

	if (serial > prevSerial)
		shiftAllDatasets();

	if (ipType == 0) {
		/* ipv4 */
		for (var offset = 20, n = data.byteLength; offset < n; offset += 24) {
			v = new DataView(data, offset);
			updateDataset(decode4(v));
		}
	} else {
		/* ipv6 */
		for (var offset = 20, n = data.byteLength; offset < n; offset += 48) {
			v = new DataView(data, offset);
			updateDataset(decode6(v));
		}
	}
	document.getElementById("parse_res").textContent = serial + " parsed.";

	checkExpiredDatasets();
	calcStackedDatasets();

	sortLegendIndex();
	genChartLegend();
	updateCharts();

	prevSerial = serial;
}

function decode4(v) {
	return {
		key: [
			formatIPv4addr(v.getUint32(8, true)), /* srcip */
			formatIPv4addr(v.getUint32(12, true)), /* dstip */
			v.getUint16(16, true), /* srcport */
			v.getUint16(18, true), /* dstport */
			v.getUint8(20), /* proto */
		],
		stats: [
			v.getUint32(0, true), /* pps */
			v.getUint32(4, true), /* bps */
		]
	};
}

function decode6(v) {
	return {
		key: [
			formatIPv6addr([
				v.getUint16(8, false), v.getUint16(10, false),
				v.getUint16(12, false), v.getUint16(14, false),
				v.getUint16(16, false), v.getUint16(18, false),
				v.getUint16(20, false), v.getUint16(22, false),
			]), /* srcip */
			formatIPv6addr([
				v.getUint16(24, false), v.getUint16(26, false),
				v.getUint16(28, false), v.getUint16(30, false),
				v.getUint16(32, false), v.getUint16(34, false),
				v.getUint16(36, false), v.getUint16(38, false),
			]), /* dstip */
			v.getUint16(40, true), /* srcport */
			v.getUint16(42, true), /* dstport */
			v.getUint8(44), /* proto */
		],
		stats: [
			v.getUint32(0, true), /* pps */
			v.getUint32(4, true), /* bps */
		]
	};
}

function formatIPv4addr(ip) {
	return (0xff&(ip>>24))+"."+(0xff&(ip>>16))+"."+(0xff&(ip>>8))+"."+(0xff&ip);
}
function formatIPv6addr(ip) {
	return "["+conv4hex(ip[0])+":"+conv4hex(ip[1])+":"+ conv4hex(ip[2])+":"+conv4hex(ip[3])+":"
		+ conv4hex(ip[4])+":"+conv4hex(ip[5])+":"+ conv4hex(ip[6])+":"+conv4hex(ip[7])+"]";
}
function conv4hex(n) { return ("000"+n.toString(16)).slice(-4); }

function strFlowFromKey(key) {
	var proto;
	if (key[4] == 6)
		proto = "TCP";
	else if (key[4] == 17)
		proto = "UDP";
	else
		proto = key[4];
	return key[0]+":"+key[2]+" -("+proto+")-> "+key[1]+":"+key[3];
}


/* flow statistic datasets */

var datasetsPkt = {};
var datasetsByte = {};

function shiftAllDatasets() {
	var d;
	for (var key in datasetsPkt) {
		if (datasetsPkt.hasOwnProperty(key)) {
			d = datasetsPkt[key];
			d.shift();
			d.push(0);
			d = datasetsByte[key];
			d.shift();
			d.push(0);
		}
	}
}

function checkExpiredDatasets() {
	var d;
	for (var key in datasetsPkt) {
		if (datasetsPkt.hasOwnProperty(key)) {
			d = datasetsPkt[key];
			var sum = 0;
			for (var i = 0; i < 30; i++) {
				sum += d[i];
			}
			if (sum == 0) {
				var x = renderDatasetsIndex.indexOf(key);
				var y = legendIndex.indexOf(key);
				if (x != -1) {
					lineChartDataPkt.datasets.splice(x, 1);
					lineChartDataByte.datasets.splice(x, 1);
					renderDatasetsIndex.splice(x, 1);
					renderColorIndex.splice(x, 1);
					renderDatasetsPkt.splice(x, 1);
					renderDatasetsByte.splice(x, 1);
					stackedDatasetsPkt.splice(x, 1);
					stackedDatasetsByte.splice(x, 1);
				}
				if (y != -1) {
					legendIndex.splice(y, 1);
				}

				delete datasetsPkt[key];
				delete datasetsByte[key];
				delete legendAve[key];
			}
		}
	}
}

function updateDataset(data) {
	var key = data.key;
	if (datasetsPkt.hasOwnProperty(key)) {
		datasetsPkt[key][29] = data.stats[0];
		datasetsByte[key][29] = data.stats[1];
	} else {
		datasetsPkt[key] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,data.stats[0]];
		datasetsByte[key] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,data.stats[1]];
		legendIndex.push(key.toString());
	}
}


/* Draw Graph with Chart.js */

var renderDatasetsIndex;
var renderColorIndex;
var renderDatasetsPkt;
var renderDatasetsByte;
var stackedDatasetsPkt;
var stackedDatasetsByte;

var lineChartDataPkt;
var lineChartDataByte;

var xlabel = ['30','29','28','27','26','25','24','23','22','21','20','19','18','17','16','15','14','13','12','11','10','9','8','7','6','5','4','3','2','1'];

function initRenderDatasets() {
	renderDatasetsIndex = [0];
	renderColorIndex = [0];
	renderDatasetsPkt = [[]];
	renderDatasetsByte = [[]];
	stackedDatasetsPkt = [[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]];
	stackedDatasetsByte = [[0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]];

	lineChartDataPkt = {
		labels: xlabel,
		datasets: [presetDataset("data0")],
	};
	lineChartDataByte = {
		labels: xlabel,
		datasets: [presetDataset("data0")],
	};
}
initRenderDatasets();

var optionPkt = {
	animation: false,
	scaleLabel: "<%=value%> pps",
	showTooltips: false,
	pointDot: false,
	bezierCurve: false,
};
var optionByte = {
	animation: false,
	scaleLabel: "<%=value%> bps",
	showTooltips: false,
	pointDot: false,
	bezierCurve: false,
};

var ctxByte = document.getElementById("linechart_byte").getContext("2d");
var chartByte = new Chart(ctxByte).Line(lineChartDataByte, optionByte);
var ctxPkt = document.getElementById("linechart_pkt").getContext("2d");
var chartPkt = new Chart(ctxPkt).Line(lineChartDataPkt, optionPkt);

var colorArray = [
	15073298,1908872,39236,14942335,16773376,41193,15964160,40598,9421599,26807,9570179,15007823,
	15425792,39787,13622016,34513,6297990,15007850,41153,16566272,2272312,18333,12451969,15073331
];

function getColorIndex() {
	for (var i = 0; i < colorArray.length; i++) {
		var x = renderColorIndex.indexOf(colorArray[i]);
		if (x == -1)
			return i;
	}
	return -1;
}

function updateCharts() {
	chartByte = new Chart(ctxByte).Line(lineChartDataByte, optionByte);
	chartPkt = new Chart(ctxPkt).Line(lineChartDataPkt, optionPkt);
}

var legendIndex = [];
var legendAve = {};
var termCalcLegendAve = 30;
function sortLegendIndex() {
	for (var key in datasetsPkt) {
		var s = 0;
		for (i = 0; i < termCalcLegendAve; i++) {
			s += datasetsByte[key][i];
		}
		legendAve[key] = s;
	}
	legendIndex.sort(function (a,b) {
		if (legendAve[a] > legendAve[b]) return -1;
		if (legendAve[a] < legendAve[b]) return 1;
		return 0;
	});
}

var legendLimit = 300;
function genChartLegend() {
	var table = document.getElementById("flowdata");
	table.textContent = null;

	var row = document.createElement("tr");
	row.innerHTML = "<th>Show Graph</th><th>Graph Color</th><th>Flow</th>"
		+ "<th class=\"t_val\">packets / sec</th><th class=\"t_val\">bytes / sec</th>";
	table.appendChild(row);

	var counter = 0;
	for (var i = 0; i < legendIndex.length; i++) {
		var key = legendIndex[i];
		if (legendLimit > 0 && counter > legendLimit)
			break;
		counter += 1;
		if (datasetsPkt.hasOwnProperty(key)) {
			var dp = datasetsPkt[key], db = datasetsByte[key];

			row = document.createElement("tr");
			var cellBtn = row.insertCell(0);
			var cellColor = row.insertCell(1);
			var cellFlow = row.insertCell(2);
			var cellPkt = row.insertCell(3);
			var cellByte = row.insertCell(4);

			var x = renderDatasetsIndex.indexOf(key);
			if (x == -1) {
				cellBtn.appendChild(document.createTextNode("show"));
				cellBtn.style.backgroundColor = "#fff";
				cellColor.style.backgroundColor = "#fff";
			} else {
				cellBtn.appendChild(document.createTextNode("hide"));
				cellBtn.style.backgroundColor = "#3fff3f";
				cellColor.style.backgroundColor = genRGBA(renderColorIndex[x], 1);
			}
			cellBtn.style.cursor = "pointer";
			cellBtn.style.textAlign = "center";
			cellBtn.onclick = callSwitchDataset(key);
			cellFlow.appendChild(document.createTextNode(strFlowFromKey(key.split(","))));
			cellPkt.appendChild(document.createTextNode(dp[29]));
			cellPkt.style.textAlign = "right";
			cellByte.appendChild(document.createTextNode(db[29]));
			cellByte.style.textAlign = "right";

			table.appendChild(row);
		}
	}
}

function calcStackedDatasets() {
	var len = renderDatasetsIndex.length;
	for (var i = len - 1; i > 0; i--) {
		for (var j = 0; j < 30; j++) {
			stackedDatasetsPkt[i-1][j] =
				stackedDatasetsPkt[i][j] + renderDatasetsPkt[i-1][j];
			stackedDatasetsByte[i-1][j] =
				stackedDatasetsByte[i][j] + renderDatasetsByte[i-1][j];
		}
	}
	return len;
}

function switchDataset(key) {
	var x = renderDatasetsIndex.indexOf(key);

	if (x == -1) { /* add */
		var y = getColorIndex();
		if (y != -1) {
			renderDatasetsIndex.unshift(key);
			renderColorIndex.unshift(colorArray[y]);
			renderDatasetsPkt.unshift(datasetsPkt[key]);
			renderDatasetsByte.unshift(datasetsByte[key]);
			stackedDatasetsPkt.unshift([0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]);
			stackedDatasetsByte.unshift([0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]);

			calcStackedDatasets();
			lineChartDataPkt.datasets.unshift(
					genNewDataset(key, colorArray[y], stackedDatasetsPkt[0])
			);
			lineChartDataByte.datasets.unshift(
					genNewDataset(key, colorArray[y], stackedDatasetsByte[0])
			);
		}

	} else { /* remove */
		lineChartDataPkt.datasets.splice(x, 1);
		lineChartDataByte.datasets.splice(x, 1);
		renderDatasetsIndex.splice(x, 1);
		renderColorIndex.splice(x, 1);
		renderDatasetsPkt.splice(x, 1);
		renderDatasetsByte.splice(x, 1);
		stackedDatasetsPkt.splice(x, 1);
		stackedDatasetsByte.splice(x, 1);

		calcStackedDatasets();
	}

	genChartLegend();
	updateCharts();
}
function callSwitchDataset(arg) { return function(){ switchDataset(arg); }; }

function genNewDataset(key, color, datasets) {
	return {
		strokeColor: genRGBA(color, 1),
		fillColor: genRGBA(color, 0.8),
		pointColor: genRGBA(color, 1),
		pointStrokeColor: genRGBA(color, 1),
		data: datasets,
		label: strFlowFromKey(key)
	};
}
function presetDataset(label) {
	return {
		strokeColor: 'rgba(0,0,0,1)',
		fillColor: 'rgba(0,0,0,0.8)',
		pointColor: 'rgba(0,0,0,1)',
		pointStrokeColor: 'rgba(0,0,0,1)',
		data: [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0],
		label: label
	};
}

function genRGBA(x, alpha) {
	return 'rgba('+(0xff&(x>>16))+','+(0xff&(x>>8))+','+(0xff&x)+','+alpha+')';
}
