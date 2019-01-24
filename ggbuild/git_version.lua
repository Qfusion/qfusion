local function exec( cmd )
	local pipe = assert( io.popen( cmd ) )
	local res = pipe:read( "*line" )
	pipe:close()
	return res or ""
end

local version = exec( "git tag --points-at HEAD" )
if version == "" then
	version = "git-" .. exec( "git rev-parse --short HEAD" )
end

local a, b, c, d = version:match( "^v(%d+)%.(%d+)%.(%d+)%.(%d+)$" )
if not a then
	a = 0
	b = 0
	c = 0
	d = 0
end

local gitversion = ""
	.. "#define APP_VERSION \"" .. version .. "\"\n"
	.. "#define APP_VERSION_A " .. a .. "\n"
	.. "#define APP_VERSION_B " .. b .. "\n"
	.. "#define APP_VERSION_C " .. c .. "\n"
	.. "#define APP_VERSION_D " .. d .. "\n"

local r = io.open( "source/qcommon/gitversion.h", "r" )
local current = r and r:read( "*all" )
if r then
	r:close()
end

if current ~= gitversion then
	local w = assert( io.open( "source/qcommon/gitversion.h", "w" ) )
	w:write( gitversion )
	assert( w:close() )
end
