/*************************************************************************

	Midway T-unit system

    driver by Alex Pasadyn, Zsolt Vasvari, Kurt Mahan, Ernesto Corvi,
    and Aaron Giles

	Games supported:
		* Mortal Kombat (T-unit version)
		* Mortal Kombat 2
		* NBA Jam
		* NBA Jam Tournament Edition
		* Judge Dredd (prototype)

	Known bugs:
		* page flipping seems off in NBA Jam (or else there's a blank-the
			screen bit we're missing)

**************************************************************************/


#include "driver.h"
#include "cpu/tms34010/tms34010.h"
#include "cpu/adsp2100/adsp2100.h"
#include "sndhrdw/williams.h"
#include "sndhrdw/dcs.h"
#include "midtunit.h"

#include "bootstrap.h"
#include "inptport.h"
#include "controls.h"

const char *const mk_sample_names_tunit[] =
{
	"*mk",
	"title-01.wav",
	"title-02.wav",
	"c-select-01.wav",
	"c-select-02.wav",
	"battle-menu-01.wav",
	"battle-menu-02.wav",
	"continue-01.wav",
	"continue-02.wav",
	"fatality-01.wav",
	"fatality-02.wav",
	"courtyard-01.wav",
	"courtyard-02.wav",
	"courtyard-end-01.wav",
	"courtyard-end-02.wav",
	"courtyard-finish-him-01.wav",
	"courtyard-finish-him-02.wav",
	"test-your-might-01.wav",
	"test-your-might-02.wav",
	"test-your-might-end-01.wav",
	"test-your-might-end-02.wav",
	"gameover-01.wav",
	"gameover-02.wav",
	"warriors-shrine-01.wav",
	"warriors-shrine-02.wav",
	"warriors-shrine-end-01.wav",
	"warriors-shrine-end-02.wav",
	"warriors-shrine-finish-him-01.wav",
	"warriors-shrine-finish-him-02.wav",
	"pit-01.wav",
	"pit-02.wav",
	"pit-end-01.wav",
	"pit-end-02.wav",
	"pit-finish-him-01.wav",
	"pit-finish-him-02.wav",
	"throne-room-01.wav",
	"throne-room-02.wav",
	"throne-room-end-01.wav",
	"throne-room-end-02.wav",
	"throne-room-finish-him-01.wav",
	"throne-room-finish-him-02.wav",
	"goros-lair-01.wav",
	"goros-lair-02.wav",
	"goros-lair-end-01.wav",
	"goros-lair-end-02.wav",
	"goros-lair-finish-him-01.wav",
	"goros-lair-finish-him-02.wav",
	"endurance-switch-01.wav",
	"endurance-switch-02.wav",
	"victory-01.wav",
	"victory-02.wav",
	"palace-gates-01.wav",
	"palace-gates-02.wav",
	"palace-gates-end-01.wav",
	"palace-gates-end-02.wav",
	"palace-gates-finish-him-01.wav",
	"palace-gates-finish-him-02.wav",
	0
};

static struct Samplesinterface mk_samples_tunit =
{
	2,	/* 2 channels*/
	100, /* volume*/
	mk_sample_names_tunit
};

const char *const nba_jam_sample_names_tunit[] =
{
	"*nbajam",
	"main-theme-01.wav",
	"main-theme-02.wav",
	"team-select-01.wav",
	"team-select-02.wav",
	"ingame-01.wav", /* First & third quarter*/
	"ingame-02.wav",
	"ingame-03.wav", /* Second & fourth quarter*/
	"ingame-04.wav",
	"intermission-01.wav",
	"intermission-02.wav",
	"halftime-01.wav",
	"halftime-02.wav",
	"theme-end-01.wav",
	"theme-end-02.wav",
	0
};

static struct Samplesinterface nba_jam_samples_tunit =
{
	2,	/* 2 channels*/
	100, /* volume*/
	nba_jam_sample_names_tunit
};

/*************************************
 *
 *	Memory maps
 *
 *************************************/

static MEMORY_READ16_START( readmem )
	{ TOBYTE(0x00000000), TOBYTE(0x003fffff), midtunit_vram_r },
	{ TOBYTE(0x01000000), TOBYTE(0x013fffff), MRA16_RAM },
	{ TOBYTE(0x01400000), TOBYTE(0x0141ffff), midtunit_cmos_r },
	{ TOBYTE(0x01600000), TOBYTE(0x0160003f), midtunit_input_r },
	{ TOBYTE(0x01800000), TOBYTE(0x0187ffff), MRA16_RAM },
	{ TOBYTE(0x01a80000), TOBYTE(0x01a800ff), midtunit_dma_r },
	{ TOBYTE(0x01d00000), TOBYTE(0x01d0001f), midtunit_sound_state_r },
	{ TOBYTE(0x01d01020), TOBYTE(0x01d0103f), midtunit_sound_r },
	{ TOBYTE(0x02000000), TOBYTE(0x07ffffff), midtunit_gfxrom_r },
	{ TOBYTE(0xc0000000), TOBYTE(0xc00001ff), tms34010_io_register_r },
	{ TOBYTE(0xff800000), TOBYTE(0xffffffff), MRA16_RAM },
MEMORY_END


static MEMORY_WRITE16_START( writemem )
	{ TOBYTE(0x00000000), TOBYTE(0x003fffff), midtunit_vram_w },
	{ TOBYTE(0x01000000), TOBYTE(0x013fffff), MWA16_RAM, &midyunit_scratch_ram },
	{ TOBYTE(0x01400000), TOBYTE(0x0141ffff), midtunit_cmos_w, (data16_t **)&generic_nvram, &generic_nvram_size },
	{ TOBYTE(0x01480000), TOBYTE(0x014fffff), midtunit_cmos_enable_w },
	{ TOBYTE(0x01800000), TOBYTE(0x0187ffff), midtunit_paletteram_w, &paletteram16 },
	{ TOBYTE(0x01a80000), TOBYTE(0x01a800ff), midtunit_dma_w },
	{ TOBYTE(0x01b00000), TOBYTE(0x01b0001f), midtunit_control_w },
/*	{ TOBYTE(0x01c00060), TOBYTE(0x01c0007f), midtunit_cmos_enable_w }, */
	{ TOBYTE(0x01d01020), TOBYTE(0x01d0103f), midtunit_sound_w },
	{ TOBYTE(0x01d81060), TOBYTE(0x01d8107f), watchdog_reset16_w },
	{ TOBYTE(0x01f00000), TOBYTE(0x01f0001f), midtunit_control_w },
	{ TOBYTE(0x02000000), TOBYTE(0x07ffffff), MWA16_ROM, (data16_t **)&midyunit_gfx_rom, &midyunit_gfx_rom_size },
	{ TOBYTE(0xc0000000), TOBYTE(0xc00001ff), tms34010_io_register_w },
	{ TOBYTE(0xff800000), TOBYTE(0xffffffff), MWA16_ROM, &midyunit_code_rom },
MEMORY_END



/*************************************
 *
 *	Input ports
 *
 *************************************/

INPUT_PORTS_START( mk )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_BITX(0x0010, IP_ACTIVE_LOW, IPT_SERVICE | IPF_TOGGLE, "Test", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )
	/*There should be an additional block button for player 2, but I coudn't find it.*/

	PORT_START
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x0001, 0x0001, "Test Switch" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0002, 0x0000, "Counters" )
	PORT_DIPSETTING(      0x0002, "One" )
	PORT_DIPSETTING(      0x0000, "Two" )
	PORT_DIPNAME( 0x007c, 0x007c, DEF_STR( Coinage ))
	PORT_DIPSETTING(      0x007c, "USA-1" )
	PORT_DIPSETTING(      0x003c, "USA-2" )
	PORT_DIPSETTING(      0x005c, "USA-3" )
	PORT_DIPSETTING(      0x001c, "USA-4" )
	PORT_DIPSETTING(      0x006c, "USA-ECA" )
	PORT_DIPSETTING(      0x000c, "USA-Free Play" )
	PORT_DIPSETTING(      0x0074, "German-1" )
	PORT_DIPSETTING(      0x0034, "German-2" )
	PORT_DIPSETTING(      0x0054, "German-3" )
	PORT_DIPSETTING(      0x0014, "German-4" )
	PORT_DIPSETTING(      0x0064, "German-5" )
	PORT_DIPSETTING(      0x0024, "German-ECA" )
	PORT_DIPSETTING(      0x0004, "German-Free Play" )
	PORT_DIPSETTING(      0x0078, "French-1" )
	PORT_DIPSETTING(      0x0038, "French-2" )
	PORT_DIPSETTING(      0x0058, "French-3" )
	PORT_DIPSETTING(      0x0018, "French-4" )
	PORT_DIPSETTING(      0x0068, "French-ECA" )
	PORT_DIPSETTING(      0x0008, "French-Free Play" )
	PORT_DIPNAME( 0x0080, 0x0000, "Coinage Source" )
	PORT_DIPSETTING(      0x0080, "Dipswitch" )
	PORT_DIPSETTING(      0x0000, "CMOS" )
	PORT_DIPNAME( 0x0100, 0x0000, "Skip Post Test")
	PORT_DIPSETTING(      0x0100, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0200, 0x0200, DEF_STR( Unused ))
	PORT_DIPSETTING(      0x0200, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0400, 0x0400, DEF_STR( Unused ))
	PORT_DIPSETTING(      0x0400, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0800, 0x0800, "Comic Book Offer" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0800, DEF_STR( On ))
	PORT_DIPNAME( 0x1000, 0x1000, "Attract Sound" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x1000, DEF_STR( On ))
	PORT_DIPNAME( 0x2000, 0x2000, "Low Blows" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x2000, DEF_STR( On ))
	PORT_DIPNAME( 0x4000, 0x4000, "Blood" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x4000, DEF_STR( On ))
	PORT_DIPNAME( 0x8000, 0x8000, "Violence" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x8000, DEF_STR( On ))
INPUT_PORTS_END


INPUT_PORTS_START( mk2 )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_BITX(0x0010, IP_ACTIVE_LOW, IPT_SERVICE, "Test", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0600, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX(0x0800, IP_ACTIVE_LOW, 0, "Volume Down", KEYCODE_MINUS, IP_JOY_NONE )
	PORT_BITX(0x1000, IP_ACTIVE_LOW, 0, "Volume Up", KEYCODE_EQUALS, IP_JOY_NONE )
	PORT_BIT( 0x6000, IP_ACTIVE_LOW, IPT_UNUSED )
	/*PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON5 | IPF_PLAYER1 )*/
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED ) /*Renamed to unused because without it the game seemed to hold P1 Block down-someone with more experience should check*/

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER1 )
	PORT_BIT( 0x000c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON6 | IPF_PLAYER2 )
	PORT_BIT( 0xffc0, IP_ACTIVE_LOW, IPT_UNUSED )
	/*Note-the real MK2 board has a special cable designed for SF2 cab conversions that has the 2 SF2
          Medium punch/kick buttons as block buttons for MK2. The secondary block button registers in test mode,
          but does not have an indicator light show up. During gameplay, the second block only functions temporarily.
	  You can hold the button, but the character will only take a block position for 1 second. This is correct behavior.*/

	PORT_START
	PORT_DIPNAME( 0x0001, 0x0001, "Test Switch" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0002, 0x0000, "Counters" )
	PORT_DIPSETTING(      0x0002, "One" )
	PORT_DIPSETTING(      0x0000, "Two" )
	PORT_DIPNAME( 0x007c, 0x007c, DEF_STR( Coinage ))
	PORT_DIPSETTING(      0x007c, "USA-1" )
	PORT_DIPSETTING(      0x003c, "USA-2" )
	PORT_DIPSETTING(      0x005c, "USA-3" )
	PORT_DIPSETTING(      0x001c, "USA-4" )
	PORT_DIPSETTING(      0x006c, "USA-ECA" )
	PORT_DIPSETTING(      0x000c, "USA-Free Play" )
	PORT_DIPSETTING(      0x0074, "German-1" )
	PORT_DIPSETTING(      0x0034, "German-2" )
	PORT_DIPSETTING(      0x0054, "German-3" )
	PORT_DIPSETTING(      0x0014, "German-4" )
	PORT_DIPSETTING(      0x0064, "German-5" )
	PORT_DIPSETTING(      0x0024, "German-ECA" )
	PORT_DIPSETTING(      0x0004, "German-Free Play" )
	PORT_DIPSETTING(      0x0078, "French-1" )
	PORT_DIPSETTING(      0x0038, "French-2" )
	PORT_DIPSETTING(      0x0058, "French-3" )
	PORT_DIPSETTING(      0x0018, "French-4" )
	PORT_DIPSETTING(      0x0068, "French-ECA" )
	PORT_DIPSETTING(      0x0008, "French-Free Play" )
	PORT_DIPNAME( 0x0080, 0x0000, "Coinage Source" )
	PORT_DIPSETTING(      0x0080, "Dipswitch" )
	PORT_DIPSETTING(      0x0000, "CMOS" )
	PORT_DIPNAME( 0x0100, 0x0000, "Circuit Boards" )
	PORT_DIPSETTING(      0x0100, "2" )
	PORT_DIPSETTING(      0x0000, "1" )
	PORT_DIPNAME( 0x0200, 0x0000, "Powerup Test" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0200, DEF_STR( On ))
	PORT_DIPNAME( 0x0400, 0x0400, "Bill Validator" )
	PORT_DIPSETTING(      0x0000, "Installed" )
	PORT_DIPSETTING(      0x0400, "Not Present" )
	PORT_DIPNAME( 0x0800, 0x0800, "Comic Book Offer" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0800, DEF_STR( On ))
	PORT_DIPNAME( 0x1000, 0x1000, "Attract Sound" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x1000, DEF_STR( On ))
	PORT_DIPNAME( 0x2000, 0x2000, "Low Blows" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x2000, DEF_STR( On ))
	PORT_DIPNAME( 0x4000, 0x4000, "Blood" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x4000, DEF_STR( On ))
	PORT_DIPNAME( 0x8000, 0x8000, "Violence" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x8000, DEF_STR( On ))
