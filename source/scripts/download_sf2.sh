

function download_sf2() {
	sf2_dir=externals/fluidsynth~.mxo/Contents/Resources/sf2
	mkdir -p ${sf2_dir}
	if [[ ! -a ${sf2_dir}/FluidR3_GM.sf2 ]]; then
		wget -P ${sf2_dir} https://github.com/pianobooster/fluid-soundfont/releases/download/v3.1/FluidR3_GM.sf2
	fi
	if [[ ! -a ${sf2_dir}/FluidR3_GS.sf2 ]]; then
		wget -P ${sf2_dir} https://github.com/pianobooster/fluid-soundfont/releases/download/v3.1/FluidR3_GS.sf2
	fi
}

download_sf2
