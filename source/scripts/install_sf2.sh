SF2_DIR=externals/fluidsynth~.mxo/Contents/Resources/sf2

function download_sf2() {
	mkdir -p ${SF2_DIR}
	if [[ ! -a ${SF2_DIR}/FluidR3_GM.sf2 ]]; then
		wget -P ${SF2_DIR} https://github.com/pianobooster/fluid-soundfont/releases/download/v3.1/FluidR3_GM.sf2
	fi
	if [[ ! -a ${SF2_DIR}/FluidR3_GS.sf2 ]]; then
		wget -P ${SF2_DIR} https://github.com/pianobooster/fluid-soundfont/releases/download/v3.1/FluidR3_GS.sf2
	fi
}

function install_sf2() {\
	mkdir -p ${SF2_DIR}
	cp examples/sf2/*.sf2 ${SF2_DIR}
}


# download_sf2

install_sf2