INPUT_PORTS_END


INPUT_PORTS_START( jdreddp )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER1 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER2 )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_BITX(0x0010, IP_ACTIVE_LOW, IPT_SERVICE | IPF_TOGGLE, "Test", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BITX(0x0800, IP_ACTIVE_LOW, 0, "Volume Down", KEYCODE_MINUS, IP_JOY_NONE )
	PORT_BITX(0x1000, IP_ACTIVE_LOW, 0, "Volume Up", KEYCODE_EQUALS, IP_JOY_NONE )
	PORT_BIT( 0xe000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_BUTTON4 | IPF_PLAYER3 )
	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	    /* DS1 */
	PORT_DIPNAME( 0x0001, 0x0001, "Test Switch" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0002, 0x0002, DEF_STR( Unused )) /*listed as 'Powerup Test' in service mode, does nothing*/
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0004, 0x0004, DEF_STR( Unused ))
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0008, 0x0008, DEF_STR( Unused ))
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0010, 0x0010, DEF_STR( Unused ))
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0020, 0x0020, "Blood" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0020, DEF_STR( On ))
	PORT_DIPNAME( 0x0040, 0x0040, "Validator" )
	PORT_DIPSETTING(      0x0000, "Installed" )
	PORT_DIPSETTING(      0x0040, "None" )
	PORT_DIPNAME( 0x0080, 0x0080, "Freeze" ) /*listed as 2/3 player in service mode*/
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0300, 0x0000, "Coin Counters" )
/*	PORT_DIPSETTING(      0x0300, "1 Counter, 1 count/coin" )*/
	PORT_DIPSETTING(      0x0200, "1 Counter, Totalizing" )
	PORT_DIPSETTING(      0x0100, "2 Counters, 1 count/coin" )
	PORT_DIPSETTING(      0x0000, "1 Counter, 1 count/coin" )
	PORT_DIPNAME( 0x0c00, 0x0c00, "Country" )
	PORT_DIPSETTING(      0x0c00, "USA" )
	PORT_DIPSETTING(      0x0800, "French" )
	PORT_DIPSETTING(      0x0400, "German" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Unused ))
	PORT_DIPNAME( 0x7000, 0x5000, DEF_STR( Coinage ))
	PORT_DIPSETTING(      0x7000, "1" )
	PORT_DIPSETTING(      0x3000, "2" )
	PORT_DIPSETTING(      0x5000, "3" ) /* the game reads this as Skip Power up Test and Coinage 3*/
	PORT_DIPSETTING(      0x1000, "4" )
	PORT_DIPSETTING(      0x6000, "ECA" )
/*	PORT_DIPSETTING(      0x4000, DEF_STR( Unused ))*/
/*	PORT_DIPSETTING(      0x2000, DEF_STR( Unused ))*/
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ))
	PORT_DIPNAME( 0x8000, 0x0000, "Coinage Source" )
	PORT_DIPSETTING(      0x8000, "Dipswitch" )
	PORT_DIPSETTING(      0x0000, "CMOS" )
INPUT_PORTS_END


INPUT_PORTS_START( nbajam )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_BITX(0x0010, IP_ACTIVE_LOW, IPT_SERVICE | IPF_TOGGLE, "Test", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BITX(0x0800, IP_ACTIVE_LOW, 0, "Volume Down", KEYCODE_MINUS, IP_JOY_NONE )
	PORT_BITX(0x1000, IP_ACTIVE_LOW, 0, "Volume Up", KEYCODE_EQUALS, IP_JOY_NONE )
	PORT_BIT( 0xe000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	    /* DS1 */
	PORT_DIPNAME( 0x0001, 0x0001, "Test Switch" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0002, 0x0000, "Powerup Test" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0002, DEF_STR( On ))
	PORT_DIPNAME( 0x0004, 0x0004, DEF_STR( Unused ))
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0008, 0x0008, DEF_STR( Unused ))
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0010, 0x0010, DEF_STR( Unused ))
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0020, 0x0020, "Video" )
	PORT_DIPSETTING(      0x0000, "Skip" )
	PORT_DIPSETTING(      0x0020, "Show" )
	PORT_DIPNAME( 0x0040, 0x0040, "Validator" )
	PORT_DIPSETTING(      0x0000, "Installed" )
	PORT_DIPSETTING(      0x0040, "Not Present" )
	PORT_DIPNAME( 0x0080, 0x0080, "Players" )
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_DIPSETTING(      0x0080, "4" )
	PORT_DIPNAME( 0x0300, 0x0300, "Coin Counters" )
	PORT_DIPSETTING(      0x0300, "1 Counter, 1 count/coin" )
	PORT_DIPSETTING(      0x0200, "1 Counter, Totalizing" )
	PORT_DIPSETTING(      0x0100, "2 Counters, 1 count/coin" )
/*	PORT_DIPSETTING(      0x0000, "1 Counter, 1 count/coin" )*/
	PORT_DIPNAME( 0x0c00, 0x0c00, "Country" )
	PORT_DIPSETTING(      0x0c00, "USA" )
	PORT_DIPSETTING(      0x0800, "French" )
	PORT_DIPSETTING(      0x0400, "German" )
/*	PORT_DIPSETTING(      0x0000, DEF_STR( Unused ))*/
	PORT_DIPNAME( 0x7000, 0x7000, DEF_STR( Coinage ))
	PORT_DIPSETTING(      0x7000, "1" )
	PORT_DIPSETTING(      0x3000, "2" )
	PORT_DIPSETTING(      0x5000, "3" )
	PORT_DIPSETTING(      0x1000, "4" )
	PORT_DIPSETTING(      0x6000, "ECA" )
/*	PORT_DIPSETTING(      0x4000, DEF_STR( Unused ))*/
/*	PORT_DIPSETTING(      0x2000, DEF_STR( Unused ))*/
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ))
	PORT_DIPNAME( 0x8000, 0x0000, "Coinage Source" )
	PORT_DIPSETTING(      0x8000, "Dipswitch" )
	PORT_DIPSETTING(      0x0000, "CMOS" )
INPUT_PORTS_END


INPUT_PORTS_START( nbajamte )
	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER1 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER1 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER2 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER2 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_TILT ) /* Slam Switch */
	PORT_BITX(0x0010, IP_ACTIVE_LOW, IPT_SERVICE, "Test", KEYCODE_F2, IP_JOY_NONE )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BITX(0x0800, IP_ACTIVE_LOW, 0, "Volume Down", KEYCODE_MINUS, IP_JOY_NONE )
	PORT_BITX(0x1000, IP_ACTIVE_LOW, 0, "Volume Up", KEYCODE_EQUALS, IP_JOY_NONE )
	PORT_BIT( 0xe000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER3 | IPF_8WAY )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER3 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER3 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER3 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER4 | IPF_8WAY )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_PLAYER4 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_PLAYER4 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_PLAYER4 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	    /* DS1 */
	PORT_DIPNAME( 0x0001, 0x0001, "Test Switch" )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0002, 0x0000, "Powerup Test" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0002, DEF_STR( On ))
	PORT_DIPNAME( 0x0004, 0x0004, DEF_STR( Unused ))
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0008, 0x0008, DEF_STR( Unused ))
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0010, 0x0010, DEF_STR( Unused ))
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0000, DEF_STR( On ))
	PORT_DIPNAME( 0x0020, 0x0020, DEF_STR( Unused ))
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ))
	PORT_DIPSETTING(      0x0020, DEF_STR( On ))
	PORT_DIPNAME( 0x0040, 0x0040, "Validator" )
	PORT_DIPSETTING(      0x0000, "Installed" )
	PORT_DIPSETTING(      0x0040, "Not Present" )
	PORT_DIPNAME( 0x0080, 0x0080, "Players" )
	PORT_DIPSETTING(      0x0000, "2" )
	PORT_DIPSETTING(      0x0080, "4" )
	PORT_DIPNAME( 0x0300, 0x0300, "Coin Counters" )
	PORT_DIPSETTING(      0x0300, "1 Counter, 1 count/coin" )
	PORT_DIPSETTING(      0x0200, "1 Counter, Totalizing" )
	PORT_DIPSETTING(      0x0100, "2 Counters, 1 count/coin" )
/*	PORT_DIPSETTING(      0x0000, "1 Counter, 1 count/coin" )*/
	PORT_DIPNAME( 0x0c00, 0x0c00, "Country" )
	PORT_DIPSETTING(      0x0c00, "USA" )
	PORT_DIPSETTING(      0x0800, "French" )
	PORT_DIPSETTING(      0x0400, "German" )
/*	PORT_DIPSETTING(      0x0000, DEF_STR( Unused ))*/
	PORT_DIPNAME( 0x7000, 0x7000, DEF_STR( Coinage ))
	PORT_DIPSETTING(      0x7000, "1" )
	PORT_DIPSETTING(      0x3000, "2" )
	PORT_DIPSETTING(      0x5000, "3" )
	PORT_DIPSETTING(      0x1000, "4" )
	PORT_DIPSETTING(      0x6000, "ECA" )
/*	PORT_DIPSETTING(      0x4000, DEF_STR( Unused ))*/
/*	PORT_DIPSETTING(      0x2000, DEF_STR( Unused ))*/
	PORT_DIPSETTING(      0x0000, DEF_STR( Free_Play ))
	PORT_DIPNAME( 0x8000, 0x0000, "Coinage Source" )
	PORT_DIPSETTING(      0x8000, "Dipswitch" )
	PORT_DIPSETTING(      0x0000, "CMOS" )
INPUT_PORTS_END


/*************************************
 *
 *	34010 configuration
 *
 *************************************/

static struct tms34010_config cpu_config =
{
	0,								/* halt on reset */
	NULL,							/* generate interrupt */
	midtunit_to_shiftreg,			/* write to shiftreg function */
	midtunit_from_shiftreg,			/* read from shiftreg function */
	0,								/* display address changed */
	0								/* display interrupt callback */
};



/*************************************
 *
 *	Machine drivers
 *
 *************************************/

/*
	all games use identical visible areas and VBLANK timing
	based on these video params:

	          VERTICAL                   HORIZONTAL
	mk:       0014-0112 / 0120 (254)     002D-00F5 / 00FC (400)
	mk2:      0014-0112 / 0120 (254)     002D-00F5 / 00FC (400)
	jdredd:   0014-0112 / 0120 (254)     0032-00FA / 00FC (400)
	nbajam:   0014-0112 / 0120 (254)     0032-00FA / 00FC (400)
*/

