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

function applyAxes(data) {
	let x = 0;
	return data.map(y => {
		return {
			"x": x++,
			"y": y
		};
	});
}

Math.toRadians = (deg) => Math.PI / 180.0 * deg;

window.onload = () => {

	/// Integer settings ///
	$("input[type='number']").each(function() {
		$(`#btn_${this.id}`)[0].onclick = () => {
			console.log(this);
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

	const rektaszension=45.699482; 
	const deklination=89.261964; 
	const siderial=21.992521;

	$('img[data-lazysrc]').each(function () {
		$(this).attr('src', $(this).attr('data-lazysrc') + '?a=' + Math.random());
		$(this).on('load', () => {
			width = this.width;
			height = this.height;
			$('#imginfo')[0].innerText = width + 'x' + height;

			let canvas = $('#canvas')[0];
			if (canvas.width != width) canvas.width = width;
			if (canvas.height != height) canvas.height = height;

			$.get('/status', (data) => {
				$('#status')[0].innerText = data;

				$.get('/stars', (data) => {
					let stars = data.split('\n').map((x) => x.split(' '));
					var ctx = canvas.getContext("2d");

					$.get('/profil', (data) => {
						let values = data.split('\n').map((x) => parseInt(x));
						chart.data.labels = [...Array(parseInt(values.length / 2) + 1).keys()];
						chart.data.datasets[0].data = values.splice(0, values.length / 2);
						chart.data.datasets[1].data = values;
						chart.update();

						$("#chartwrapper")[0].style.display = values.length > 1 ? "block" : "none";

						// Draw stars
						if (values.length <= 1) {
							ctx.clearRect(0, 0, width, height);
							ctx.lineWidth = "1";
							ctx.strokeStyle = "red";

							for (let star of stars) {
								let s = parseInt(star[2]) + 3;
								let x = parseInt(star[0] - s / 2) + 0.5;
								let y = parseInt(star[1] - s / 2) + 0.5;
								ctx.strokeRect(x, y, s, s);
							}
						}

						// Draw centerpoint
						ctx.strokeStyle = 'blue';
						ctx.lineWidth = 3;
						ctx.beginPath();
						ctx.moveTo(width/2, height/2);
						ctx.lineTo(width/2+Math.cos(Math.toRadians(rektaszension))*300, height/2+Math.sin(Math.toRadians(rektaszension))*300);
						ctx.stroke();

						// Update image
						setTimeout(() => {
							$(this).attr('src', $(this).attr('data-lazysrc') + '?a=' + Math.random());
						}, 100);

					});
				});
			});
		});
	});

};
