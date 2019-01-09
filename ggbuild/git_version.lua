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

print( "#define APP_VERSION \"" .. version .. "\"" )
print( "#define APP_VERSION_A " .. a )
print( "#define APP_VERSION_B " .. b )
print( "#define APP_VERSION_C " .. c )
print( "#define APP_VERSION_D " .. d )