static MACHINE_DRIVER_START( tunit_core )

	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", TMS34010, 50000000/TMS34010_CLOCK_DIVIDER)
	MDRV_CPU_CONFIG(cpu_config)
	MDRV_CPU_MEMORY(readmem,writemem)

	MDRV_FRAMES_PER_SECOND(MKLA5_FPS)
	MDRV_VBLANK_DURATION((1000000 * (288 - 254)) / (MKLA5_FPS * 288))
	MDRV_MACHINE_INIT(midtunit)
	MDRV_NVRAM_HANDLER(generic_0fill)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(400, 256)
	MDRV_VISIBLE_AREA(0, 399, 0, 253)
	MDRV_PALETTE_LENGTH(32768)

	MDRV_VIDEO_START(midtunit)
	MDRV_VIDEO_UPDATE(midtunit)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( tunit_adpcm )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(tunit_core)
	MDRV_IMPORT_FROM(williams_adpcm_sound)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( mk )
	mk_playing_mortal_kombat_t = true; /* --> Let the sound hardware know we are playing Mortal Kombat.*/
	
	/* basic machine hardware */
	MDRV_IMPORT_FROM(tunit_core)
	MDRV_IMPORT_FROM(williams_adpcm_sound)

	/* Lets add our Mortal Kombat music sample packs.*/
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(SAMPLES, mk_samples_tunit)	
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( nbajam )
	nba_jam_playing = true; /* --> Let the sound hardware know we are playing NBA Jam.*/
	nba_jam_title_screen = false;
	nba_jam_select_screen = false;
	nba_jam_intermission = false;
	nba_jam_in_game = false;
	nba_jam_boot_up	= true;
	nba_jam_playing_title_music = false;
	
	m_nba_last_offset = 0;
	m_nba_start_counter = 0;
	
	/* basic machine hardware */
	MDRV_IMPORT_FROM(tunit_core)
	MDRV_IMPORT_FROM(williams_adpcm_sound)

	/* Lets add our NBA Jam music sample packs.*/
	MDRV_SOUND_ATTRIBUTES(SOUND_SUPPORTS_STEREO)
	MDRV_SOUND_ADD(SAMPLES, nba_jam_samples_tunit)
MACHINE_DRIVER_END


static MACHINE_DRIVER_START( tunit_dcs )

	/* basic machine hardware */
	MDRV_IMPORT_FROM(tunit_core)
	MDRV_IMPORT_FROM(dcs_audio)
MACHINE_DRIVER_END



/*************************************
 *
 *	ROM definitions
 *
 *************************************/

ROM_START( mk )
	ROM_REGION( 0x10, REGION_CPU1, 0 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2, 0 ) 	/* sound CPU */
	ROM_LOAD( "mks-u3.rom", 0x10000, 0x40000, CRC(c615844c) SHA1(5732f9053a5f73b0cc3b0166d7dc4430829d5bc7) )

	ROM_REGION( 0xc0000, REGION_SOUND1, 0 )	/* ADPCM */
	ROM_LOAD( "mks-u12.rom", 0x00000, 0x40000, CRC(258bd7f9) SHA1(463890b23f17350fb9b8a85897b0777c45bc2d54) )
	ROM_LOAD( "mks-u13.rom", 0x40000, 0x40000, CRC(7b7ec3b6) SHA1(6eec1b90d4a4855f34a7ebfbf93f3358d5627db4) )

	ROM_REGION16_LE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )	/* 34010 code */
	ROM_LOAD16_BYTE( "mkt-uj12.bin", 0x00000, 0x80000, CRC(f4990bf2) SHA1(796ec84d37c8d20ca36d6439c14dee626fb8481e) )
	ROM_LOAD16_BYTE( "mkt-ug12.bin", 0x00001, 0x80000, CRC(b06aeac1) SHA1(f66655eeab67c8cf5e496ae42dbae54d6400586f) )

	ROM_REGION( 0xc00000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "mkt-ug14.bin", 0x000000, 0x80000, CRC(9e00834e) SHA1(2b97b63f52ba1dba6af6ae56c223519a52b2ab9d) )
	ROM_LOAD( "mkt-ug16.bin", 0x080000, 0x80000, CRC(52c9d1e5) SHA1(7b1880fca0a11544782b70365c7dd96381ac48e7) )
	ROM_LOAD( "mkt-ug17.bin", 0x100000, 0x80000, CRC(e34fe253) SHA1(6b010bee795c1770297c9557ded1fe83425857f2) )

	ROM_LOAD( "mkt-uj14.bin", 0x300000, 0x80000, CRC(f4b0aaa7) SHA1(4cc6ee34c89e3cde325ad24b29511f70ae6a5a72) )
	ROM_LOAD( "mkt-uj16.bin", 0x380000, 0x80000, CRC(c94c58cf) SHA1(974d75667eee779497325d5be8df937f15417edf) )
	ROM_LOAD( "mkt-uj17.bin", 0x400000, 0x80000, CRC(a56e12f5) SHA1(5db637c4710990cd06bb0069714b19621532e431) )

	ROM_LOAD( "mkt-ug19.bin", 0x600000, 0x80000, CRC(2d8c7ba1) SHA1(f891d6eb618dbf3e77f02e0f93da216e20571905) )
	ROM_LOAD( "mkt-ug20.bin", 0x680000, 0x80000, CRC(2f7e55d3) SHA1(bda6892ee6fcb46959e4d0892bbe7d9fc6072dd3) )
	ROM_LOAD( "mkt-ug22.bin", 0x700000, 0x80000, CRC(b537bb4e) SHA1(05a447deee2e89b49bdb3ca2161a021d7ec5f11e) )

	ROM_LOAD( "mkt-uj19.bin", 0x900000, 0x80000, CRC(33b9b7a4) SHA1(e8ceca4c049e1f55d480a03ff793b595bd04d344) )
	ROM_LOAD( "mkt-uj20.bin", 0x980000, 0x80000, CRC(eae96df0) SHA1(b40532312ba61e4065abfd733dd0c93eecad48e9) )
	ROM_LOAD( "mkt-uj22.bin", 0xa00000, 0x80000, CRC(5e12523b) SHA1(468f93ef9bb6addb45c1c939d24b6511f255426a) )
ROM_END


ROM_START( mkr4 )
	ROM_REGION( 0x10, REGION_CPU1, 0 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2, 0 ) 	/* sound CPU */
	ROM_LOAD( "mks-u3.rom", 0x10000, 0x40000, CRC(c615844c) SHA1(5732f9053a5f73b0cc3b0166d7dc4430829d5bc7) )

	ROM_REGION( 0xc0000, REGION_SOUND1, 0 )	/* ADPCM */
	ROM_LOAD( "mks-u12.rom", 0x00000, 0x40000, CRC(258bd7f9) SHA1(463890b23f17350fb9b8a85897b0777c45bc2d54) )
	ROM_LOAD( "mks-u13.rom", 0x40000, 0x40000, CRC(7b7ec3b6) SHA1(6eec1b90d4a4855f34a7ebfbf93f3358d5627db4) )

	ROM_REGION16_LE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )	/* 34010 code */
	ROM_LOAD16_BYTE( "mkr4uj12.bin", 0x00000, 0x80000, CRC(a1b6635a) SHA1(22d396cc9c1e3a14cb01d196de6d3e864f7afc55) )
	ROM_LOAD16_BYTE( "mkr4ug12.bin", 0x00001, 0x80000, CRC(aa94f7ea) SHA1(bd8957bf52f73b49767cc78fec84ed1109a37701) )

	ROM_REGION( 0xc00000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "mkt-ug14.bin", 0x000000, 0x80000, CRC(9e00834e) SHA1(2b97b63f52ba1dba6af6ae56c223519a52b2ab9d) )
	ROM_LOAD( "mkt-ug16.bin", 0x080000, 0x80000, CRC(52c9d1e5) SHA1(7b1880fca0a11544782b70365c7dd96381ac48e7) )
	ROM_LOAD( "mkt-ug17.bin", 0x100000, 0x80000, CRC(e34fe253) SHA1(6b010bee795c1770297c9557ded1fe83425857f2) )

	ROM_LOAD( "mkt-uj14.bin", 0x300000, 0x80000, CRC(f4b0aaa7) SHA1(4cc6ee34c89e3cde325ad24b29511f70ae6a5a72) )
	ROM_LOAD( "mkt-uj16.bin", 0x380000, 0x80000, CRC(c94c58cf) SHA1(974d75667eee779497325d5be8df937f15417edf) )
	ROM_LOAD( "mkt-uj17.bin", 0x400000, 0x80000, CRC(a56e12f5) SHA1(5db637c4710990cd06bb0069714b19621532e431) )

	ROM_LOAD( "mkt-ug19.bin", 0x600000, 0x80000, CRC(2d8c7ba1) SHA1(f891d6eb618dbf3e77f02e0f93da216e20571905) )
	ROM_LOAD( "mkt-ug20.bin", 0x680000, 0x80000, CRC(2f7e55d3) SHA1(bda6892ee6fcb46959e4d0892bbe7d9fc6072dd3) )
	ROM_LOAD( "mkt-ug22.bin", 0x700000, 0x80000, CRC(b537bb4e) SHA1(05a447deee2e89b49bdb3ca2161a021d7ec5f11e) )

	ROM_LOAD( "mkt-uj19.bin", 0x900000, 0x80000, CRC(33b9b7a4) SHA1(e8ceca4c049e1f55d480a03ff793b595bd04d344) )
	ROM_LOAD( "mkt-uj20.bin", 0x980000, 0x80000, CRC(eae96df0) SHA1(b40532312ba61e4065abfd733dd0c93eecad48e9) )
	ROM_LOAD( "mkt-uj22.bin", 0xa00000, 0x80000, CRC(5e12523b) SHA1(468f93ef9bb6addb45c1c939d24b6511f255426a) )
ROM_END


ROM_START( mk2 )
	ROM_REGION( 0x10, REGION_CPU1, 0 )		/* 34010 dummy region */

	ROM_REGION( ADSP2100_SIZE + 0x800000, REGION_CPU2, 0 )	/* ADSP-2105 data */
	ROM_LOAD( "su2.l1", ADSP2100_SIZE + 0x000000, 0x80000, CRC(5f23d71d) SHA1(54c2afef243759e0f3dbe2907edbc4302f5c8bad) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x080000, 0x80000 )
	ROM_LOAD( "su3.l1", ADSP2100_SIZE + 0x100000, 0x80000, CRC(d6d92bf9) SHA1(397351c6b707f2595e36360471015f9fa494e894) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x180000, 0x80000 )
	ROM_LOAD( "su4.l1", ADSP2100_SIZE + 0x200000, 0x80000, CRC(eebc8e0f) SHA1(705ab63ff7672a4857d546afda6dca4973cce1ad) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x280000, 0x80000 )
	ROM_LOAD( "su5.l1", ADSP2100_SIZE + 0x300000, 0x80000, CRC(2b0b7961) SHA1(1cdc64aab74d14afbd8c3531e3d0bd49271a281f) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x380000, 0x80000 )
	ROM_LOAD( "su6.l1", ADSP2100_SIZE + 0x400000, 0x80000, CRC(f694b27f) SHA1(d43e38a124665f49ebb4ffc5a55e8f19a1a64686) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x480000, 0x80000 )
	ROM_LOAD( "su7.l1", ADSP2100_SIZE + 0x500000, 0x80000, CRC(20387e0a) SHA1(505d05173b2a1f1ee3ebc2898ccd3a95c98dd04a) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x580000, 0x80000 )
	/* su8 and su9 are unpopulated */

	ROM_REGION16_LE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )	/* 34010 code */
	ROM_LOAD16_BYTE( "uj12.l31", 0x00000, 0x80000, CRC(cf100a75) SHA1(c5cf739fdb08e311f47794eb93a8d34d4bc11cde) )
	ROM_LOAD16_BYTE( "ug12.l31", 0x00001, 0x80000, CRC(582c7dfd) SHA1(f32bd1213ce70f74caa97a2047815cf4baee56b5) )

	ROM_REGION( 0xc00000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "ug14-vid", 0x000000, 0x100000, CRC(01e73af6) SHA1(6598cfd704cc92a7f358a0e1f1c973ab79dcc493) )
	ROM_LOAD( "ug16-vid", 0x100000, 0x100000, CRC(8ba6ae18) SHA1(465fe907de4a1e502180c4e41642998dd3abc8e6) )
	ROM_LOAD( "ug17-vid", 0x200000, 0x100000, CRC(937d8620) SHA1(8b9f80a460b124a747a6d1495b53f01f580e28f1) )

	ROM_LOAD( "uj14-vid", 0x300000, 0x100000, CRC(d4985cbb) SHA1(367865da7efae38d83de3c0868d02a705177ae63) )
	ROM_LOAD( "uj16-vid", 0x400000, 0x100000, CRC(39d885b4) SHA1(2251826d247c3c6df421124718401fb35a672f83) )
	ROM_LOAD( "uj17-vid", 0x500000, 0x100000, CRC(218de160) SHA1(87aea173720d2a33d8183903f4fe8ba1d47e3348) )

	ROM_LOAD( "ug19-vid", 0x600000, 0x100000, CRC(fec137be) SHA1(f11ecb8a7993f5c4f4449564b4911f69bd6e9bf8) )
	ROM_LOAD( "ug20-vid", 0x700000, 0x100000, CRC(809118c1) SHA1(86153e648834c749e34573151cd4fee403a81962) )
	ROM_LOAD( "ug22-vid", 0x800000, 0x100000, CRC(154d53b1) SHA1(58ff0aa59101f40a9a3b5fbae1c904d0b0b31612) )

	ROM_LOAD( "uj19-vid", 0x900000, 0x100000, CRC(2d763156) SHA1(06536006da49ab5fb6b75b25f801b83fad000ff5) )
	ROM_LOAD( "uj20-vid", 0xa00000, 0x100000, CRC(b96824f0) SHA1(d42b122f9a57da330192abc7e5f97abc4065d718) )
	ROM_LOAD( "uj22-vid", 0xb00000, 0x100000, CRC(8891d785) SHA1(fd460df1ef8f4306ea42f7dc41488a80fd2c8f53) )
