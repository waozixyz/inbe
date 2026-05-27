'use strict'

function peek16(mem, addr) {
	return (mem[addr] << 8) + mem[addr + 1]
}

function poke16(mem, addr, val) {
	mem[addr] = val >> 8, mem[addr + 1] = val;
}

function Emu (embed)
{
	this.embed = embed
	this.system = new System(this)
	this.console = new Console(this)
	this.controller = new Controller(this)
	this.screen = new Screen(this)
	this.datetime = new DateTime(this)
	this.mouse = new Mouse(this)

	if (typeof UxnWASM !== 'undefined') {
		console.log("Using WebAssembly core")
		this.uxn = new (UxnWASM.Uxn)(this)
	} else {
		console.log("Using Vanilla JS core")
		this.uxn = new Uxn(this)
	}

	this.init = (embed) => {
		this.uxn.init(this).then(() => {
			/* start devices */
			this.console.init()
			this.screen.init()
			this.controller.init()
			/* Reveal */
			const themeClass = localStorage.getItem('theme') || 'light';
			document.body.className = (this.embed ? "embed" : "default") + " " + themeClass;
			// File loading removed for simplified lotus pages
			// Decode rom in url
			const rom_url = window.location.hash.match(/r(om)?=([^&]+)/);
			if (rom_url) {
				let rom = b64decode(rom_url[2]);
				if(!rom_url[1])
					rom = decodeUlz(rom);
				this.load(rom);
			}
			else if (boot_ulz) {
				this.load(decodeUlz(b64decode(boot_ulz)));
			}
			else if(boot_rom.length) {
				this.load(boot_rom);
			}
			// Start screen vector
			setInterval(() => {
				window.requestAnimationFrame(() => {
					if(this.screen.vector)
						this.uxn.eval(this.screen.vector)
					if(this.screen.repaint) {
						this.screen.x1 = 0
						this.screen.y1 = 0
						this.screen.x2 = this.screen.width
						this.screen.y2 = this.screen.height
						this.screen.repaint = 0
						let x = this.screen.x1, y = this.screen.y1;
						let w = this.screen.x2 - x, h = this.screen.y2 - y;
						this.screen.redraw()
						const imagedata = new ImageData(this.screen.pixels, this.screen.width, this.screen.height)
						this.screen.displayctx.putImageData(imagedata,0,0,x,y,w,h);
					}
					if(this.screen.changed()) {
						let x = this.screen.x1, y = this.screen.y1;
						let w = this.screen.x2 - x, h = this.screen.y2 - y;
						this.screen.redraw()
						const imagedata = new ImageData(this.screen.pixels, this.screen.width, this.screen.height)
						this.screen.displayctx.putImageData(imagedata,0,0,x,y,w,h);
					}
				});
			}, 1000 / 60);
		})
	}

	this.start = (rom) => {
		this.console.start()
		this.screen.set_zoom(default_zoom ? default_zoom : 1)
		this.uxn.load(rom).eval(0x0100);
		// Share and save features removed for simplified lotus pages
	}

	this.load_file = (file) => {
		let reader = new FileReader()
		reader.onload = (e) => {
			this.load(new Uint8Array(e.target.result));
		};
		reader.readAsArrayBuffer(file)
	}

	this.load = (rom) => {
		this.start(rom)
	}

	this.dei = (port) => {
		switch (port & 0xf0) {
		case 0xc0: return this.datetime.dei(port)
		case 0x20: return this.screen.dei(port)
		}
		return this.uxn.dev[port]
	}

	this.deo = (port, val) => {
		this.uxn.dev[port] = val
		switch(port & 0xf0) {
			case 0x00: this.system.deo(port); break;
			case 0x10: this.console.deo(port); break;
			case 0x20: this.screen.deo(port); break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// Sharing and Save features removed for simplified lotus pages
////////////////////////////////////////////////////////////////////////////////

const save = { setROM: (v) => {} };
const share = { setROM: (v) => {} };

async function b64encode(bs) {
	const url = await new Promise(resolve => {
		const reader = new FileReader()
		reader.onload = () => { resolve(reader.result); }
		reader.readAsDataURL(new Blob([bs]))
	});
	return url.slice(url.indexOf(',') + 1).replace(/\//g, '_').replace(/\+/g, '-').replace(/=+$/, '');
}

function b64decode(s) {
	if (s.length % 4 != 0) {
		s += ('===').slice(0, 4 - (s.length % 4));
	}
	return new Uint8Array([...atob(s.replace(/_/g, '/').replace(/-/g, '+'))].map(c=>c.charCodeAt()));
}

function decodeUlz(src) {
	const dst = [];
	let sp = 0;
	while (sp < src.length) {
		const c = src[sp++];
		if (c & 0x80) {
			// CPY
			let length;
			if (c & 0x40) {
				if (sp >= src.length) {
				throw new Error(`incomplete CPY2`);
				}
				length = ((c & 0x3f) << 8) | src[sp++];
			} else {
				length = c & 0x3f;
			}
			if (sp >= src.length) {
				throw new Error(`incomplete CPY`);
			}
			let cp = dst.length - (src[sp++] + 1);
			if (cp < 0) {
				throw new Error(`CPY underflow`);
			}
			for (let i = 0; i < length + 4; i++) {
				dst.push(dst[cp++]);
			}
		} else {
			// LIT
			if (sp + c >= src.length) {
				throw new Error(`LIT out of bounds: ${sp} + ${c} >= ${src.length}`);
			}
			for (let i = 0; i < c + 1; i++) {
				dst.push(src[sp++]);
			}
		}
	}
	return new Uint8Array(dst);
}

const MIN_MAX_LENGTH = 4;

function findBestMatch(src, sp, dlen, slen) {
	let bmlen = 0;
	let bmp = 0;
	let dp = sp - dlen;
	for (; dlen; dp++, dlen--) {
		let i = 0;
		for (; ; i++) {
			if (i == slen) {
				return [dp, i];
			}
			if (src[sp + i] != src[dp + (i % dlen)]) {
				break;
			}
		}
		if (i > bmlen) {
			bmlen = i;
			bmp = dp;
		}
	}
	return [bmp, bmlen];
}

function encodeUlz(src) {
	let dst = [];
	let sp = 0;
	let litp = -1;
	while (sp < src.length) {
		const dlen = Math.min(sp, 256);
		const slen = Math.min(src.length - sp, 0x3fff + MIN_MAX_LENGTH);
		const [bmp, bmlen] = findBestMatch(src, sp, dlen, slen);
		if (bmlen >= MIN_MAX_LENGTH) {
			// CPY
			const bmctl = bmlen - MIN_MAX_LENGTH;
			if (bmctl > 0x3f) {
				//	CPY2
				dst.push((bmctl >> 8) | 0xc0);
				dst.push(bmctl & 0xff);
			} else {
				dst.push(bmctl | 0x80);
			}
			dst.push(sp - bmp - 1);
			sp += bmlen;
			litp = -1;
		} else {
			// LIT
			if (litp >= 0) {
				if ((dst[litp] += 1) == 127) {
					litp = -1;
				}
			} else {
				dst.push(0);
				litp = dst.length - 1;
			}
			dst.push(src[sp++]);
		}
	}
	return new Uint8Array(dst);
}
