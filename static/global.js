const elementCanvas = $('#canvas_overlay')[0];
const elementStatus = $('#status')[0];
const elementVideo = $('#video')[0];
const ctx = elementCanvas.getContext("2d");

elementVideo.src = document.location.href.replaceAll("8080/", "8081/image");

let counter = 0;

function apply(id, value) {
	if (value.length > 0) {
		$.get(`/set/${id}/${value}`, (data) => {
			if (data != 'true') {
				alert(data);
			}
			window.location.reload();
		});
	}
}

async function downloadImage() {
	const date = (new Date()).toLocaleDateString("de-DE").replaceAll(".", "_");
	const a = document.createElement('a');

	a.href = "/fullimage?a=" + Date.now();
	a.download = date + '.png';
	a.click();
}

Math.toRadians = function (deg) {
	return Math.PI / 180.0 * deg;
};

/// Integer settings ///
$("input[type='number']").each(function () {
	$(`#btn_${this.id}`)[0].onclick = () => {
		apply(this.id, this.value);
	};
	this.onkeypress = (e) => {
		console.log("tes");
		if (e.keyCode == 13) {
			apply(this.id, this.value);
		}
	};
});


/// Mode settings ///
$("select").each(function () {
	$(`#btn_${this.id}`)[0].onclick = () => {
		apply(this.id, this.value);
	};
});


/// Checkbox settings ///
$("input[type='checkbox']").each(function () {
	$(`#btn_${this.id}`)[0].onclick = () => {
		apply(this.id, this.checked ? "1" : "0");
	};
});


/// ActionButton ///
$("button[class='actionbtn']").each(function () {
	this.onclick = () => {
		if (this.id == "btn_download") {
			downloadImage();
			return;
		}

		this.innerHTML = "<img src='static/loading.gif' width='100%'></img>";
		this.disabled = true;

		$.get(`/call/${this.id}`, (data) => {
			alert(data);

			this.innerHTML = "Run";
			this.disabled = false;
		});
	};
});

// Make font color of chart white
Chart.defaults.global.defaultFontColor = "#fff";

// Create a chart
const chart = new Chart("chart", {
	type: "line",
	data: {
		datasets: [{
			data: [],
			borderColor: "blue",
			fill: false,
			pointRadius: 0,
			label: 'Vertical',
		}, {
			data: [],
			borderColor: "orange",
			fill: false,
			pointRadius: 0,
			label: 'Horizontal'
		}],
	},
	options: {
		animation: false,
		events: ['click'],
		scales: {
			xAxes: [{
				display: true,
				scaleLabel: {
					display: true,
					labelString: 'Position (pixels)'
				},
				ticks: {
					autoSkip: true,
					maxTicksLimit: 8
				}
			}],
			yAxes: [{
				display: true,
				scaleLabel: {
					display: true,
					labelString: 'Brightness',
				},
				ticks: {
					beginAtZero: true,
				}
			}]
		}
	},
});
chart.canvas.style.display = "none";

/* When image has loaded, refresh the canvas size */
setInterval(async () => {
	$.ajax({
		url: "/info",
		type: "GET",
		dataType: 'json',
		success: (data) => {
			const { width, height } = elementVideo;

			if (!width || !height) {
				elementStatus.innerText = "Image not fully loaded yet!";
				return;
			}

			if (elementCanvas.width != width || elementCanvas.height != height) {
				elementCanvas.width = width;
				elementCanvas.height = height;
				if (elementCanvas.pan) {
					elementCanvas.pan.dispose();
				}
				elementCanvas.pan = panzoom($("#draggable")[0], {
					zoomDoubleClickSpeed: 1,
					minZoom: 0.1,
					initialZoom: window.innerHeight/height/2
				});
				elementCanvas.pan.moveTo(window.innerWidth/2-window.innerHeight/4-150, window.innerHeight/2-window.innerHeight/4);
			}

			elementStatus.innerText = 'Dimensions: ' + width + 'x' + height + '\nDisplayed Frame: ' + counter + '\n' + data["status"];

			// hide chart
			chart.canvas.style.display = "none";

			// refresh overlay canvas and draw indicators and stars
			ctx.clearRect(0, 0, width, height);

			const {profil, stars, radius_polaris, deg_polaris, pltslv_x, pltslv_y} = data;

			// Get all points for drawing a the star profil graph
			if (profil && profil.vertical.length > 0) {

				// Set information for chart
				chart.canvas.style.display = "block";
				chart.data.labels = [...Array(parseInt(profil.vertical.length) + 1).keys()]; // creates a list filled with numbers counting up from: 1 to size of profil
				chart.data.datasets[0].data = profil.horizonzal;
				chart.data.datasets[1].data = profil.vertical;
				chart.update();

			} else {
				//  Draw boxes around stars on canvas
				for (let star of stars) {
					let d = star["d"] + 3;  // diameter
					let x = star["x"] + 0.5;
					let y = star["y"] + 0.5;

					ctx.beginPath();
					ctx.lineWidth = "1";
					ctx.strokeStyle = "red";
					ctx.arc(x, y, d, 0, 2 * Math.PI, false);
					ctx.lineWidth = 3;
					ctx.stroke();
				}

				// Line to polaris
				let polarisX = width / 2 + radius_polaris * Math.sin((Math.PI / 180) * deg_polaris);
				let polarisY = height / 2 - radius_polaris * Math.cos((Math.PI / 180) * deg_polaris);
				ctx.strokeStyle = 'blue';
				ctx.lineWidth = 3;
				ctx.beginPath();
				ctx.moveTo(width / 2, height / 2);
				ctx.lineTo(polarisX, polarisY);
				ctx.stroke();

				// Grid
				for (let i = 0; i < 24; ++i) {
					let polarisX = width / 2 + radius_polaris * Math.sin((Math.PI / 180) * (i * 15));
					let polarisY = height / 2 - radius_polaris * Math.cos((Math.PI / 180) * (i * 15));
					ctx.strokeStyle = 'gray';
					ctx.lineWidth = 1;
					ctx.beginPath();
					ctx.moveTo(width / 2, height / 2);
					ctx.lineTo(polarisX, polarisY);
					ctx.stroke();
				}

				// Polaris orbit
				ctx.strokeStyle = 'blue';
				ctx.lineWidth = 3;
				ctx.beginPath();
				ctx.arc(width / 2, height / 2, radius_polaris, 0, 2 * Math.PI, false);
				ctx.lineWidth = 5;
				ctx.stroke();

				// Draw platesolving line
				ctx.strokeStyle = 'green';
				ctx.lineWidth = 3;
				ctx.beginPath();
				ctx.moveTo(polarisX, polarisY);
				ctx.lineTo(polarisX + pltslv_x, polarisY + pltslv_y);
				ctx.stroke();
			}
		},
		error: function (_request, _status, errorThrown) {
			elementStatus.innerText = "Connection to server failed " + errorThrown;
		}
	});
}, 1000);


// These lines of codes prevent Safaris pan/zoom behaviour which break the page
// source: https://stackoverflow.com/questions/37808180/disable-viewport-zooming-ios-10-safari
document.addEventListener('gesturestart', function (e) {
    e.preventDefault();
});

document.addEventListener('touchmove', function (event) {
  if (event.scale !== 1) { event.preventDefault(); }
}, false);

var lastTouchEnd = 0;
document.addEventListener('touchend', function (event) {
  var now = (new Date()).getTime();
  if (now - lastTouchEnd <= 300) {
    event.preventDefault();
  }
  lastTouchEnd = now;
}, false);