ROM_END


ROM_START( mk2r32 )
	ROM_REGION( 0x10, REGION_CPU1, 0 )		/* 34010 dummy region */

	ROM_REGION( ADSP2100_SIZE + 0x800000, REGION_CPU2, 0 )	/* ADSP-2105 data */
	ROM_LOAD( "su2.l1", ADSP2100_SIZE + 0x000000, 0x80000, CRC(5f23d71d) SHA1(54c2afef243759e0f3dbe2907edbc4302f5c8bad) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x080000, 0x80000 )
	ROM_LOAD( "su3.l1", ADSP2100_SIZE + 0x100000, 0x80000, CRC(d6d92bf9) SHA1(397351c6b707f2595e36360471015f9fa494e894) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x180000, 0x80000 )
	ROM_LOAD( "su4.l1", ADSP2100_SIZE + 0x200000, 0x80000, CRC(eebc8e0f) SHA1(705ab63ff7672a4857d546afda6dca4973cce1ad) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x280000, 0x80000 )
	ROM_LOAD( "su5.l1", ADSP2100_SIZE + 0x300000, 0x80000, CRC(2b0b7961) SHA1(1cdc64aab74d14afbd8c3531e3d0bd49271a281f) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x380000, 0x80000 )
	ROM_LOAD( "su6.l1", ADSP2100_SIZE + 0x400000, 0x80000, CRC(f694b27f) SHA1(d43e38a124665f49ebb4ffc5a55e8f19a1a64686) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x480000, 0x80000 )
	ROM_LOAD( "su7.l1", ADSP2100_SIZE + 0x500000, 0x80000, CRC(20387e0a) SHA1(505d05173b2a1f1ee3ebc2898ccd3a95c98dd04a) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x580000, 0x80000 )
	/* su8 and su9 are unpopulated */

	ROM_REGION16_LE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )	/* 34010 code */
	ROM_LOAD16_BYTE( "uj12.l32", 0x00000, 0x80000, CRC(43f773a6) SHA1(a97b75bac2793f99738abcbd4054f2b860aff574) )
	ROM_LOAD16_BYTE( "ug12.l32", 0x00001, 0x80000, CRC(dcde9619) SHA1(72b39bd68eff5938cd87d3388074172a07bda816) )

	ROM_REGION( 0xc00000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "ug14-vid", 0x000000, 0x100000, CRC(01e73af6) SHA1(6598cfd704cc92a7f358a0e1f1c973ab79dcc493) )
	ROM_LOAD( "ug16-vid", 0x100000, 0x100000, CRC(8ba6ae18) SHA1(465fe907de4a1e502180c4e41642998dd3abc8e6) )
	ROM_LOAD( "ug17-vid", 0x200000, 0x100000, CRC(937d8620) SHA1(8b9f80a460b124a747a6d1495b53f01f580e28f1) )

	ROM_LOAD( "uj14-vid", 0x300000, 0x100000, CRC(d4985cbb) SHA1(367865da7efae38d83de3c0868d02a705177ae63) )
	ROM_LOAD( "uj16-vid", 0x400000, 0x100000, CRC(39d885b4) SHA1(2251826d247c3c6df421124718401fb35a672f83) )
	ROM_LOAD( "uj17-vid", 0x500000, 0x100000, CRC(218de160) SHA1(87aea173720d2a33d8183903f4fe8ba1d47e3348) )

	ROM_LOAD( "ug19-vid", 0x600000, 0x100000, CRC(fec137be) SHA1(f11ecb8a7993f5c4f4449564b4911f69bd6e9bf8) )
	ROM_LOAD( "ug20-vid", 0x700000, 0x100000, CRC(809118c1) SHA1(86153e648834c749e34573151cd4fee403a81962) )
	ROM_LOAD( "ug22-vid", 0x800000, 0x100000, CRC(154d53b1) SHA1(58ff0aa59101f40a9a3b5fbae1c904d0b0b31612) )

	ROM_LOAD( "uj19-vid", 0x900000, 0x100000, CRC(2d763156) SHA1(06536006da49ab5fb6b75b25f801b83fad000ff5) )
	ROM_LOAD( "uj20-vid", 0xa00000, 0x100000, CRC(b96824f0) SHA1(d42b122f9a57da330192abc7e5f97abc4065d718) )
	ROM_LOAD( "uj22-vid", 0xb00000, 0x100000, CRC(8891d785) SHA1(fd460df1ef8f4306ea42f7dc41488a80fd2c8f53) )
ROM_END


ROM_START( mk2r21 )
	ROM_REGION( 0x10, REGION_CPU1, 0 )		/* 34010 dummy region */

	ROM_REGION( ADSP2100_SIZE + 0x800000, REGION_CPU2, 0 )	/* ADSP-2105 data */
	ROM_LOAD( "su2.l1", ADSP2100_SIZE + 0x000000, 0x80000, CRC(5f23d71d) SHA1(54c2afef243759e0f3dbe2907edbc4302f5c8bad) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x080000, 0x80000 )
	ROM_LOAD( "su3.l1", ADSP2100_SIZE + 0x100000, 0x80000, CRC(d6d92bf9) SHA1(397351c6b707f2595e36360471015f9fa494e894) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x180000, 0x80000 )
	ROM_LOAD( "su4.l1", ADSP2100_SIZE + 0x200000, 0x80000, CRC(eebc8e0f) SHA1(705ab63ff7672a4857d546afda6dca4973cce1ad) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x280000, 0x80000 )
	ROM_LOAD( "su5.l1", ADSP2100_SIZE + 0x300000, 0x80000, CRC(2b0b7961) SHA1(1cdc64aab74d14afbd8c3531e3d0bd49271a281f) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x380000, 0x80000 )
	ROM_LOAD( "su6.l1", ADSP2100_SIZE + 0x400000, 0x80000, CRC(f694b27f) SHA1(d43e38a124665f49ebb4ffc5a55e8f19a1a64686) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x480000, 0x80000 )
	ROM_LOAD( "su7.l1", ADSP2100_SIZE + 0x500000, 0x80000, CRC(20387e0a) SHA1(505d05173b2a1f1ee3ebc2898ccd3a95c98dd04a) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x580000, 0x80000 )
	/* su8 and su9 are unpopulated */

	ROM_REGION16_LE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )	/* 34010 code */
	ROM_LOAD16_BYTE( "uj12.121", 0x00000, 0x80000, CRC(d6a35699) SHA1(17feee7886108d6f946bf04669479d35c2edac76) )
	ROM_LOAD16_BYTE( "ug12.121", 0x00001, 0x80000, CRC(aeb703ff) SHA1(e94cd9e6feb45e3de85661ca12452aff6e14d3ae) )

	ROM_REGION( 0xc00000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "ug14-vid", 0x000000, 0x100000, CRC(01e73af6) SHA1(6598cfd704cc92a7f358a0e1f1c973ab79dcc493) )
	ROM_LOAD( "ug16-vid", 0x100000, 0x100000, CRC(8ba6ae18) SHA1(465fe907de4a1e502180c4e41642998dd3abc8e6) )
	ROM_LOAD( "ug17-vid", 0x200000, 0x100000, CRC(937d8620) SHA1(8b9f80a460b124a747a6d1495b53f01f580e28f1) )

	ROM_LOAD( "uj14-vid", 0x300000, 0x100000, CRC(d4985cbb) SHA1(367865da7efae38d83de3c0868d02a705177ae63) )
	ROM_LOAD( "uj16-vid", 0x400000, 0x100000, CRC(39d885b4) SHA1(2251826d247c3c6df421124718401fb35a672f83) )
	ROM_LOAD( "uj17-vid", 0x500000, 0x100000, CRC(218de160) SHA1(87aea173720d2a33d8183903f4fe8ba1d47e3348) )

	ROM_LOAD( "ug19-vid", 0x600000, 0x100000, CRC(fec137be) SHA1(f11ecb8a7993f5c4f4449564b4911f69bd6e9bf8) )
	ROM_LOAD( "ug20-vid", 0x700000, 0x100000, CRC(809118c1) SHA1(86153e648834c749e34573151cd4fee403a81962) )
	ROM_LOAD( "ug22-vid", 0x800000, 0x100000, CRC(154d53b1) SHA1(58ff0aa59101f40a9a3b5fbae1c904d0b0b31612) )

	ROM_LOAD( "uj19-vid", 0x900000, 0x100000, CRC(2d763156) SHA1(06536006da49ab5fb6b75b25f801b83fad000ff5) )
	ROM_LOAD( "uj20-vid", 0xa00000, 0x100000, CRC(b96824f0) SHA1(d42b122f9a57da330192abc7e5f97abc4065d718) )
	ROM_LOAD( "uj22-vid", 0xb00000, 0x100000, CRC(8891d785) SHA1(fd460df1ef8f4306ea42f7dc41488a80fd2c8f53) )
ROM_END


ROM_START( mk2r14 )
	ROM_REGION( 0x10, REGION_CPU1, 0 )		/* 34010 dummy region */

	ROM_REGION( ADSP2100_SIZE + 0x800000, REGION_CPU2, 0 )	/* ADSP-2105 data */
	ROM_LOAD( "su2.l1", ADSP2100_SIZE + 0x000000, 0x80000, CRC(5f23d71d) SHA1(54c2afef243759e0f3dbe2907edbc4302f5c8bad) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x080000, 0x80000 )
	ROM_LOAD( "su3.l1", ADSP2100_SIZE + 0x100000, 0x80000, CRC(d6d92bf9) SHA1(397351c6b707f2595e36360471015f9fa494e894) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x180000, 0x80000 )
	ROM_LOAD( "su4.l1", ADSP2100_SIZE + 0x200000, 0x80000, CRC(eebc8e0f) SHA1(705ab63ff7672a4857d546afda6dca4973cce1ad) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x280000, 0x80000 )
	ROM_LOAD( "su5.l1", ADSP2100_SIZE + 0x300000, 0x80000, CRC(2b0b7961) SHA1(1cdc64aab74d14afbd8c3531e3d0bd49271a281f) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x380000, 0x80000 )
	ROM_LOAD( "su6.l1", ADSP2100_SIZE + 0x400000, 0x80000, CRC(f694b27f) SHA1(d43e38a124665f49ebb4ffc5a55e8f19a1a64686) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x480000, 0x80000 )
	ROM_LOAD( "su7.l1", ADSP2100_SIZE + 0x500000, 0x80000, CRC(20387e0a) SHA1(505d05173b2a1f1ee3ebc2898ccd3a95c98dd04a) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x580000, 0x80000 )
	/* su8 and su9 are unpopulated */

	ROM_REGION16_LE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )	/* 34010 code */
	ROM_LOAD16_BYTE( "uj12.l14", 0x00000, 0x80000, CRC(6d43bc6d) SHA1(578ea9c60fa94689d6ae583b86769cd56d8db311) )
	ROM_LOAD16_BYTE( "ug12.l14", 0x00001, 0x80000, CRC(42b0da21) SHA1(94ef25b04c35b4c26b692c2c3c5f68ba747bef49) )

	ROM_REGION( 0xc00000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "ug14-vid", 0x000000, 0x100000, CRC(01e73af6) SHA1(6598cfd704cc92a7f358a0e1f1c973ab79dcc493) )
	ROM_LOAD( "ug16-vid", 0x100000, 0x100000, CRC(8ba6ae18) SHA1(465fe907de4a1e502180c4e41642998dd3abc8e6) )
	ROM_LOAD( "ug17-vid", 0x200000, 0x100000, CRC(937d8620) SHA1(8b9f80a460b124a747a6d1495b53f01f580e28f1) )

	ROM_LOAD( "uj14-vid", 0x300000, 0x100000, CRC(d4985cbb) SHA1(367865da7efae38d83de3c0868d02a705177ae63) )
	ROM_LOAD( "uj16-vid", 0x400000, 0x100000, CRC(39d885b4) SHA1(2251826d247c3c6df421124718401fb35a672f83) )
	ROM_LOAD( "uj17-vid", 0x500000, 0x100000, CRC(218de160) SHA1(87aea173720d2a33d8183903f4fe8ba1d47e3348) )

	ROM_LOAD( "ug19-vid", 0x600000, 0x100000, CRC(fec137be) SHA1(f11ecb8a7993f5c4f4449564b4911f69bd6e9bf8) )
	ROM_LOAD( "ug20-vid", 0x700000, 0x100000, CRC(809118c1) SHA1(86153e648834c749e34573151cd4fee403a81962) )
	ROM_LOAD( "ug22-vid", 0x800000, 0x100000, CRC(154d53b1) SHA1(58ff0aa59101f40a9a3b5fbae1c904d0b0b31612) )

	ROM_LOAD( "uj19-vid", 0x900000, 0x100000, CRC(2d763156) SHA1(06536006da49ab5fb6b75b25f801b83fad000ff5) )
	ROM_LOAD( "uj20-vid", 0xa00000, 0x100000, CRC(b96824f0) SHA1(d42b122f9a57da330192abc7e5f97abc4065d718) )
	ROM_LOAD( "uj22-vid", 0xb00000, 0x100000, CRC(8891d785) SHA1(fd460df1ef8f4306ea42f7dc41488a80fd2c8f53) )
