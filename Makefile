all: debug
.PHONY: debug asan release clean

LUA = ggbuild/lua.linux
NINJA = ggbuild/ninja.linux
ifeq ($(OS),Windows_NT)
	LUA = ggbuild/lua.exe
	NINJA = ggbuild/ninja.exe
endif

debug:
	@$(LUA) make.lua > build.ninja
	@$(NINJA)

asan:
	@$(LUA) make.lua asan > build.ninja
	@$(NINJA)

release:
	@$(LUA) make.lua release > build.ninja
	@$(NINJA)

clean:
	@$(LUA) make.lua debug > build.ninja
	@$(NINJA) -t clean || true
	@$(LUA) make.lua asan > build.ninja
	@$(NINJA) -t clean || true
	@rm -f source/qcommon/gitversion.h
	@rm -rf build release
	@rm -f build.ninja
