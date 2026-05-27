'use strict'

function Console(emu)
{
	this.vector = 0
	this.write_el = null
	this.error_el = null

	this.init = () => {
		this.input_el = document.getElementById("console_input")
		this.write_el = document.getElementById("console_std")
		this.error_el = document.getElementById("console_err")
		if (this.input_el) {
			this.input_el.addEventListener("keyup", this.on_console)
		}
		this.start()
	}

	this.start = () => {
		// Hide until vector
		if (this.input_el) {
			this.input_el.style.display = "none"
		}
		// Hide until stdout
		if (this.write_el) {
			this.write_el.style.display = "none"
			this.write_el.innerHTML = ""
		}
		// Hide until stderr
		if (this.error_el) {
			this.error_el.style.display = "none"
			this.error_el.innerHTML = ""
		}
	}

	this.write = (char) => {
		if (this.write_el) {
			this.write_el.innerHTML += String.fromCharCode(char)
			this.write_el.style.display = "block"
		}
	}

	this.error = (char) => {
		if (this.error_el) {
			this.error_el.innerHTML += String.fromCharCode(char)
			this.error_el.style.display = "block"
		}
	}

	this.debug = (byte) => {
		if (this.error_el) {
			this.error_el.innerHTML += byte.toString(16)
		}
	}

	this.on_console = (event) => {
		if (event.key === "Enter" && this.input_el) {
			let query = this.input_el.value
			for (let i = 0; i < query.length; i++)
				this.input(query.charAt(i).charCodeAt(0), 1)
			this.input(0x0a, 1)
			this.input_el.value = ""
		}
	}

	this.input = (char, type) => {
		emu.uxn.dev[0x12] = char
		emu.uxn.dev[0x17] = type
		if(this.vector) emu.uxn.eval(this.vector)
	}

	this.set_vector = (hb, lb) => {
		this.vector = hb << 8 | lb
		if (this.input_el) {
			this.input_el.style.display = "block"
		}
	}
	
	this.deo = (addr) => {
		switch(addr) {
			case 0x10:
			case 0x11: this.set_vector(emu.uxn.dev[0x10], emu.uxn.dev[0x11]); break;
			case 0x18: this.write(emu.uxn.dev[0x18]); break;
			case 0x19: this.error(emu.uxn.dev[0x19]); break;
			case 0x1a: this.debug(emu.uxn.dev[0x1a]); break;
			case 0x1b: this.debug(emu.uxn.dev[0x1b]); break;
		}
	}
}