ROM_END


ROM_START( mk2r42 )
	ROM_REGION( 0x10, REGION_CPU1, 0 )		/* 34010 dummy region */

	ROM_REGION( ADSP2100_SIZE + 0x800000, REGION_CPU2, 0 )	/* ADSP-2105 data */
	ROM_LOAD( "su2.l1", ADSP2100_SIZE + 0x000000, 0x80000, CRC(5f23d71d) SHA1(54c2afef243759e0f3dbe2907edbc4302f5c8bad) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x080000, 0x80000 )
	ROM_LOAD( "su3.l1", ADSP2100_SIZE + 0x100000, 0x80000, CRC(d6d92bf9) SHA1(397351c6b707f2595e36360471015f9fa494e894) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x180000, 0x80000 )
	ROM_LOAD( "su4.l1", ADSP2100_SIZE + 0x200000, 0x80000, CRC(eebc8e0f) SHA1(705ab63ff7672a4857d546afda6dca4973cce1ad) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x280000, 0x80000 )
	ROM_LOAD( "su5.l1", ADSP2100_SIZE + 0x300000, 0x80000, CRC(2b0b7961) SHA1(1cdc64aab74d14afbd8c3531e3d0bd49271a281f) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x380000, 0x80000 )
	ROM_LOAD( "su6.l1", ADSP2100_SIZE + 0x400000, 0x80000, CRC(f694b27f) SHA1(d43e38a124665f49ebb4ffc5a55e8f19a1a64686) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x480000, 0x80000 )
	ROM_LOAD( "su7.l1", ADSP2100_SIZE + 0x500000, 0x80000, CRC(20387e0a) SHA1(505d05173b2a1f1ee3ebc2898ccd3a95c98dd04a) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x580000, 0x80000 )
	/* su8 and su9 are unpopulated */

	ROM_REGION16_LE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )	/* 34010 code */
	ROM_LOAD16_BYTE( "mk242j12.bin", 0x00000, 0x80000, CRC(c7fb1525) SHA1(350be1a6f6da3a6b42764cfceae196696482def2) )
	ROM_LOAD16_BYTE( "mk242g12.bin", 0x00001, 0x80000, CRC(443d0e0a) SHA1(20e69c266cda59be92d7cd6423f6e03ad65226eb) )

	ROM_REGION( 0xc00000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "ug14-vid", 0x000000, 0x100000, CRC(01e73af6) SHA1(6598cfd704cc92a7f358a0e1f1c973ab79dcc493) )
	ROM_LOAD( "ug16-vid", 0x100000, 0x100000, CRC(8ba6ae18) SHA1(465fe907de4a1e502180c4e41642998dd3abc8e6) )
	ROM_LOAD( "ug17-vid", 0x200000, 0x100000, CRC(937d8620) SHA1(8b9f80a460b124a747a6d1495b53f01f580e28f1) )

	ROM_LOAD( "uj14-vid", 0x300000, 0x100000, CRC(d4985cbb) SHA1(367865da7efae38d83de3c0868d02a705177ae63) )
	ROM_LOAD( "uj16-vid", 0x400000, 0x100000, CRC(39d885b4) SHA1(2251826d247c3c6df421124718401fb35a672f83) )
	ROM_LOAD( "uj17-vid", 0x500000, 0x100000, CRC(218de160) SHA1(87aea173720d2a33d8183903f4fe8ba1d47e3348) )

	ROM_LOAD( "ug19-vid", 0x600000, 0x100000, CRC(fec137be) SHA1(f11ecb8a7993f5c4f4449564b4911f69bd6e9bf8) )
	ROM_LOAD( "ug20-vid", 0x700000, 0x100000, CRC(809118c1) SHA1(86153e648834c749e34573151cd4fee403a81962) )
	ROM_LOAD( "ug22-vid", 0x800000, 0x100000, CRC(154d53b1) SHA1(58ff0aa59101f40a9a3b5fbae1c904d0b0b31612) )

	ROM_LOAD( "uj19-vid", 0x900000, 0x100000, CRC(2d763156) SHA1(06536006da49ab5fb6b75b25f801b83fad000ff5) )
	ROM_LOAD( "uj20-vid", 0xa00000, 0x100000, CRC(b96824f0) SHA1(d42b122f9a57da330192abc7e5f97abc4065d718) )
	ROM_LOAD( "uj22-vid", 0xb00000, 0x100000, CRC(8891d785) SHA1(fd460df1ef8f4306ea42f7dc41488a80fd2c8f53) )
ROM_END


ROM_START( mk2r91 )
	ROM_REGION( 0x10, REGION_CPU1, 0 )		/* 34010 dummy region */

	ROM_REGION( ADSP2100_SIZE + 0x800000, REGION_CPU2, 0 )	/* ADSP-2105 data */
	ROM_LOAD( "su2.l1", ADSP2100_SIZE + 0x000000, 0x80000, CRC(5f23d71d) SHA1(54c2afef243759e0f3dbe2907edbc4302f5c8bad) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x080000, 0x80000 )
	ROM_LOAD( "su3.l1", ADSP2100_SIZE + 0x100000, 0x80000, CRC(d6d92bf9) SHA1(397351c6b707f2595e36360471015f9fa494e894) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x180000, 0x80000 )
	ROM_LOAD( "su4.l1", ADSP2100_SIZE + 0x200000, 0x80000, CRC(eebc8e0f) SHA1(705ab63ff7672a4857d546afda6dca4973cce1ad) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x280000, 0x80000 )
	ROM_LOAD( "su5.l1", ADSP2100_SIZE + 0x300000, 0x80000, CRC(2b0b7961) SHA1(1cdc64aab74d14afbd8c3531e3d0bd49271a281f) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x380000, 0x80000 )
	ROM_LOAD( "su6.l1", ADSP2100_SIZE + 0x400000, 0x80000, CRC(f694b27f) SHA1(d43e38a124665f49ebb4ffc5a55e8f19a1a64686) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x480000, 0x80000 )
	ROM_LOAD( "su7.l1", ADSP2100_SIZE + 0x500000, 0x80000, CRC(20387e0a) SHA1(505d05173b2a1f1ee3ebc2898ccd3a95c98dd04a) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x580000, 0x80000 )
	/* su8 and su9 are unpopulated */

	ROM_REGION16_LE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )	/* 34010 code */
	ROM_LOAD16_BYTE( "uj12.l91", 0x00000, 0x80000, CRC(41953903) SHA1(f72f92beb32e724d37e5f951b24539902dc16a9f) )
	ROM_LOAD16_BYTE( "ug12.l91", 0x00001, 0x80000, CRC(c07f745a) SHA1(049a18bc162274c897cae695032f32c851e57330) )

	ROM_REGION( 0xc00000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "ug14-vid", 0x000000, 0x100000, CRC(01e73af6) SHA1(6598cfd704cc92a7f358a0e1f1c973ab79dcc493) )
	ROM_LOAD( "ug16-vid", 0x100000, 0x100000, CRC(8ba6ae18) SHA1(465fe907de4a1e502180c4e41642998dd3abc8e6) )
	ROM_LOAD( "ug17-vid", 0x200000, 0x100000, CRC(937d8620) SHA1(8b9f80a460b124a747a6d1495b53f01f580e28f1) )

	ROM_LOAD( "uj14-vid", 0x300000, 0x100000, CRC(d4985cbb) SHA1(367865da7efae38d83de3c0868d02a705177ae63) )
	ROM_LOAD( "uj16-vid", 0x400000, 0x100000, CRC(39d885b4) SHA1(2251826d247c3c6df421124718401fb35a672f83) )
	ROM_LOAD( "uj17-vid", 0x500000, 0x100000, CRC(218de160) SHA1(87aea173720d2a33d8183903f4fe8ba1d47e3348) )

	ROM_LOAD( "ug19-vid", 0x600000, 0x100000, CRC(fec137be) SHA1(f11ecb8a7993f5c4f4449564b4911f69bd6e9bf8) )
	ROM_LOAD( "ug20-vid", 0x700000, 0x100000, CRC(809118c1) SHA1(86153e648834c749e34573151cd4fee403a81962) )
	ROM_LOAD( "ug22-vid", 0x800000, 0x100000, CRC(154d53b1) SHA1(58ff0aa59101f40a9a3b5fbae1c904d0b0b31612) )

	ROM_LOAD( "uj19-vid", 0x900000, 0x100000, CRC(2d763156) SHA1(06536006da49ab5fb6b75b25f801b83fad000ff5) )
	ROM_LOAD( "uj20-vid", 0xa00000, 0x100000, CRC(b96824f0) SHA1(d42b122f9a57da330192abc7e5f97abc4065d718) )
	ROM_LOAD( "uj22-vid", 0xb00000, 0x100000, CRC(8891d785) SHA1(fd460df1ef8f4306ea42f7dc41488a80fd2c8f53) )
ROM_END


ROM_START( mk2chal )
	ROM_REGION( 0x10, REGION_CPU1, 0 )		/* 34010 dummy region */

	ROM_REGION( ADSP2100_SIZE + 0x800000, REGION_CPU2, 0 )	/* ADSP-2105 data */
	ROM_LOAD( "su2.l1", ADSP2100_SIZE + 0x000000, 0x80000, CRC(5f23d71d) SHA1(54c2afef243759e0f3dbe2907edbc4302f5c8bad) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x080000, 0x80000 )
	ROM_LOAD( "su3.l1", ADSP2100_SIZE + 0x100000, 0x80000, CRC(d6d92bf9) SHA1(397351c6b707f2595e36360471015f9fa494e894) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x180000, 0x80000 )
	ROM_LOAD( "su4.l1", ADSP2100_SIZE + 0x200000, 0x80000, CRC(eebc8e0f) SHA1(705ab63ff7672a4857d546afda6dca4973cce1ad) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x280000, 0x80000 )
	ROM_LOAD( "su5.l1", ADSP2100_SIZE + 0x300000, 0x80000, CRC(2b0b7961) SHA1(1cdc64aab74d14afbd8c3531e3d0bd49271a281f) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x380000, 0x80000 )
	ROM_LOAD( "su6.l1", ADSP2100_SIZE + 0x400000, 0x80000, CRC(f694b27f) SHA1(d43e38a124665f49ebb4ffc5a55e8f19a1a64686) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x480000, 0x80000 )
	ROM_LOAD( "su7.l1", ADSP2100_SIZE + 0x500000, 0x80000, CRC(20387e0a) SHA1(505d05173b2a1f1ee3ebc2898ccd3a95c98dd04a) )
	ROM_RELOAD(	        ADSP2100_SIZE + 0x580000, 0x80000 )
	/* su8 and su9 are unpopulated */

	ROM_REGION16_LE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )	/* 34010 code */
	ROM_LOAD16_BYTE( "uj12.chl", 0x00000, 0x80000, CRC(2d5c04e6) SHA1(85947876319c86bdcdeccda99ae1ddbcfb212484) )
	ROM_LOAD16_BYTE( "ug12.chl", 0x00001, 0x80000, CRC(3e7a4bad) SHA1(9a8ad99e09badcea7f2bcf80a649c96a883a0463) )

	ROM_REGION( 0xc00000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "ug14-vid", 0x000000, 0x100000, CRC(01e73af6) SHA1(6598cfd704cc92a7f358a0e1f1c973ab79dcc493) )
	ROM_LOAD( "ug16-vid", 0x100000, 0x100000, CRC(8ba6ae18) SHA1(465fe907de4a1e502180c4e41642998dd3abc8e6) )
	ROM_LOAD( "ug17-vid", 0x200000, 0x100000, CRC(937d8620) SHA1(8b9f80a460b124a747a6d1495b53f01f580e28f1) )

	ROM_LOAD( "uj14-vid", 0x300000, 0x100000, CRC(d4985cbb) SHA1(367865da7efae38d83de3c0868d02a705177ae63) )
	ROM_LOAD( "uj16-vid", 0x400000, 0x100000, CRC(39d885b4) SHA1(2251826d247c3c6df421124718401fb35a672f83) )
	ROM_LOAD( "uj17-vid", 0x500000, 0x100000, CRC(218de160) SHA1(87aea173720d2a33d8183903f4fe8ba1d47e3348) )

	ROM_LOAD( "ug19-vid", 0x600000, 0x100000, CRC(fec137be) SHA1(f11ecb8a7993f5c4f4449564b4911f69bd6e9bf8) )
	ROM_LOAD( "ug20-vid", 0x700000, 0x100000, CRC(809118c1) SHA1(86153e648834c749e34573151cd4fee403a81962) )
	ROM_LOAD( "ug22-vid", 0x800000, 0x100000, CRC(154d53b1) SHA1(58ff0aa59101f40a9a3b5fbae1c904d0b0b31612) )

	ROM_LOAD( "uj19-vid", 0x900000, 0x100000, CRC(2d763156) SHA1(06536006da49ab5fb6b75b25f801b83fad000ff5) )
	ROM_LOAD( "uj20-vid", 0xa00000, 0x100000, CRC(b96824f0) SHA1(d42b122f9a57da330192abc7e5f97abc4065d718) )
	ROM_LOAD( "uj22-vid", 0xb00000, 0x100000, CRC(8891d785) SHA1(fd460df1ef8f4306ea42f7dc41488a80fd2c8f53) )
