//Basic 4wires, Autodetect speed config
//AIM65 - 2020

@cfn 4w_48k_base
@fln Basic_Volume

cfg_reg registers[] = {
//program memory
    { 0x00, 0x00 },		
    { 0x7f, 0x00 },		Switch to Book 0
    { 0x02, 0x11 },		PowerDown DSP. Bit 0 not documented
    { 0x01, 0x11 },		Reset module & registers. Bits are auto cleared
    { 0x00, 0x00 },		Wait
    { 0x00, 0x00 },
    { 0x00, 0x00 },	
    { 0x00, 0x00 },
    { 0x03, 0x11 },		Mute L&R
    { 0x2a, 0x00 },		Zero Data path L&R (= mute...)
    { 0x25, 0x18 },		Ignore MCLK error detection
    { 0x0d, 0x00 },		Clck config..
    { 0x02, 0x00 },		Wake up DSP
//Sample rate update
    { 0x00, 0x00 },		
    { 0x7f, 0x00 },		Switch to Book0 ... again
    { 0x02, 0x80 },		Reset DSP
    { 0x00, 0x00 },		
    { 0x7f, 0x00 },		Switch to Book0 ... again
// speed 03-48k 04-96k
//dynamically reading speed
    { 0x22, 0x03 },		Set FS Speed. Ignored in auto clock mode	
    { 0x00, 0x00 },		Switch to Book0 ... again
    { 0x7f, 0x00 },
    { 0x02, 0x00 },		Restart DSP
//write coefficients of various components
    { 0x00, 0x00 },		Switch to Book8c
    { 0x7f, 0x8c },
    { 0x00, 0x1e },
    { 0x44, 0x00 },		Vol to 0dB
    { 0x45, 0x80 },
    { 0x46, 0x00 },
    { 0x47, 0x00 },
    { 0x48, 0x00 },
    { 0x49, 0x80 },
    { 0x4a, 0x00 },
    { 0x4b, 0x00 },

//swap command
    { 0x00, 0x00 },		
    { 0x7f, 0x8c },		Switch to Book8c
    { 0x00, 0x23 },		Goto Page 23
    { 0x14, 0x00 },		Write 00 00 00 01 in reg 0x14+
    { 0x15, 0x00 },
    { 0x16, 0x00 },
    { 0x17, 0x01 },

//register tuning
    { 0x00, 0x00 },		
    { 0x7f, 0x00 },		Switch to Book0
    { 0x00, 0x00 },
    { 0x07, 0x00 },		SDOut is post processing output	
    { 0x08, 0x20 },		SDOut is output. No mute from PCM
    { 0x55, 0x16 },		SDOut mux selector - DSP Boot done
    { 0x00, 0x00 },		
    { 0x7f, 0x00 },		Switch to Book0
    { 0x00, 0x00 },
    { 0x3d, 0x24 },		Digital volume Left	-18dB = -> 36 = 0x24
    { 0x3e, 0x24 },		Digital volume Right - same - 
    { 0x00, 0x00 },		
    { 0x7f, 0x00 },		Switch to Book0
    { 0x00, 0x01 },		Switch to Page1
    { 0x02, 0x00 },		Analog Gain set to 0dB	
    { 0x06, 0x01 },		Analog mute follows digital mute	


//Unmute the device
    { 0x00, 0x00 },		Switch to Book0
    { 0x7f, 0x00 },
    { 0x03, 0x00 },		UnMute L&R	
    { 0x2a, 0x11 },		Enable data path (L-L & R-R)

};