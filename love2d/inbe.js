function goFullScreen() {
	var canvas = document.getElementById("canvas");
	if (canvas.requestFullScreen) {
		canvas.requestFullScreen();
	} else if (canvas.webkitRequestFullScreen) {
		canvas.webkitRequestFullScreen();
	} else if (canvas.mozRequestFullScreen) {
		canvas.mozRequestFullScreen();
	}
}

document.getElementById("fullscreenButton").addEventListener("click", goFullScreen);
document.getElementById("loadingCanvas").addEventListener("contextmenu", function(e) {
	e.preventDefault();
});
document.getElementById("canvas").addEventListener("contextmenu", function(e) {
	e.preventDefault();
});

var loadingContext = document.getElementById("loadingCanvas").getContext("2d");

function drawLoadingText(text) {
	var canvas = loadingContext.canvas;
	loadingContext.fillStyle = "rgb(142, 195, 227)";
	loadingContext.fillRect(0, 0, canvas.scrollWidth, canvas.scrollHeight);
	loadingContext.font = "2em arial";
	loadingContext.textAlign = "center";
	loadingContext.fillStyle = "rgb( 11, 86, 117 )";
	loadingContext.fillText(text, canvas.scrollWidth / 2, canvas.scrollHeight / 2);
	loadingContext.fillText("Powered By LOVE.", canvas.scrollWidth / 2, canvas.scrollHeight / 4 * 3);
}

window.onload = function() {
	window.focus();
};
window.onclick = function() {
	window.focus();
};
window.addEventListener("keydown", function(e) {
	if ([32, 37, 38, 39, 40].indexOf(e.keyCode) > -1) {
		e.preventDefault();
	}
}, false);

var Module = {
	arguments: ["/game.love"],
	locateFile: function(path) {
		return "build/inbe/" + path;
	},
	INITIAL_MEMORY: 16777216,
	printErr: console.error.bind(console),
	canvas: (function() {
		var canvas = document.getElementById("canvas");
		canvas.addEventListener("webglcontextlost", function(e) {
			alert("WebGL context lost. You will need to reload the page.");
			e.preventDefault();
		}, false);
		return canvas;
	})(),
	setStatus: function(text) {
		if (text) {
			drawLoadingText(text);
		} else if (Module.remainingDependencies === 0) {
			document.getElementById("loadingCanvas").style.display = "none";
			document.getElementById("canvas").style.visibility = "visible";
		}
	},
	totalDependencies: 0,
	remainingDependencies: 0,
	monitorRunDependencies: function(left) {
		this.remainingDependencies = left;
		this.totalDependencies = Math.max(this.totalDependencies, left);
		Module.setStatus(left ? "Preparing... (" + (this.totalDependencies - left) + "/" + this.totalDependencies + ")" : "All downloads complete.");
	}
};

Module.setStatus("Downloading...");
window.onerror = function() {
	Module.setStatus("Exception thrown, see JavaScript console");
	Module.setStatus = function(text) {
		if (text) {
			Module.printErr("[post-exception status] " + text);
		}
	};
};

function applicationLoad() {
	Love(Module);
}

window.addEventListener("load", applicationLoad);