ROM_END


/*
    equivalences for the extension board version (same contents, split in half)

	ROM_LOAD( "ug14.l1",  0x000000, 0x080000, CRC(74f5aaf1) )
	ROM_LOAD( "ug16.l11", 0x080000, 0x080000, CRC(1cf58c4c) )
	ROM_LOAD( "u8.l1",    0x200000, 0x080000, CRC(56e22ff5) )
	ROM_LOAD( "u11.l1",   0x280000, 0x080000, CRC(559ca4a3) )
	ROM_LOAD( "ug17.l1",  0x100000, 0x080000, CRC(4202d8bf) )
	ROM_LOAD( "ug18.l1",  0x180000, 0x080000, CRC(a3deab6a) )

	ROM_LOAD( "uj14.l1",  0x300000, 0x080000, CRC(869a3c55) )
	ROM_LOAD( "uj16.l11", 0x380000, 0x080000, CRC(c70cf053) )
	ROM_LOAD( "u9.l1",    0x500000, 0x080000, CRC(67da0769) )
	ROM_LOAD( "u10.l1",   0x580000, 0x080000, CRC(69000ac3) )
	ROM_LOAD( "uj17.l1",  0x400000, 0x080000, CRC(ec3e1884) )
	ROM_LOAD( "uj18.l1",  0x480000, 0x080000, CRC(c9f5aef4) )

	ROM_LOAD( "u6.l1",    0x600000, 0x080000, CRC(8d4c496a) )
	ROM_LOAD( "u13.l11",  0x680000, 0x080000, CRC(7fb20a45) )
	ROM_LOAD( "ug19.l1",  0x800000, 0x080000, CRC(d6c1f75e) )
	ROM_LOAD( "ug20.l1",  0x880000, 0x080000, CRC(19a33cff) )
	ROM_LOAD( "ug22.l1",  0x700000, 0x080000, CRC(db6cfa45) )
	ROM_LOAD( "ug23.l1",  0x780000, 0x080000, CRC(bfd8b656) )

	ROM_LOAD( "u7.l1",    0x900000, 0x080000, CRC(3988aac8) )
	ROM_LOAD( "u12.l11",  0x980000, 0x080000, CRC(2ef12cc6) )
	ROM_LOAD( "uj19.l1",  0xb00000, 0x080000, CRC(4eed6f18) )
	ROM_LOAD( "uj20.l1",  0xb80000, 0x080000, CRC(337b1e20) )
	ROM_LOAD( "uj22.l1",  0xa00000, 0x080000, CRC(a6546b15) )
	ROM_LOAD( "uj23.l1",  0xa80000, 0x080000, CRC(45867c6f) )
*/


ROM_START( nbajam )
	ROM_REGION( 0x10, REGION_CPU1, 0 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD(  "nbau3.bin", 0x010000, 0x20000, CRC(3a3ea480) SHA1(d12a45cba5c35f046b176661d7877fa4fd0e6c13) )
	ROM_RELOAD(             0x030000, 0x20000 )

	ROM_REGION( 0x1c0000, REGION_SOUND1, 0 )	/* ADPCM */
	ROM_LOAD( "nbau12.bin", 0x000000, 0x80000, CRC(b94847f1) SHA1(e7efa0a379bfa91fe4ffb75f07a5dfbfde9a96b4) )
	ROM_LOAD( "nbau13.bin", 0x080000, 0x80000, CRC(b6fe24bd) SHA1(f70f75b5570a2b368ebc74d2a7d264c618940430) )

	ROM_REGION16_LE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )	/* 34010 code */
	ROM_LOAD16_BYTE( "nbauj12.bin", 0x00000, 0x80000, CRC(b93e271c) SHA1(b0e9f055376a4a4cd1115a81f71c933903c251b1) )
	ROM_LOAD16_BYTE( "nbaug12.bin", 0x00001, 0x80000, CRC(407d3390) SHA1(a319bc890d94310e44fe2ec98bfc95665a662701) )

	ROM_REGION( 0xc00000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "nbaug14.bin", 0x000000, 0x80000, CRC(04bb9f64) SHA1(9e1a8c37e14cb6fe67f4aa3caa9022f356f1ca64) )
	ROM_LOAD( "nbaug16.bin", 0x080000, 0x80000, CRC(8591c572) SHA1(237bab2e93abf438a84be3603505db5de59922af) )
	ROM_LOAD( "nbaug17.bin", 0x100000, 0x80000, CRC(6f921886) SHA1(72542249ca6602dc4816952765c1810f064ff394) )
	ROM_LOAD( "nbaug18.bin", 0x180000, 0x80000, CRC(5162d3d6) SHA1(14d377977510b7793e4006a7a5089dbfd785d7d1) )

	ROM_LOAD( "nbauj14.bin", 0x300000, 0x80000, CRC(b34b7af3) SHA1(0abb74d2f414bc9da0380a81beb134f3a87c1a0a) )
	ROM_LOAD( "nbauj16.bin", 0x380000, 0x80000, CRC(d2e554f1) SHA1(139aa39bd48b8605058ece188f9f5e6793561fcb) )
	ROM_LOAD( "nbauj17.bin", 0x400000, 0x80000, CRC(b2e14981) SHA1(5cec9b7fcaa6d0ce5bff689541fc98db435c5b5f) )
	ROM_LOAD( "nbauj18.bin", 0x480000, 0x80000, CRC(fdee0037) SHA1(3bcc740f4bdb3236822cd6e7ed06241804351cca) )

	ROM_LOAD( "nbaug19.bin", 0x600000, 0x80000, CRC(a8f22fbb) SHA1(514208a9d6d0c8c2d7847cc02d4387eac90be659) )
	ROM_LOAD( "nbaug20.bin", 0x680000, 0x80000, CRC(44fd6221) SHA1(1d6754bf2c24950080523f66b77407931babba29) )
	ROM_LOAD( "nbaug22.bin", 0x700000, 0x80000, CRC(ab05ed89) SHA1(4153d098fbaeac963d93f26dcd9d8bc33a48a734) )
	ROM_LOAD( "nbaug23.bin", 0x780000, 0x80000, CRC(7b934c7a) SHA1(a6992fb3c50429ac4fa15bd91612ae0c0b8f961d) )

	ROM_LOAD( "nbauj19.bin", 0x900000, 0x80000, CRC(8130a8a2) SHA1(f23f124024285d07d8cf822817b62e42c38b82db) )
	ROM_LOAD( "nbauj20.bin", 0x980000, 0x80000, CRC(f9cebbb6) SHA1(6202e490bc5658bd0741422f841540fcd037cfee) )
	ROM_LOAD( "nbauj22.bin", 0xa00000, 0x80000, CRC(59a95878) SHA1(b95165987853f164842ab2b5895ea95484a1d78b) )
	ROM_LOAD( "nbauj23.bin", 0xa80000, 0x80000, CRC(427d2eee) SHA1(4985e3dd9c9e1bedd5a900958bf549656debd494) )
ROM_END


ROM_START( nbajamr2 )
	ROM_REGION( 0x10, REGION_CPU1, 0 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD(  "nbau3.bin", 0x010000, 0x20000, CRC(3a3ea480) SHA1(d12a45cba5c35f046b176661d7877fa4fd0e6c13) )
	ROM_RELOAD(             0x030000, 0x20000 )

	ROM_REGION( 0x1c0000, REGION_SOUND1, 0 )	/* ADPCM */
	ROM_LOAD( "nbau12.bin", 0x000000, 0x80000, CRC(b94847f1) SHA1(e7efa0a379bfa91fe4ffb75f07a5dfbfde9a96b4) )
	ROM_LOAD( "nbau13.bin", 0x080000, 0x80000, CRC(b6fe24bd) SHA1(f70f75b5570a2b368ebc74d2a7d264c618940430) )

	ROM_REGION16_LE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )	/* 34010 code */
	ROM_LOAD16_BYTE( "jam2uj12.bin", 0x00000, 0x80000, CRC(0fe80b36) SHA1(fe6b21dc9b393b25c511b2914b568fa92301d749) )
	ROM_LOAD16_BYTE( "jam2ug12.bin", 0x00001, 0x80000, CRC(5d106315) SHA1(e2cddd9ed6771e77711e3a4f25fe2d07712d954e) )

	ROM_REGION( 0xc00000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "nbaug14.bin", 0x000000, 0x80000, CRC(04bb9f64) SHA1(9e1a8c37e14cb6fe67f4aa3caa9022f356f1ca64) )
	ROM_LOAD( "nbaug16.bin", 0x080000, 0x80000, CRC(8591c572) SHA1(237bab2e93abf438a84be3603505db5de59922af) )
	ROM_LOAD( "nbaug17.bin", 0x100000, 0x80000, CRC(6f921886) SHA1(72542249ca6602dc4816952765c1810f064ff394) )
	ROM_LOAD( "nbaug18.bin", 0x180000, 0x80000, CRC(5162d3d6) SHA1(14d377977510b7793e4006a7a5089dbfd785d7d1) )

	ROM_LOAD( "nbauj14.bin", 0x300000, 0x80000, CRC(b34b7af3) SHA1(0abb74d2f414bc9da0380a81beb134f3a87c1a0a) )
	ROM_LOAD( "nbauj16.bin", 0x380000, 0x80000, CRC(d2e554f1) SHA1(139aa39bd48b8605058ece188f9f5e6793561fcb) )
	ROM_LOAD( "nbauj17.bin", 0x400000, 0x80000, CRC(b2e14981) SHA1(5cec9b7fcaa6d0ce5bff689541fc98db435c5b5f) )
	ROM_LOAD( "nbauj18.bin", 0x480000, 0x80000, CRC(fdee0037) SHA1(3bcc740f4bdb3236822cd6e7ed06241804351cca) )

	ROM_LOAD( "nbaug19.bin", 0x600000, 0x80000, CRC(a8f22fbb) SHA1(514208a9d6d0c8c2d7847cc02d4387eac90be659) )
	ROM_LOAD( "nbaug20.bin", 0x680000, 0x80000, CRC(44fd6221) SHA1(1d6754bf2c24950080523f66b77407931babba29) )
	ROM_LOAD( "nbaug22.bin", 0x700000, 0x80000, CRC(ab05ed89) SHA1(4153d098fbaeac963d93f26dcd9d8bc33a48a734) )
	ROM_LOAD( "nbaug23.bin", 0x780000, 0x80000, CRC(7b934c7a) SHA1(a6992fb3c50429ac4fa15bd91612ae0c0b8f961d) )

	ROM_LOAD( "nbauj19.bin", 0x900000, 0x80000, CRC(8130a8a2) SHA1(f23f124024285d07d8cf822817b62e42c38b82db) )
	ROM_LOAD( "nbauj20.bin", 0x980000, 0x80000, CRC(f9cebbb6) SHA1(6202e490bc5658bd0741422f841540fcd037cfee) )
	ROM_LOAD( "nbauj22.bin", 0xa00000, 0x80000, CRC(59a95878) SHA1(b95165987853f164842ab2b5895ea95484a1d78b) )
	ROM_LOAD( "nbauj23.bin", 0xa80000, 0x80000, CRC(427d2eee) SHA1(4985e3dd9c9e1bedd5a900958bf549656debd494) )
ROM_END


