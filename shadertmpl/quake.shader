//----------------------
// QUAKE
//----------------------

quake1/Base_Template
{
	cull front

	{
		material $1
	}
}

quake1/BaseAnim_Template
{
	cull front

	{
		// base pass
		animmap 3 $1		
	}
	{
		map $lightmap
		blendfunc filter
	}	
}

quake1/Glow_Template
{
	cull front

	{
		// base pass
		material $1
	}
	{
		// glow pass
		map $2
		blendfunc gl_src_alpha gl_one
	}
}

quake1/GlowAnim_Template
{
	cull front

	{
		// base pass
		animmap 3 $1
	}
	{
		map $lightmap
		blendfunc filter
	}	
	{
		// glow pass
		animmap 3 $2
		blendfunc gl_src_alpha gl_one
	}
}

quake1/Warp_Template
{
	cull none

	{
		// map/animmap
		map $1
		tcMod turb 0 .1 0 .05
	}
	{
		map $lightmap
		blendfunc filter
	}	
}

quake1/WarpGlow_Template
{
	cull none

	{
		// map/animmap
		map $1
		tcMod turb 0 .1 0 .05
	}
	{
		// glow pass
		map $2
		tcMod turb 0 .1 0 .05
		blendfunc gl_src_alpha gl_one
	}
}

quake1/Sky_Template
{
	skyParms - 378 -

	{
		map $right $1
		tcmod scale 16 16
		tcmod scroll -0.0625 -0.0625
	}
	{
		map $left $1
		tcmod scale 16 16
		tcmod scroll -0.125 -0.125
		blendfunc blend
	}
}

//----------------------
// QUAKE 2
//----------------------

quake2/Base_Template
{
	cull front

	{
		// base pass
		map $1

		// tcmod/skip
		$2 scroll 0.25 0
	}
	{
		map $lightmap
		blendfunc filter
	}
}

quake2/BaseAnim_Template
{
	cull front

	{
		// base pass
		animmap 3 $1

		// tcmod/skip
		$2 scroll 0.25 0
	}
	{
		map $lightmap
		blendfunc filter
	}
}

quake2/Alpha_Template
{
	cull none

	{
		// base pass
		map $1

		// tcmod/skip
		$2 scroll 0.25 0

		blendfunc blend

		alphagen const 0.33
	}
}

quake2/AlphaAnim_Template
{
	cull none

	{
		// base pass
		animmap 3 $1

		// tcmod/skip
		$2 scroll 0.25 0

		blendfunc blend

		alphagen const 0.33
	}
}

quake2/Warp_Template
{
	cull none

	{
		// base pass
		map $1

		tcMod turb 0 .1 0 .05

		// tcmod/skip
		$2 scroll 0.25 0
	}
}

quake2/AlphaWarp_Template
{
	cull none

	{
		// base pass
		map $1

		tcMod turb 0 .1 0 .05

		// tcmod/skip
		$2 scroll 0.25 0

		blendfunc blend

		alphagen const $3
	}
}

quake2/Skybox_Template
{
	skyParms2 $1 512 -
}
