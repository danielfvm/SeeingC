function apply(id, value) {
	console.log(id, value)
	if (value.length > 0) {
		$.get(`/set/${id}/${value}`, (data) => {
			if (data == 'true') {
				$(`#btn_${id}`).prop('disabled', true);
			} else {
				alert(data);
			}
		});
	}
}

function request(url, params = {}, method = 'GET') {
    return $.ajax({
      url: url,
      type: method,
      dataType: 'text',
      data: params
    });
}

Math.toRadians = (deg) => Math.PI / 180.0 * deg;

window.onload = () => {

	/// Integer settings ///
	$("input[type='number']").each(function() {
		$(`#btn_${this.id}`)[0].onclick = () => {
			apply(this.id, this.value);
		};

		this.onchange = () => {
			$('#btn_' + this.id).prop('disabled', false);
		};

		this.onkeypress = (evt) => {
			var theEvent = evt || window.event;

			// Handle paste
			if (theEvent.type === 'paste') {
				key = evt.clipboardData.getData('text/plain');
			} else {
				// Handle key press
				var key = theEvent.keyCode || theEvent.which;
				key = String.fromCharCode(key);
			}

			var regex = /[0-9]|\./;
			if (!regex.test(key)) {
				theEvent.returnValue = false;
				if (theEvent.preventDefault) {
					theEvent.preventDefault();
				}
			} else {
				$(`#btn_${this.id}`).prop('disabled', false);
			}
		};
	});


	/// Mode settings ///
	$("select").each(function() {
		$(`#btn_${this.id}`)[0].onclick = () => {
			apply(this.id, this.value);
		};

		this.onchange = () => {
			$('#btn_' + this.id).prop('disabled', false);
		};
	});


	/// Checkbox settings ///
	$("input[type='checkbox']").each(function() {
		$(`#btn_${this.id}`)[0].onclick = () => {
			apply(this.id, this.checked ? "1" : "0");
		};

		this.onchange = () => {
			$('#btn_' + this.id).prop('disabled', false);
		};
	});


	/// ActionButton ///
	$("button[class='actionbtn']").each(function() {
		this.onclick = () => {
			this.innerHTML = "<img src='static/loading.gif' width='100%'></img>";
			this.disabled = true;
			$.get(`/call/${this.id}`, (data) => {
				alert(data);
				this.innerHTML = "Run";
				this.disabled = false;
			});
		};
	});



	const viewport = $('#viewport')[0];
	const zoom = $('#zoom')[0];

	let start = {x: 0, y: 0};
	let scale = 1;
	let panning = false;
	let pointX = Math.floor(window.innerWidth / 2);
	let pointY = Math.floor(window.innerHeight / 2);

	const setTransform = () => {
		zoom.style.transform = 'translate(' + pointX + 'px, ' + pointY + 'px) scale(' + scale + ')';
	}

	viewport.onmousedown = (e) => {
		e.preventDefault();
		start = {x: e.clientX - pointX, y: e.clientY - pointY};
		panning = true;
	}

	viewport.onmouseleave = viewport.onmouseup = (_) => {
		panning = false;
	}

	viewport.onmousemove = (e) => {
		e.preventDefault();
		if (!panning) {
			return;
		}
		pointX = (e.clientX - start.x);
		pointY = (e.clientY - start.y);
		setTransform();
	}

	viewport.onwheel = (e) => {
		e.preventDefault();
		let xs = (e.clientX - pointX) / scale,
			ys = (e.clientY - pointY) / scale,
			delta = (e.wheelDelta ? e.wheelDelta : -e.deltaY);
		(delta > 0) ? (scale *= 1.2) : (scale /= 1.2);
		pointX = e.clientX - xs * scale;
		pointY = e.clientY - ys * scale;

		setTransform();
	}

	setTransform();

	let width = 0, height = 0;

	let chart = new Chart("chart", {
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
						labelString: 'Distance (pixels)'
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
						labelString: 'Gray Value',
					},
					ticks: {
						beginAtZero: true,
					}
				}]
			}
		},
	});

	/* Update image, status and indicators */
	$('img[data-lazysrc]').each(function () {
		$(this).attr('src', $(this).attr('data-lazysrc') + '?a=' + Math.random());
		$(this).on('load', async () => {
			width = this.width;
			height = this.height;
			$('#imginfo')[0].innerText = width + 'x' + height;

			const canvas = $('#canvas')[0];
			const ctx = canvas.getContext("2d");
			if (canvas.width != width) canvas.width = width;
			if (canvas.height != height) canvas.height = height;

			// Set status text
			$('#status')[0].innerText = await request("/status");

			// Get all points for drawing a the star profil graph
			const profil = (await request("/profil")).split('\n').map((x) => parseInt(x));
			$("#chartwrapper")[0].style.display = profil.length > 1 ? "block" : "none";
			chart.data.labels = [...Array(parseInt(profil.length / 2) + 1).keys()];
			chart.data.datasets[0].data = profil.splice(0, profil.length / 2);
			chart.data.datasets[1].data = profil;
			chart.update();

			// Get all star coordinates
			const stars = (await request("/stars")).split('\n').map((x) => x.split(' '));

			// Get indicators for finding polaris
			const indicators = (await request("/indicators")).split('\n');
			const [ radius_polaris, deg_polaris, pltslv_x, pltslv_y ] = indicators;

			// Clear screen
			ctx.clearRect(0, 0, width, height);

			// Update image
			setTimeout(() => {
				$(this).attr('src', $(this).attr('data-lazysrc') + '?a=' + Math.random());
			}, 100);

			if (profil.length > 1) {
				return;
			}


			//  Draw boxes around stars on canvas
			for (let star of stars) {
				let s = parseInt(star[2]) + 3;
				let x = parseInt(star[0]) + 0.5;
				let y = parseInt(star[1]) + 0.5;
				ctx.beginPath();
				ctx.lineWidth = "1";
				ctx.strokeStyle = "red";
				ctx.arc(x, y, s, 0, 2 * Math.PI, false);
				ctx.lineWidth = 3;
				ctx.stroke();
			}

			// Line to polaris
			let polarisX = width/2+radius_polaris*Math.sin((Math.PI/180)*deg_polaris);
			let polarisY = height/2-radius_polaris*Math.cos((Math.PI/180)*deg_polaris);
			ctx.strokeStyle = 'blue';
			ctx.lineWidth = 3;
			ctx.beginPath();
			ctx.moveTo(width/2, height/2);
			ctx.lineTo(polarisX, polarisY);
			ctx.stroke();

			for (let i = 0; i < 24; ++ i) {
				let polarisX = width/2+radius_polaris*Math.sin((Math.PI/180)*(i*15));
				let polarisY = height/2-radius_polaris*Math.cos((Math.PI/180)*(i*15));
				ctx.strokeStyle = 'gray';
				ctx.lineWidth = 1;
				ctx.beginPath();
				ctx.moveTo(width/2, height/2);
				ctx.lineTo(polarisX, polarisY);
				ctx.stroke();
			}

			ctx.strokeStyle = 'blue';
			ctx.lineWidth = 3;
			ctx.beginPath();
			ctx.arc(width/2, height/2, radius_polaris, 0, 2 * Math.PI, false);
			ctx.lineWidth = 5;
			ctx.stroke();

			// Draw centerpoint
			ctx.strokeStyle = 'green';
			ctx.lineWidth = 3;
			ctx.beginPath();
			ctx.moveTo(polarisX, polarisY);
			ctx.lineTo(polarisX+pltslv_x, polarisY+pltslv_y);
			ctx.stroke();
		});
	});
};