ROM_START( nbajamte )
	ROM_REGION( 0x10, REGION_CPU1, 0 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD(  "te-u3.bin", 0x010000, 0x20000, CRC(d4551195) SHA1(e8908fbe4339fb8c93f7e74113dfd25dda1667ea) )
	ROM_RELOAD(             0x030000, 0x20000 )

	ROM_REGION( 0x1c0000, REGION_SOUND1, 0 )	/* ADPCM */
	ROM_LOAD( "te-u12.bin", 0x000000, 0x80000, CRC(4fac97bc) SHA1(bd88d8c3edab0e35ad9f9350bcbaa17cda61d87a) )
	ROM_LOAD( "te-u13.bin", 0x080000, 0x80000, CRC(6f27b202) SHA1(c1f0db15624d1e7102ce9fd1db49ccf86e8611d6) )

	ROM_REGION16_LE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )	/* 34010 code */
	ROM_LOAD16_BYTE( "te-uj12.l4", 0x00000, 0x80000, CRC(d7c21bc4) SHA1(e05f0299b955500df6a08b1c0b24b932a9cdfa6a) )
	ROM_LOAD16_BYTE( "te-ug12.l4", 0x00001, 0x80000, CRC(7ad49229) SHA1(e9ceedb0e620809d8a4d42087d806aa296a4cd59) )

	ROM_REGION( 0xc00000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "nbaug14.bin", 0x000000, 0x80000, CRC(04bb9f64) SHA1(9e1a8c37e14cb6fe67f4aa3caa9022f356f1ca64) )
	ROM_LOAD( "te-ug16.bin", 0x080000, 0x80000, CRC(c7ce74d0) SHA1(93861cd909e0f28ed112096d6f9fc57d0d31c57c) )
	ROM_LOAD( "te-ug17.bin", 0x100000, 0x80000, CRC(9401be62) SHA1(597413a8a1eb66a7ad89af2f548fa3062e5e8efb) )
	ROM_LOAD( "te-ug18.bin", 0x180000, 0x80000, CRC(6fd08f57) SHA1(5b7031dffc88374c5bfdf3021aa01ec4e28d0631) )

	ROM_LOAD( "nbauj14.bin", 0x300000, 0x80000, CRC(b34b7af3) SHA1(0abb74d2f414bc9da0380a81beb134f3a87c1a0a) )
	ROM_LOAD( "te-uj16.bin", 0x380000, 0x80000, CRC(905ad88b) SHA1(24c336ccc0e2ac0ee96a34ad6fe4aa7464de0009) )
	ROM_LOAD( "te-uj17.bin", 0x400000, 0x80000, CRC(8a852b9e) SHA1(604c7f4305887e9505320630027765ea76607c58) )
	ROM_LOAD( "te-uj18.bin", 0x480000, 0x80000, CRC(4eb73c26) SHA1(693bf45f777da8e55b7bcd8699ea5bd711964941) )

	ROM_LOAD( "nbaug19.bin", 0x600000, 0x80000, CRC(a8f22fbb) SHA1(514208a9d6d0c8c2d7847cc02d4387eac90be659) )
	ROM_LOAD( "te-ug20.bin", 0x680000, 0x80000, CRC(8a48728c) SHA1(3684099b4934b027336c319c77d9e0710b8c22dc) )
	ROM_LOAD( "te-ug22.bin", 0x700000, 0x80000, CRC(3b05133b) SHA1(f6067abb92b8751afe7352a4f1b1a22c9528002b) )
	ROM_LOAD( "te-ug23.bin", 0x780000, 0x80000, CRC(854f73bc) SHA1(242cc8ce28711f6f0787524a1070eb4b0956e6ae) )

	ROM_LOAD( "nbauj19.bin", 0x900000, 0x80000, CRC(8130a8a2) SHA1(f23f124024285d07d8cf822817b62e42c38b82db) )
	ROM_LOAD( "te-uj20.bin", 0x980000, 0x80000, CRC(bf263d61) SHA1(b5b59e8df55f8030eff068c1d8b07dad8521bf5d) )
	ROM_LOAD( "te-uj22.bin", 0xa00000, 0x80000, CRC(39791051) SHA1(7aa02500ddacd31fca04044a22a38f36452ca300) )
	ROM_LOAD( "te-uj23.bin", 0xa80000, 0x80000, CRC(f8c30998) SHA1(33e2f982d74e9f3686b1f4a8172c49fb8b604cf5) )
ROM_END


ROM_START( nbajamt1 )
	ROM_REGION( 0x10, REGION_CPU1, 0 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD(  "te-u3.bin", 0x010000, 0x20000, CRC(d4551195) SHA1(e8908fbe4339fb8c93f7e74113dfd25dda1667ea) )
	ROM_RELOAD(             0x030000, 0x20000 )

	ROM_REGION( 0x1c0000, REGION_SOUND1, 0 )	/* ADPCM */
	ROM_LOAD( "te-u12.bin", 0x000000, 0x80000, CRC(4fac97bc) SHA1(bd88d8c3edab0e35ad9f9350bcbaa17cda61d87a) )
	ROM_LOAD( "te-u13.bin", 0x080000, 0x80000, CRC(6f27b202) SHA1(c1f0db15624d1e7102ce9fd1db49ccf86e8611d6) )

	ROM_REGION16_LE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )	/* 34010 code */
	ROM_LOAD16_BYTE( "te-uj12.l1", 0x00000, 0x80000, CRC(a9f555ad) SHA1(34f5fc1b003ef8acbb2b38fbacd58d018d20ab1b) )
	ROM_LOAD16_BYTE( "te-ug12.l1", 0x00001, 0x80000, CRC(bd4579b5) SHA1(c893cff931f1e60a1d0d29d2719f514d92fb3490) )

	ROM_REGION( 0xc00000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "nbaug14.bin", 0x000000, 0x80000, CRC(04bb9f64) SHA1(9e1a8c37e14cb6fe67f4aa3caa9022f356f1ca64) )
	ROM_LOAD( "te-ug16.bin", 0x080000, 0x80000, CRC(c7ce74d0) SHA1(93861cd909e0f28ed112096d6f9fc57d0d31c57c) )
	ROM_LOAD( "te-ug17.bin", 0x100000, 0x80000, CRC(9401be62) SHA1(597413a8a1eb66a7ad89af2f548fa3062e5e8efb) )
	ROM_LOAD( "te-ug18.bin", 0x180000, 0x80000, CRC(6fd08f57) SHA1(5b7031dffc88374c5bfdf3021aa01ec4e28d0631) )

	ROM_LOAD( "nbauj14.bin", 0x300000, 0x80000, CRC(b34b7af3) SHA1(0abb74d2f414bc9da0380a81beb134f3a87c1a0a) )
	ROM_LOAD( "te-uj16.bin", 0x380000, 0x80000, CRC(905ad88b) SHA1(24c336ccc0e2ac0ee96a34ad6fe4aa7464de0009) )
	ROM_LOAD( "te-uj17.bin", 0x400000, 0x80000, CRC(8a852b9e) SHA1(604c7f4305887e9505320630027765ea76607c58) )
	ROM_LOAD( "te-uj18.bin", 0x480000, 0x80000, CRC(4eb73c26) SHA1(693bf45f777da8e55b7bcd8699ea5bd711964941) )

	ROM_LOAD( "nbaug19.bin", 0x600000, 0x80000, CRC(a8f22fbb) SHA1(514208a9d6d0c8c2d7847cc02d4387eac90be659) )
	ROM_LOAD( "te-ug20.bin", 0x680000, 0x80000, CRC(8a48728c) SHA1(3684099b4934b027336c319c77d9e0710b8c22dc) )
	ROM_LOAD( "te-ug22.bin", 0x700000, 0x80000, CRC(3b05133b) SHA1(f6067abb92b8751afe7352a4f1b1a22c9528002b) )
	ROM_LOAD( "te-ug23.bin", 0x780000, 0x80000, CRC(854f73bc) SHA1(242cc8ce28711f6f0787524a1070eb4b0956e6ae) )

	ROM_LOAD( "nbauj19.bin", 0x900000, 0x80000, CRC(8130a8a2) SHA1(f23f124024285d07d8cf822817b62e42c38b82db) )
	ROM_LOAD( "te-uj20.bin", 0x980000, 0x80000, CRC(bf263d61) SHA1(b5b59e8df55f8030eff068c1d8b07dad8521bf5d) )
	ROM_LOAD( "te-uj22.bin", 0xa00000, 0x80000, CRC(39791051) SHA1(7aa02500ddacd31fca04044a22a38f36452ca300) )
	ROM_LOAD( "te-uj23.bin", 0xa80000, 0x80000, CRC(f8c30998) SHA1(33e2f982d74e9f3686b1f4a8172c49fb8b604cf5) )
ROM_END


ROM_START( nbajamt2 )
	ROM_REGION( 0x10, REGION_CPU1, 0 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD(  "te-u3.bin", 0x010000, 0x20000, CRC(d4551195) SHA1(e8908fbe4339fb8c93f7e74113dfd25dda1667ea) )
	ROM_RELOAD(             0x030000, 0x20000 )

	ROM_REGION( 0x1c0000, REGION_SOUND1, 0 )	/* ADPCM */
	ROM_LOAD( "te-u12.bin", 0x000000, 0x80000, CRC(4fac97bc) SHA1(bd88d8c3edab0e35ad9f9350bcbaa17cda61d87a) )
	ROM_LOAD( "te-u13.bin", 0x080000, 0x80000, CRC(6f27b202) SHA1(c1f0db15624d1e7102ce9fd1db49ccf86e8611d6) )

	ROM_REGION16_LE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )	/* 34010 code */
	ROM_LOAD16_BYTE( "te-uj12.l2", 0x00000, 0x80000, CRC(eaa6fb32) SHA1(8c8c0c6ace2b98679d7fe90e1f9284bdf0e14eaf) )
	ROM_LOAD16_BYTE( "te-ug12.l2", 0x00001, 0x80000, CRC(5a694d9a) SHA1(fb74e4242d9adba03f24a81451ea06e8d9b4af96) )

	ROM_REGION( 0xc00000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "nbaug14.bin", 0x000000, 0x80000, CRC(04bb9f64) SHA1(9e1a8c37e14cb6fe67f4aa3caa9022f356f1ca64) )
	ROM_LOAD( "te-ug16.bin", 0x080000, 0x80000, CRC(c7ce74d0) SHA1(93861cd909e0f28ed112096d6f9fc57d0d31c57c) )
	ROM_LOAD( "te-ug17.bin", 0x100000, 0x80000, CRC(9401be62) SHA1(597413a8a1eb66a7ad89af2f548fa3062e5e8efb) )
	ROM_LOAD( "te-ug18.bin", 0x180000, 0x80000, CRC(6fd08f57) SHA1(5b7031dffc88374c5bfdf3021aa01ec4e28d0631) )

	ROM_LOAD( "nbauj14.bin", 0x300000, 0x80000, CRC(b34b7af3) SHA1(0abb74d2f414bc9da0380a81beb134f3a87c1a0a) )
	ROM_LOAD( "te-uj16.bin", 0x380000, 0x80000, CRC(905ad88b) SHA1(24c336ccc0e2ac0ee96a34ad6fe4aa7464de0009) )
	ROM_LOAD( "te-uj17.bin", 0x400000, 0x80000, CRC(8a852b9e) SHA1(604c7f4305887e9505320630027765ea76607c58) )
	ROM_LOAD( "te-uj18.bin", 0x480000, 0x80000, CRC(4eb73c26) SHA1(693bf45f777da8e55b7bcd8699ea5bd711964941) )

	ROM_LOAD( "nbaug19.bin", 0x600000, 0x80000, CRC(a8f22fbb) SHA1(514208a9d6d0c8c2d7847cc02d4387eac90be659) )
	ROM_LOAD( "te-ug20.bin", 0x680000, 0x80000, CRC(8a48728c) SHA1(3684099b4934b027336c319c77d9e0710b8c22dc) )
	ROM_LOAD( "te-ug22.bin", 0x700000, 0x80000, CRC(3b05133b) SHA1(f6067abb92b8751afe7352a4f1b1a22c9528002b) )
	ROM_LOAD( "te-ug23.bin", 0x780000, 0x80000, CRC(854f73bc) SHA1(242cc8ce28711f6f0787524a1070eb4b0956e6ae) )

	ROM_LOAD( "nbauj19.bin", 0x900000, 0x80000, CRC(8130a8a2) SHA1(f23f124024285d07d8cf822817b62e42c38b82db) )
	ROM_LOAD( "te-uj20.bin", 0x980000, 0x80000, CRC(bf263d61) SHA1(b5b59e8df55f8030eff068c1d8b07dad8521bf5d) )
	ROM_LOAD( "te-uj22.bin", 0xa00000, 0x80000, CRC(39791051) SHA1(7aa02500ddacd31fca04044a22a38f36452ca300) )
	ROM_LOAD( "te-uj23.bin", 0xa80000, 0x80000, CRC(f8c30998) SHA1(33e2f982d74e9f3686b1f4a8172c49fb8b604cf5) )
ROM_END


ROM_START( nbajamt3 )
	ROM_REGION( 0x10, REGION_CPU1, 0 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD(  "te-u3.bin", 0x010000, 0x20000, CRC(d4551195) SHA1(e8908fbe4339fb8c93f7e74113dfd25dda1667ea) )
	ROM_RELOAD(             0x030000, 0x20000 )

	ROM_REGION( 0x1c0000, REGION_SOUND1, 0 )	/* ADPCM */
	ROM_LOAD( "te-u12.bin", 0x000000, 0x80000, CRC(4fac97bc) SHA1(bd88d8c3edab0e35ad9f9350bcbaa17cda61d87a) )
	ROM_LOAD( "te-u13.bin", 0x080000, 0x80000, CRC(6f27b202) SHA1(c1f0db15624d1e7102ce9fd1db49ccf86e8611d6) )

	ROM_REGION16_LE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )	/* 34010 code */
	ROM_LOAD16_BYTE( "te-uj12.l3", 0x00000, 0x80000, CRC(8fdf77b4) SHA1(1a8a178b19d0b8e7a5fd2ddf373a4279321440d0) )
	ROM_LOAD16_BYTE( "te-ug12.l3", 0x00001, 0x80000, CRC(656579ed) SHA1(b038fdc814ebc8d203724fdb2f7501d40f1dc21f) )

	ROM_REGION( 0xc00000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "nbaug14.bin", 0x000000, 0x80000, CRC(04bb9f64) SHA1(9e1a8c37e14cb6fe67f4aa3caa9022f356f1ca64) )
	ROM_LOAD( "te-ug16.bin", 0x080000, 0x80000, CRC(c7ce74d0) SHA1(93861cd909e0f28ed112096d6f9fc57d0d31c57c) )
	ROM_LOAD( "te-ug17.bin", 0x100000, 0x80000, CRC(9401be62) SHA1(597413a8a1eb66a7ad89af2f548fa3062e5e8efb) )
	ROM_LOAD( "te-ug18.bin", 0x180000, 0x80000, CRC(6fd08f57) SHA1(5b7031dffc88374c5bfdf3021aa01ec4e28d0631) )

	ROM_LOAD( "nbauj14.bin", 0x300000, 0x80000, CRC(b34b7af3) SHA1(0abb74d2f414bc9da0380a81beb134f3a87c1a0a) )
	ROM_LOAD( "te-uj16.bin", 0x380000, 0x80000, CRC(905ad88b) SHA1(24c336ccc0e2ac0ee96a34ad6fe4aa7464de0009) )
	ROM_LOAD( "te-uj17.bin", 0x400000, 0x80000, CRC(8a852b9e) SHA1(604c7f4305887e9505320630027765ea76607c58) )
	ROM_LOAD( "te-uj18.bin", 0x480000, 0x80000, CRC(4eb73c26) SHA1(693bf45f777da8e55b7bcd8699ea5bd711964941) )

	ROM_LOAD( "nbaug19.bin", 0x600000, 0x80000, CRC(a8f22fbb) SHA1(514208a9d6d0c8c2d7847cc02d4387eac90be659) )
	ROM_LOAD( "te-ug20.bin", 0x680000, 0x80000, CRC(8a48728c) SHA1(3684099b4934b027336c319c77d9e0710b8c22dc) )
	ROM_LOAD( "te-ug22.bin", 0x700000, 0x80000, CRC(3b05133b) SHA1(f6067abb92b8751afe7352a4f1b1a22c9528002b) )
	ROM_LOAD( "te-ug23.bin", 0x780000, 0x80000, CRC(854f73bc) SHA1(242cc8ce28711f6f0787524a1070eb4b0956e6ae) )

	ROM_LOAD( "nbauj19.bin", 0x900000, 0x80000, CRC(8130a8a2) SHA1(f23f124024285d07d8cf822817b62e42c38b82db) )
	ROM_LOAD( "te-uj20.bin", 0x980000, 0x80000, CRC(bf263d61) SHA1(b5b59e8df55f8030eff068c1d8b07dad8521bf5d) )
	ROM_LOAD( "te-uj22.bin", 0xa00000, 0x80000, CRC(39791051) SHA1(7aa02500ddacd31fca04044a22a38f36452ca300) )
	ROM_LOAD( "te-uj23.bin", 0xa80000, 0x80000, CRC(f8c30998) SHA1(33e2f982d74e9f3686b1f4a8172c49fb8b604cf5) )
ROM_END


ROM_START( jdreddp )
	ROM_REGION( 0x10, REGION_CPU1, 0 )		/* 34010 dummy region */

	ROM_REGION( 0x50000, REGION_CPU2, 0 ) /* sound CPU */
	ROM_LOAD(  "jd_u3.rom", 0x010000, 0x20000, CRC(6154d108) SHA1(54328455ec22ba815de85aa3bfe6405353c64f5c) )
	ROM_RELOAD(             0x030000, 0x20000 )

	ROM_REGION( 0x1c0000, REGION_SOUND1, 0 )	/* ADPCM */
	ROM_LOAD( "jd_u12.rom", 0x000000, 0x80000, CRC(ef32f202) SHA1(16aea085e63496dec259291de1a64fbeab52f039) )
	ROM_LOAD( "jd_u13.rom", 0x080000, 0x80000, CRC(3dc70473) SHA1(a3d7210301ff0579889009a075092115d9bf0600) )

	ROM_REGION16_LE( 0x100000, REGION_USER1, ROMREGION_DISPOSE )	/* 34010 code */
	ROM_LOAD16_BYTE( "jd_uj12.rom", 0x00000, 0x80000, CRC(7e5c8d5a) SHA1(65c0e887fea01846426067adfc4cf60dce4a1e24) )
	ROM_LOAD16_BYTE( "jd_ug12.rom", 0x00001, 0x80000, CRC(a16b8a4a) SHA1(77abb31e7cb3b66c63ef7c1874d8544ae9a02667) )

	ROM_REGION( 0xc00000, REGION_GFX1, ROMREGION_DISPOSE )
	ROM_LOAD( "jd_ug14.rom", 0x000000, 0x80000, CRC(468484d7) SHA1(87e3b87051e3afff097333af90efa0eb4dd61a35) )
	ROM_LOAD( "jd_ug16.rom", 0x080000, 0x80000, CRC(1d7f12b6) SHA1(beb864615a6c554097377a2f2b6dfe361c1fb084) )
	ROM_LOAD( "jd_ug17.rom", 0x100000, 0x80000, CRC(b6d83d74) SHA1(e0e71f691af5b55fb4153a6b80d3055641cb7cf4) )
	ROM_LOAD( "jd_ug18.rom", 0x180000, 0x80000, CRC(c8a45e01) SHA1(6d63a977c30d5f421baf48db55da90c75032a75f) )

	ROM_LOAD( "jd_uj14.rom", 0x300000, 0x80000, CRC(fe6ec0ec) SHA1(3e3b1774e1c5cf6629fbd3aeff36cadff1adfbf9) )
	ROM_LOAD( "jd_uj16.rom", 0x380000, 0x80000, CRC(31d4a71b) SHA1(703448956968f1913e5755a6aedf0f7d15ea4a4e) )
	ROM_LOAD( "jd_uj17.rom", 0x400000, 0x80000, CRC(ddc76f0b) SHA1(8f3091c6a5ec1488fcd296e75bbd0572f1a4485c) )
	ROM_LOAD( "jd_uj18.rom", 0x480000, 0x80000, CRC(3e16e7a9) SHA1(f517d42594225b06d70404f29e44dc144ad87a72) )

	ROM_LOAD( "jd_ug19.rom", 0x600000, 0x80000, CRC(e076c08e) SHA1(9b52470feac66b258e62e53dfd6a6a74c1e47ac1) )
	ROM_LOAD( "jd_ug20.rom", 0x680000, 0x80000, CRC(7b8c370a) SHA1(e6562782519610447657d0850481b1f9fd7c08b3) )
	ROM_LOAD( "jd_ug22.rom", 0x700000, 0x80000, CRC(6705d5b3) SHA1(da304ea33cd20c118b97147fe603237fe5940732) )
	ROM_LOAD( "jd_ug23.rom", 0x780000, 0x80000, CRC(0c9edbc4) SHA1(bb3926a992efd1923d64c5bc615dac39867f426d) )

	ROM_LOAD( "jd_uj19.rom", 0x900000, 0x80000, CRC(bd8cffe0) SHA1(7690bfa82ab5c2c102dc5c6e60628f341b83a77b) )
	ROM_LOAD( "jd_uj20.rom", 0x980000, 0x80000, CRC(8fc7bfb9) SHA1(c3c31ea641a6e304b060a7938e2ac473db8a7aab) )
	ROM_LOAD( "jd_uj22.rom", 0xa00000, 0x80000, CRC(7438295e) SHA1(dbc28a9273897d50abf8e7bebe0753949365eb42) )
	ROM_LOAD( "jd_uj23.rom", 0xa80000, 0x80000, CRC(86ea157d) SHA1(9189e07abc73b601a26ae8aaf6d49ed87d1befca) )
ROM_END



/*************************************
 *
 *	Game drivers
 *
 *************************************/

GAME( 1992, mk,       0,       mk, mk,      mk,       ROT0, "Midway",   "Mortal Kombat (rev 5.0 T-Unit 03-19-93)" )
GAME( 1992, mkr4,     mk,      mk, mk,      mkr4,     ROT0, "Midway",   "Mortal Kombat (rev 4.0 T-Unit 02-11-93)" )

GAMEC( 1993, mk2,      0,       tunit_dcs,   mk2,     mk2,      ROT0, "Midway",   "Mortal Kombat II (rev L3.1)", &generic_ctrl, &mk2_bootstrap )
GAMEC( 1993, mk2r32,   mk2,     tunit_dcs,   mk2,     mk2,      ROT0, "Midway",   "Mortal Kombat II (rev L3.2 (European))", &generic_ctrl, &mk2r32_bootstrap )
GAMEC( 1993, mk2r21,   mk2,     tunit_dcs,   mk2,     mk2r21,   ROT0, "Midway",   "Mortal Kombat II (rev L2.1)", &generic_ctrl, &mk2r21_bootstrap)
GAMEC( 1993, mk2r14,   mk2,     tunit_dcs,   mk2,     mk2r14,   ROT0, "Midway",   "Mortal Kombat II (rev L1.4)", &generic_ctrl, &mk2r14_bootstrap )
GAMEC( 1993, mk2r42,   mk2,     tunit_dcs,   mk2,     mk2,      ROT0, "hack",     "Mortal Kombat II (rev L4.2, hack)", &generic_ctrl, &mk2r42_bootstrap )
GAMEC( 1993, mk2r91,   mk2,     tunit_dcs,   mk2,     mk2,      ROT0, "hack",     "Mortal Kombat II (rev L9.1, hack)", &generic_ctrl, &mk2r91_bootstrap)
GAMEC( 1993, mk2chal,  mk2,     tunit_dcs,   mk2,     mk2,      ROT0, "hack",     "Mortal Kombat II Challenger (hack)", &generic_ctrl, &mk2chal_bootstrap )

GAME( 1993, jdreddp,  0,       tunit_adpcm, jdreddp, jdreddp,  ROT0, "Midway",   "Judge Dredd (rev LA1, prototype)" )

GAME( 1993, nbajam,   0,       nbajam, nbajam,  nbajam,   ROT0, "Midway",   "NBA Jam (rev 3.01 04-07-93)" )
GAME( 1993, nbajamr2, nbajam,  nbajam, nbajam,  nbajam20, ROT0, "Midway",   "NBA Jam (rev 2.00 02-10-93)" )
GAME( 1994, nbajamte, nbajam,  tunit_adpcm, nbajamte,  nbajamte, ROT0, "Midway",   "NBA Jam TE (rev 4.0 03-23-94)" )
GAME( 1994, nbajamt1, nbajam,  tunit_adpcm, nbajamte,  nbajamte, ROT0, "Midway",   "NBA Jam TE (rev 1.0 01-17-94)" )
GAME( 1994, nbajamt2, nbajam,  tunit_adpcm, nbajamte,  nbajamte, ROT0, "Midway",   "NBA Jam TE (rev 2.0 01-28-94)" )
GAME( 1994, nbajamt3, nbajam,  tunit_adpcm, nbajamte,  nbajamte, ROT0, "Midway",   "NBA Jam TE (rev 3.0 03-04-94)" )
